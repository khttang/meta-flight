FILESEXTRAPATHS:prepend := "${THISDIR}/files:"
SRC_URI += "file://flight.cfg \
            file://0001-misc-add-imu-sim-a-simulated-IMU-platform-driver.patch \
            "

# Fail the build if any option in flight.cfg did not reach the final .config
do_flight_cfgcheck() {
    for opt in $(grep -E "^CONFIG_[A-Za-z0-9_]+=" ${UNPACKDIR}/flight.cfg); do
        grep -qx "$opt" ${B}/.config || bbfatal "flight.cfg: $opt is not in the final .config"
    done
}
addtask flight_cfgcheck after do_kernel_configme before do_compile
