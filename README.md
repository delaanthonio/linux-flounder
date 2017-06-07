## What is Sublime-N9?

Sublime-N9 is a Linux kernel for
the [Nexus 9 Android tablet](https://en.wikipedia.org/wiki/Nexus_9)
based on [ElementalX](https://github.com/flar2/ElementalX-N9) kernel. Sublime-N9
features a battery-friendly CPU frequency governor—Sublime Active—which scales
the CPU frequency less aggressively than the default CPU frequency governor
(Interactive) for Nexus devices, which results in a 20% reduction in battery
usage.

## Features
* Sublime Active CPU frequency governor
* Touch CPU quiet governor
* 2.0 Amp charging
* IO Schedulers: noop, cfq, fiops, sio, bfq
* USB fast-charge
* Double tap to wake
* Sweep to wake
* Sweep to sleep
* CPU overclocking up to 2.5 Mhz
* GPU overclocking up to 984 Mhz
* NTFS and exFAT support

## Building Instructions

Download the build dependencies:
```bash
$ sudo apt-get install git-core gnupg flex bison gperf build-essential \
zip curl zlib1g-dev gcc-multilib g++-multilib libc6-dev-i386 \
lib32ncurses5-dev x11proto-core-dev libx11-dev lib32z-dev ccache \
libgl1-mesa-dev libxml2-utils xsltproc unzip
```
Create a project directory
```bash
$ mkdir sublime && cd sublime
  ```
Clone a toolchain for building the kernel
```bash
$ git clone https://bitbucket.org/UBERTC/aarch64-linux-android-4.9-kernel.git ubertc-4.9
  ```
Clone the source
```bash
$ git clone https://github.com/Beta1440/sublime-n9.git sublime-n9
  ```
Set the cross compiler prefix
```bash
$ export CROSS_COMPILE=$(pwd)/ubertc-4.9/bin/aarch64-linux-android-
  ```
Build the kernel
```bash
$ cd sublime-n9
$ make sublime_defconfig
$ make -j4
  ```
