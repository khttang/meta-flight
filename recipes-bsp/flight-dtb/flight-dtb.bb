SUMMARY = "QEMU virt device tree with the flight bench nodes"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

inherit devicetree

SRC_URI = "file://flight-virt.dts"
COMPATIBLE_MACHINE = "qemuarm64"

# The dts is complete on its own: no kernel include paths, so no wait for the kernel
KERNEL_INCLUDE = ""
