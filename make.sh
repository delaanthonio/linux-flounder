#!/bin/bash

# Stupid shell script to compile kernel, nothing fancy
ID="`pwd`"

# Exports all the needed things Arch, SubArch and Cross Compile
export ARCH=arm64
echo 'exporting Arch'
export SUBARCH=arm64
echo 'exporting SubArch'

# Export toolchain, use android default
export CROSS_COMPILE=/home/prbassplayer/Slim7/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin/aarch64-linux-android-

# Make sure build is clean!
echo 'Cleaning build'
make clean

# Generates a new .config and exists
if [ "$1" = "config" ] ; then
    if [ "$2" = "aosp" ] ; then
        echo 'Making defconfig for flounder aosp'
        make flounder_defconfig
    else
        echo 'Making defconfig for flounder slim'
        make slim_flounder_defconfig
    fi
    exit
fi

# Exports kernel local version? Not sure yet.
#echo 'Exporting kernel version'
#export LOCALVERSION='SlimTest_1.0'

# Lets go!
echo 'Lets start!'
make -j$1
