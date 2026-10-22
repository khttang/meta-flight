// imu-read: read samples from /dev/imu0 and report rate, gaps and the z range.
// usage: imu-read [count [none|stuck|spike]]     default 200 samples, no fault
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "imu_sim.h"

int main(int argc, char **argv)
{
	long want = argc > 1 ? strtol(argv[1], NULL, 10) : 200;
	if (want < 2 || want > 100000) {
		fprintf(stderr, "usage: imu-read [count 2..100000 [none|stuck|spike]]\n");
		return 2;
	}

	int fd = open("/dev/imu0", O_RDONLY);
	if (fd < 0) {
		perror("open /dev/imu0");
		return 1;
	}

	__u32 rate = 0;
	if (ioctl(fd, IMU_SIM_GET_RATE, &rate) < 0) {
		perror("IMU_SIM_GET_RATE");
		return 1;
	}

	if (argc > 2) {
		__u32 fault;
		if (!strcmp(argv[2], "none"))
			fault = IMU_SIM_FAULT_NONE;
		else if (!strcmp(argv[2], "stuck"))
			fault = IMU_SIM_FAULT_STUCK;
		else if (!strcmp(argv[2], "spike"))
			fault = IMU_SIM_FAULT_SPIKE;
		else {
			fprintf(stderr, "unknown fault '%s'\n", argv[2]);
			return 2;
		}
		if (ioctl(fd, IMU_SIM_SET_FAULT, &fault) < 0) {
			perror("IMU_SIM_SET_FAULT");
			return 1;
		}
	}

	struct imu_sim_sample s[16], first = {0}, prev = {0};
	long got = 0, gaps = 0, faulted = 0;
	__s32 zmin = 1 << 30, zmax = -(1 << 30);

	while (got < want) {
		ssize_t n = read(fd, s, sizeof(s));
		if (n < 0) {
			if (errno == EINTR)
				continue;
			perror("read");
			return 1;
		}
		for (size_t i = 0; i < (size_t)n / sizeof(s[0]) && got < want; i++, got++) {
			if (got == 0)
				first = s[i];
			else if (s[i].seq != prev.seq + 1)
				gaps++;
			if (got < 3)
				printf("seq %u t %llu ns accel %d %d %d mg gyro %d %d %d mdps\n",
				       s[i].seq, (unsigned long long)s[i].t_ns,
				       s[i].accel_mg[0], s[i].accel_mg[1], s[i].accel_mg[2],
				       s[i].gyro_mdps[0], s[i].gyro_mdps[1], s[i].gyro_mdps[2]);
			if (s[i].accel_mg[2] < zmin)
				zmin = s[i].accel_mg[2];
			if (s[i].accel_mg[2] > zmax)
				zmax = s[i].accel_mg[2];
			if (s[i].fault)
				faulted++;
			prev = s[i];
		}
	}
	close(fd);

	double secs = (prev.t_ns - first.t_ns) / 1e9;
	printf("%ld samples, rate %u Hz configured, %.1f Hz measured, %ld gaps, z %d..%d mg, %ld faulted\n",
	       got, rate, (got - 1) / secs, gaps, zmin, zmax, faulted);
	return 0;
}
