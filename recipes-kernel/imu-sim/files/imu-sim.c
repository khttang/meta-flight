// SPDX-License-Identifier: GPL-2.0
/*
 * imu-sim: a simulated 6-axis IMU as a platform driver.
 *
 * Binds to the device-tree node  compatible = "flight,imu-sim"  and reads its
 * sample-rate-hz property. While /dev/imu0 is open, an hrtimer makes one sample
 * per period into a 64-sample FIFO; read() blocks until samples are there.
 * No floating point: the kernel doesn't save FPU state for kernel code.
 */
#include <linux/hrtimer.h>
#include <linux/kfifo.h>
#include <linux/miscdevice.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/property.h>
#include <linux/uaccess.h>

#include "imu_sim.h"

#define IMU_SIM_FIFO_LEN	64	/* samples; a power of two for kfifo */
#define IMU_SIM_READ_BATCH	8	/* samples copied per pass in read() */

struct imu_sim {
	struct device *dev;
	struct miscdevice misc;
	struct hrtimer timer;
	ktime_t period;
	u32 rate_hz;

	spinlock_t lock;	/* protects everything below; taken in the timer */
	DECLARE_KFIFO(fifo, struct imu_sim_sample, IMU_SIM_FIFO_LEN);
	struct imu_sim_sample last;
	u32 seq;
	u32 fault;
	u64 samples;
	u64 overruns;

	wait_queue_head_t wq;
	atomic_t open;		/* one reader at a time */
};

/* A triangle wave from -amp to +amp over `period` samples: integer only. */
static s32 tri(u32 n, u32 period, s32 amp)
{
	u32 p = n % period;
	u32 half = period / 2;
	s32 up = (s32)(p < half ? p : period - p);

	return (up * 4 * amp) / (s32)period - amp;
}

static void imu_sim_make(struct imu_sim *s, struct imu_sim_sample *out)
{
	u32 n = s->seq;

	if (s->fault == IMU_SIM_FAULT_STUCK) {
		*out = s->last;
	} else {
		out->accel_mg[0] = tri(n, s->rate_hz, 50);
		out->accel_mg[1] = tri(n + s->rate_hz / 4, s->rate_hz, 50);
		out->accel_mg[2] = 1000 + tri(n, 2 * s->rate_hz, 20);
		out->gyro_mdps[0] = tri(n, 4 * s->rate_hz, 2000);
		out->gyro_mdps[1] = 0;
		out->gyro_mdps[2] = tri(n, 8 * s->rate_hz, 500);
		if (s->fault == IMU_SIM_FAULT_SPIKE)
			out->accel_mg[2] += 8000;
	}
	out->t_ns = ktime_get_ns();
	out->seq = s->seq++;
	out->fault = s->fault;

	if (s->fault == IMU_SIM_FAULT_SPIKE)
		s->fault = IMU_SIM_FAULT_NONE;
	else if (s->fault == IMU_SIM_FAULT_NONE)
		s->last = *out;
}

/* hrtimer callback: hard-interrupt context. No sleeping, no copy_to_user. */
static enum hrtimer_restart imu_sim_tick(struct hrtimer *t)
{
	struct imu_sim *s = container_of(t, struct imu_sim, timer);
	struct imu_sim_sample smp;
	unsigned long flags;

	/*
	 * Under s->lock (spin_lock_irqsave):
	 *  - imu_sim_make(s, &smp)
	 *  - if the FIFO is full, drop the OLDEST sample (kfifo_skip) and count it
	 *    in s->overruns: a flight reader wants the newest data, not stale data
	 *  - kfifo_put the new sample; s->samples++
	 * Then, after unlocking, wake any reader sleeping on s->wq.
	 */
	spin_lock_irqsave(&s->lock, flags);
	imu_sim_make(s, &smp);
	if (kfifo_is_full(&s->fifo)) {
		kfifo_skip(&s->fifo);
		++s->overruns;
	}
	kfifo_put(&s->fifo, smp);
	++s->samples;
	spin_unlock_irqrestore(&s->lock, flags);
	wake_up_interruptible(&s->wq);
	hrtimer_forward_now(t, s->period);
	return HRTIMER_RESTART;
}

