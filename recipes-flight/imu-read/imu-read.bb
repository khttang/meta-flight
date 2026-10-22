SUMMARY = "Reads /dev/imu0 and reports rate, gaps and faults (lesson Y4)"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

# imu_sim.h is a copy of the driver's user-space header. A uapi header is a stable
# interface, so copying it is normal: linux-libc-headers does the same for the kernel's.
SRC_URI = "file://imu-read.c file://imu_sim.h"
S = "${UNPACKDIR}"
PV = "1.0"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} -Wall -Wextra -o imu-read imu-read.c
}

do_install() {
    install -Dm 0755 imu-read ${D}${bindir}/imu-read
}
