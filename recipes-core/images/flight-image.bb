SUMMARY = "Flight computer bench image: core-image-minimal plus CAN, PPS and debug tools"
LICENSE = "MIT"

inherit core-image

IMAGE_INSTALL = "packagegroup-core-boot ${CORE_IMAGE_EXTRA_INSTALL} \
    strace htop iproute2 can-utils pps-tools voter voter-rs imu-sim imu-read"
IMAGE_FEATURES += "ssh-server-dropbear"
IMAGE_LINGUAS = ""

# QEMU only: boot with our device tree instead of the one QEMU generates
EXTRA_IMAGEDEPENDS:append:qemuarm64 = " flight-dtb:do_deploy"
QB_DTB:qemuarm64 = "devicetree/flight-virt.dtb"
