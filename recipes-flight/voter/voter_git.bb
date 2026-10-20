SUMMARY = "Triple-redundant altimeter voter daemon (lesson C5)"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=093e2c337c3b584aa9624877e607c728"

SRC_URI = "git:///work/src/voter.git;protocol=file;branch=main"
SRCREV = "ba6e623bcdecfde44875ba6e60b9b53d50163bf0"
PV = "1.0+git"

inherit cmake systemd

# The test binary is for the host and for ptest, not for the product image
EXTRA_OECMAKE = "-DBUILD_TESTING=OFF"

SYSTEMD_SERVICE:${PN} = "voterd.service"

do_install:append() {
    install -Dm 0644 ${S}/voterd.service ${D}${systemd_system_unitdir}/voterd.service
}
