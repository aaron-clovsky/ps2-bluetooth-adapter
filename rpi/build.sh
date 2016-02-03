#!/bin/bash

output_log="output.log"
make_log="make.log"

# Get the path to this script
SOURCE="${BASH_SOURCE[0]}"
while [ -h "$SOURCE" ]; do
  DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"
  SOURCE="$(readlink "$SOURCE")"
  [[ $SOURCE != /* ]] && SOURCE="$DIR/$SOURCE"
done
export RPI_BUILDROOT_SCRIPT_DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"

# Function to error check commands (trap allows script to exit from function)
trap "exit 1" TERM
export TOP_PID=$$

run_cmd() {
	fail=0

	eval "$1" >$RPI_BUILDROOT_SCRIPT_DIR/$output_log</dev/null 2>&1||fail=1

	if [ $fail -eq 1 ]; then
		echo "failed"
		echo "$1"
		cat $RPI_BUILDROOT_SCRIPT_DIR/$output_log
		kill -s TERM $TOP_PID
	fi
}

# Record script start time
start_time=$(date -u +"%s")

# Begin script
pushd $RPI_BUILDROOT_SCRIPT_DIR >/dev/null

cd $RPI_BUILDROOT_SCRIPT_DIR

if [ "$1" != "-f" ]; then
  if [ -e ./rpi-buildroot ]; then
    read -p "rpi-buildroot directory exists, delete and continue? (y/n) " RESP
    if [ "$RESP" != "y" ]; then
      exit 0
    fi
  fi
fi

echo -n "Removing old directories..."
run_cmd "rm -rf rpi-buildroot/"
echo "ok"

echo -n "Downloading rpi-buildroot..."
run_cmd "git clone --depth 1 git://github.com/gamaral/rpi-buildroot.git"
echo "ok"

cd $RPI_BUILDROOT_SCRIPT_DIR/rpi-buildroot

echo -n "Setting rpi-buildroot version..."
run_cmd "git reset --hard 12ded0e925f166f5581ff237291ffe505a45f26f"
echo "ok"

cd $RPI_BUILDROOT_SCRIPT_DIR

echo -n "Preparing rootfs skeleton..."
run_cmd "rm -f ./rpi-buildroot/board/raspberrypi/skeleton/etc/init.d/S20urandom"
run_cmd "rm -f ./rpi-buildroot/board/raspberrypi/skeleton/etc/init.d/S40network"
run_cmd "mkdir ./rpi-buildroot/board/raspberrypi/skeleton/boot"
run_cmd "mkdir ./rpi-buildroot/board/raspberrypi/skeleton/var/lib/bluetooth"
run_cmd "mkdir ./rpi-buildroot/board/raspberrypi/skeleton/ps2bt"
echo "ok"

echo -n "Installing device scripts to rootfs skeleton..."
run_cmd "cp ./ps2bt/scripts/S99device ./rpi-buildroot/board/raspberrypi/skeleton/etc/init.d"
run_cmd "cp ./ps2bt/scripts/flush ./rpi-buildroot/board/raspberrypi/skeleton/ps2bt"
run_cmd "cp ./ps2bt/scripts/flushd ./rpi-buildroot/board/raspberrypi/skeleton/ps2bt"
run_cmd "cp ./ps2bt/scripts/ds4pair ./rpi-buildroot/board/raspberrypi/skeleton/ps2bt"
run_cmd "cp ./ps2bt/scripts/connectiond ./rpi-buildroot/board/raspberrypi/skeleton/ps2bt"
echo "ok"

echo -n "Patching Bluez package..."
run_cmd "patch -f ./rpi-buildroot/package/bluez5_utils/bluez5_utils.mk < ./ps2bt/patches/bluez5_utils.mk.patch"
run_cmd "cp ./ps2bt/patches/0003-auto-trust-ds3-and-do-not-set-js0-led.patch ./rpi-buildroot/package/bluez5_utils"
echo "ok"

echo -n "Installing ps2bt package..."
run_cmd "mkdir ./rpi-buildroot/dl"
run_cmd "cp -r ./ps2bt/ps2bt-src ./rpi-buildroot/dl"
run_cmd "cp -r ./ps2bt/ps2bt-package ./rpi-buildroot/package"
run_cmd "mv ./rpi-buildroot/package/ps2bt-package ./rpi-buildroot/package/ps2bt"
run_cmd "patch -f ./rpi-buildroot/package/Config.in < ./ps2bt/patches/rpi-buildroot_package_Config.in.patch"
echo "ok"

cd $RPI_BUILDROOT_SCRIPT_DIR/rpi-buildroot

echo -n "Installing preconfigured .config..."
run_cmd "rm -f .config"
run_cmd "cp ../ps2bt/rpi-buildroot-dot-config .config"
echo "ok"

echo -n "Downloading packages (this will take a very long time)..."
run_cmd "make source"
echo "ok"

cd $RPI_BUILDROOT_SCRIPT_DIR/rpi-buildroot/dl

echo -n "Patching linux kernel..."
run_cmd "tar -xvzf linux-rpi-4.0.y.tar.gz"
run_cmd "patch -f ./linux-rpi-4.0.y/drivers/input/input.c < ../../ps2bt/patches/drivers_input_input.c.patch"
run_cmd "patch -f ./linux-rpi-4.0.y/drivers/hid/hid-sony.c < ../../ps2bt/patches/drivers_hid_hid-sony.c.patch"
run_cmd "rm -f linux-rpi-4.0.y.tar.gz"
run_cmd "tar -zcvf linux-rpi-4.0.y.tar.gz linux-rpi-4.0.y"
run_cmd "rm -rf linux-rpi-4.0.y"
echo "ok"

cd $RPI_BUILDROOT_SCRIPT_DIR/rpi-buildroot

echo -n "Installing preconfigured linux .config..."
run_cmd "mkdir -p ./output/build/linux-rpi-4.0.y"
run_cmd "cp ../ps2bt/rpi-buildroot_output_build_linux-rpi-4.0.y-dot-config ./output/build/linux-rpi-4.0.y/.config"
echo "ok"

echo -n "Running make (this will take a very long time)..."
run_cmd "make"
echo "ok"

cd $RPI_BUILDROOT_SCRIPT_DIR

cp $output_log $make_log

echo -n "Copying files to firmware directory..."
run_cmd "rm -rf firmware"
run_cmd "mkdir -p firmware"
run_cmd "cp ./rpi-buildroot/output/images/zImage ./firmware/"
run_cmd "cp ./rpi-buildroot/output/images/rpi-firmware/bootcode.bin ./firmware/"
run_cmd "cp ./rpi-buildroot/output/images/rpi-firmware/cmdline.txt ./firmware/"
run_cmd "cp ./rpi-buildroot/output/images/rpi-firmware/config.txt ./firmware/"
run_cmd "cp ./rpi-buildroot/output/images/rpi-firmware/fixup.dat ./firmware/"
run_cmd "cp ./rpi-buildroot/output/images/rpi-firmware/start.elf ./firmware/"
run_cmd "cp ./ps2bt/settings.cfg ./firmware/"
echo "ok"

echo -n "Creating settings.dat..."
run_cmd "dd if=/dev/zero of=./firmware/settings.dat bs=1024 count=512"
run_cmd "mkfs.ext4 -F ./firmware/settings.dat"
echo "ok"

popd >/dev/null

echo "Build complete"

# Display script execution time
end_time=$(date -u +"%s")
diff_time=$(($end_time-$start_time))
echo "Build time was $(printf "%02d" $(($diff_time / 3600))):$(printf "%02d" $(($diff_time % 3600 / 60))):$(printf "%02d" $(($diff_time % 60)))"
