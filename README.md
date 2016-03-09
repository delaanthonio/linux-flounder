## What is Sublime-N9?

  Sublime-N9 is a Linux Kernel for the [Nexus 9 android tablet](https://en.wikipedia.org/wiki/Nexus_9). It is based on [ElementalX](https://github.com/flar2/ElementalX-N9). This kernel features a battery friendly CPU frequency governor, Sublime. It scales the CPU frequency less aggressively than the default CPU frequency governors for nexus devices, Interactive, which results in a 20% reduction in battery usage.

## Features
* Sublime CPU frequency governor
* 2.0 Amp charging
* IO Schedulers: Noop, Cfq, Fiops, Sio, Bfq
* Kernel same-page merging
* USB fast-charge
* Double tap to wake
* Sweep to wake
* Sweep to sleep
* Optimized Z-RAM
* CPU  overclocking up to 2.5 Ghz
* GPU overclocking up to 984 Mhz
* Option to disable fsync
* NTFS and exFAT support
* Frandom support

## Getting started

Download the build dependencies:

```bash
$ sudo apt-get install git-core gnupg flex bison gperf build-essential \
zip curl zlib1g-dev gcc-multilib g++-multilib libc6-dev-i386 \
lib32ncurses5-dev x11proto-core-dev libx11-dev lib32z-dev ccache \
libgl1-mesa-dev libxml2-utils xsltproc unzip
```

## Building Instructions

See the [manifest repository](https://github.com/Sublime-Development/android_manifest) for automated building instructions.
 
First, create a project directory
```bash
$ mkdir Sublime-N9 && cd Sublime-N9
  ```
Next, download the preferred toolchain for building the kernel
```bash
$ git clone https://bitbucket.org/UBERTC/aarch64-linux-android-4.9 ubertc-4.9
  ```
After that, dowload the source   
```bash
$ git clone https://github.com/Sublime-Development/Sublime_N9_Manifest.git kernel-flounder
  ```
Next, set the cross compiler prefix
```bash
$ export CROSS_COMPILE=$(pwd)/ubertc-4.9/bin/aach64-linux-android-
  ```
Finally, build the kernel
```bash
$ cd kernel-flounder
$ make sublime_defconfig
$ make -j4
  ```
