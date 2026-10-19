SUMMARY = "QEMU virt device tree with the flight bench nodes"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

inherit devicetree

# Base: QEMU dump, seeds removed, label gpio0 added. Overlay: our hardware.
SRC_URI = "file://flight-virt-base.dts file://flight-bench.dtso"
COMPATIBLE_MACHINE = "qemuarm64"

# The dts is complete on its own: no kernel include paths, so no wait for the kernel
KERNEL_INCLUDE = ""

# QEMU loads one .dtb and cannot apply overlays, so merge at build time.
# On a board with U-Boot you would deploy the .dtbo and apply it at boot instead.
do_merge_overlay() {
    fdtoverlay -i flight-virt-base.dtb -o flight-virt.dtb flight-bench.dtbo
}
do_merge_overlay[dirs] = "${B}"
addtask merge_overlay after do_compile before do_install
