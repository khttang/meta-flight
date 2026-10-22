/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * imu_sim.h: user-space interface of the simulated IMU (/dev/imu0).
 * Shared by the driver and by user programs, so only fixed-width kernel types.
 */
#ifndef _UAPI_IMU_SIM_H
#define _UAPI_IMU_SIM_H

#include <linux/ioctl.h>
#include <linux/types.h>

/* One sample. read() returns whole samples only. */
struct imu_sim_sample {
	__u64 t_ns;		/* CLOCK_MONOTONIC when the sample was taken */
	__u32 seq;		/* +1 per sample; a gap means samples were dropped */
	__s32 accel_mg[3];	/* x, y, z in milli-g */
	__s32 gyro_mdps[3];	/* x, y, z in milli-degrees per second */
	__u32 fault;		/* enum imu_sim_fault active for this sample */
};

enum imu_sim_fault {
	IMU_SIM_FAULT_NONE  = 0,
	IMU_SIM_FAULT_STUCK = 1,	/* values freeze at the last good sample */
	IMU_SIM_FAULT_SPIKE = 2,	/* one sample with +8 g on z, then clears itself */
};

#define IMU_SIM_IOC_MAGIC	'i'
#define IMU_SIM_SET_FAULT	_IOW(IMU_SIM_IOC_MAGIC, 1, __u32)
#define IMU_SIM_GET_RATE	_IOR(IMU_SIM_IOC_MAGIC, 2, __u32)

#endif /* _UAPI_IMU_SIM_H */