static struct imu_sim *file_to_imu(struct file *f)
{
	/* misc_open() sets private_data to our struct miscdevice */
	return container_of(f->private_data, struct imu_sim, misc);
}

static int imu_sim_open(struct inode *inode, struct file *f)
{
	struct imu_sim *s = file_to_imu(f);
	unsigned long flags;

	if (atomic_cmpxchg(&s->open, 0, 1))
		return -EBUSY;

	spin_lock_irqsave(&s->lock, flags);
	kfifo_reset(&s->fifo);
	spin_unlock_irqrestore(&s->lock, flags);

	hrtimer_start(&s->timer, s->period, HRTIMER_MODE_REL);
	return 0;
}

static int imu_sim_release(struct inode *inode, struct file *f)
{
	struct imu_sim *s = file_to_imu(f);

	hrtimer_cancel(&s->timer);
	atomic_set(&s->open, 0);
	return 0;
}

static ssize_t imu_sim_read(struct file *f, char __user *buf, size_t count, loff_t *ppos)
{
	struct imu_sim *s = file_to_imu(f);
	struct imu_sim_sample batch[IMU_SIM_READ_BATCH];
	size_t want = count / sizeof(batch[0]);
	size_t done = 0;
	unsigned int n;
	int ret;

	if (want == 0)
		return -EINVAL;
	while (kfifo_is_empty(&s->fifo) && done == 0) {
		if (f->f_flags & O_NONBLOCK)
			return -EAGAIN;
		ret = wait_event_interruptible(s->wq, !kfifo_is_empty(&s->fifo));
		if (ret != 0)
			return ret;
	}

	while (done < want) {
		spin_lock_irq(&s->lock);
		n = kfifo_out(&s->fifo, batch, IMU_SIM_READ_BATCH);
		spin_unlock_irq(&s->lock);
		if (n == 0)
			break;
		ret = copy_to_user(buf + done * sizeof(batch[0]), batch, n * sizeof(batch[0]));
		if (ret != 0) {
			if (done == 0)
				return -EFAULT;
			break;
		}
		done += n;
	}

	/*
	 *  1. want == 0 (buffer smaller than one sample): return -EINVAL.
	 *  2. While the FIFO is empty: with O_NONBLOCK in f->f_flags return -EAGAIN,
	 *     otherwise sleep with wait_event_interruptible() on s->wq; if it returns
	 *     non-zero (a signal), return that value.
	 *  3. Loop until `want` samples are done or the FIFO is empty: take up to
	 *     IMU_SIM_READ_BATCH samples with kfifo_out() under spin_lock_irq(), unlock,
	 *     THEN copy_to_user() them (it can sleep: never under a spinlock).
	 *     A failed copy returns -EFAULT.
	 *  4. Return the number of BYTES copied.
	 */
	return done * sizeof(batch[0]);
}

static __poll_t imu_sim_poll(struct file *f, struct poll_table_struct *wait)
{
	struct imu_sim *s = file_to_imu(f);

	poll_wait(f, &s->wq, wait);
	return kfifo_is_empty(&s->fifo) ? 0 : EPOLLIN | EPOLLRDNORM;
}

static long imu_sim_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	struct imu_sim *s = file_to_imu(f);
	u32 __user *uarg = (u32 __user *)arg;
	u32 v;

	switch (cmd) {
	case IMU_SIM_SET_FAULT:
		if (get_user(v, uarg))
			return -EFAULT;
		if (v > IMU_SIM_FAULT_SPIKE)
			return -EINVAL;
		spin_lock_irq(&s->lock);
		s->fault = v;
		spin_unlock_irq(&s->lock);
		dev_info(s->dev, "fault set to %u\n", v);
		/*
		 * TODO A3 (about 8 lines): get_user() the value (-EFAULT on failure),
		 * reject anything above IMU_SIM_FAULT_SPIKE with -EINVAL, store it in
		 * s->fault under the lock, dev_info() "fault set to %u", return 0.
		 */
		return 0;
	case IMU_SIM_GET_RATE:
		return put_user(s->rate_hz, uarg);
	default:
		return -ENOTTY;
	}
}

