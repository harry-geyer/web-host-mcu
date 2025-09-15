#/bin/bash

FW=$1

if [[ $# -lt 1 ]]; then
    echo "No firmware given" >&2
    exit -1
fi

if [[ ! -e "${FW}" ]]; then
    echo "Firmware file does not exist" >&2
    exit -1
fi

netcat -z localhost 4444

if [[ ! $? -eq "0" ]]; then
    lsusb -d 2e8a:0003 > /dev/null
    if [[ $? -eq "0" ]]; then
        sudo picotool load ${FW}
        sudo picotool reboot
    else
        openocd -f interface/cmsis-dap.cfg -c "adapter speed 5000" -f target/rp2040.cfg -s tcl -c "init" -c "dap info" -c "shutdown" >> /dev/null 2>&1
        if [[ $? -eq "0" ]]; then
            openocd -f interface/cmsis-dap.cfg -c "adapter speed 5000" -f target/rp2040.cfg -s tcl -c "program ${FW} verify reset exit" >> /dev/null 2>&1
        else
            echo "Programming failed: RP2040 is not in BOOTSEL mode" >&2
            exit -1
        fi
    fi
else
    echo "program ${FW} verify reset" | telnet 127.0.0.1 4444 >> /dev/null 2>&1 | true
fi

exit 0
