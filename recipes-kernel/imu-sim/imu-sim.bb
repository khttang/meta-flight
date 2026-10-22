SUMMARY = "Simulated 6-axis IMU: out-of-tree platform driver for the flight,imu-sim node (lesson Y4)"
LICENSE = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://COPYING;md5=12f884d2ae1ff87c09e5b7ccc2c4ca7e"

inherit module

SRC_URI = "file://Makefile \
           file://imu-sim.c \
           file://imu_sim.h \
           file://COPYING \
          "
S = "${UNPACKDIR}"
PV = "1.0"

# The module class names the package kernel-module-imu-sim-<kernel version>
RPROVIDES:${PN} += "kernel-module-imu-sim"