static const struct file_operations imu_sim_fops = {
	.owner		= THIS_MODULE,
	.open		= imu_sim_open,
	.release	= imu_sim_release,
	.read		= imu_sim_read,
	.poll		= imu_sim_poll,
	.unlocked_ioctl	= imu_sim_ioctl,
	.llseek		= noop_llseek,
};

/* sysfs: /sys/bus/platform/devices/flight-imu/{samples,overruns} */
static ssize_t samples_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct imu_sim *s = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%llu\n", s->samples);
}
static DEVICE_ATTR_RO(samples);

static ssize_t overruns_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct imu_sim *s = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%llu\n", s->overruns);
}
static DEVICE_ATTR_RO(overruns);

static struct attribute *imu_sim_attrs[] = {
	&dev_attr_samples.attr,
	&dev_attr_overruns.attr,
	NULL,
};
ATTRIBUTE_GROUPS(imu_sim);

static int imu_sim_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct imu_sim *s;
	int ret;

	s = devm_kzalloc(dev, sizeof(*s), GFP_KERNEL);
	if (!s)
		return -ENOMEM;
	s->dev = dev;

	/*
	 * TODO A4 (about 5 lines): default s->rate_hz to 100, then read the
	 * "sample-rate-hz" property with device_property_read_u32(). Outside
	 * 1..1000, fail the probe: return dev_err_probe(dev, -EINVAL, "...").
	 */
	s->rate_hz = 100;
	device_property_read_u32(dev, "sample-rate-hz", &s->rate_hz);
	if (s->rate_hz < 1 || s->rate_hz > 1000)
		return dev_err_probe(dev, -EINVAL, "sample-rate-hz %u out of range 1..1000\n",
				     s->rate_hz);
	s->period = ns_to_ktime(NSEC_PER_SEC / s->rate_hz);

	spin_lock_init(&s->lock);
	INIT_KFIFO(s->fifo);
	init_waitqueue_head(&s->wq);
	atomic_set(&s->open, 0);
	hrtimer_setup(&s->timer, imu_sim_tick, CLOCK_MONOTONIC, HRTIMER_MODE_REL);

	s->misc.minor = MISC_DYNAMIC_MINOR;
	s->misc.name = "imu0";
	s->misc.fops = &imu_sim_fops;
	s->misc.parent = dev;
	platform_set_drvdata(pdev, s);

	ret = misc_register(&s->misc);
	if (ret)
		return dev_err_probe(dev, ret, "misc_register failed\n");

	dev_info(dev, "simulated IMU at %u Hz on /dev/%s\n", s->rate_hz, s->misc.name);
	return 0;
}

static void imu_sim_remove(struct platform_device *pdev)
{
	struct imu_sim *s = platform_get_drvdata(pdev);

	misc_deregister(&s->misc);
	hrtimer_cancel(&s->timer);
}

static const struct of_device_id imu_sim_of_match[] = {
	{ .compatible = "flight,imu-sim" },
	{ }
};
MODULE_DEVICE_TABLE(of, imu_sim_of_match);

static struct platform_driver imu_sim_driver = {
	.probe	= imu_sim_probe,
	.remove	= imu_sim_remove,
	.driver	= {
		.name		= "imu-sim",
		.of_match_table	= imu_sim_of_match,
		.dev_groups	= imu_sim_groups,
		/*
		 * No sysfs bind/unbind: unbind with /dev/imu0 open frees the
		 * devm state under the open file (use-after-free). The device
		 * is fixed in the DT, and .owner already blocks rmmod.
		 */
		.suppress_bind_attrs = true,
	},
};
module_platform_driver(imu_sim_driver);

MODULE_DESCRIPTION("Simulated 6-axis IMU for the flight bench (lesson Y4)");
MODULE_AUTHOR("Khiem Tang");
MODULE_LICENSE("GPL");
