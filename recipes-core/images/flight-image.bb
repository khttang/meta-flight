SUMMARY = "Flight computer bench image: core-image-minimal plus CAN, PPS and debug tools"
LICENSE = "MIT"

inherit core-image

IMAGE_INSTALL = "packagegroup-core-boot ${CORE_IMAGE_EXTRA_INSTALL} \
    strace htop iproute2 can-utils pps-tools"
IMAGE_FEATURES += "ssh-server-dropbear"
IMAGE_LINGUAS = ""
