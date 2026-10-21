SUMMARY = "Triple-redundant altimeter voter in Rust: no_std core and a Linux daemon (lesson Y3r)"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=093e2c337c3b584aa9624877e607c728"

SRC_URI = "git:///work/src/voter-rs.git;protocol=file;branch=main"
SRCREV = "d770ab3efd3071fcccb3402869b795173244fdd8"
PV = "1.0+git"

inherit cargo cargo-update-recipe-crates systemd

require ${BPN}-crates.inc

SYSTEMD_SERVICE:${PN} = "voterd-rs.service"

do_install:append() {
    install -Dm 0644 ${S}/voterd-rs.service ${D}${systemd_system_unitdir}/voterd-rs.service
}
