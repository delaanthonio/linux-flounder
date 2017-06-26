## Linux Flounder

This is my customized linux kernel for
the [Nexus 9 Android tablet](https://en.wikipedia.org/wiki/Nexus_9)
based on [ElementalX](https://github.com/flar2/ElementalX-N9) kernel. This kernel
features a battery-friendly CPU frequency governor—Sublime Active—which scales
the CPU frequency less aggressively than the default CPU frequency governor
(Interactive) for Nexus devices, which results in a 20% reduction in battery
usage.

## Features
* Sublime Active CPU frequency governor
* Touch CPU quiet governor
* Optimized ZRAM
* 2.0 Amp charging
* IO Schedulers: noop, fiops, sio, bfq
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
$ sudo apt-get install build-essential zip 

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
$ git clone https://github.com/beta1440/linux-flounder.git
  ```
Set the cross compiler prefix
```bash
$ export CROSS_COMPILE=$(pwd)/ubertc-4.9/bin/aarch64-linux-android-
  ```
  Additionally, you may want to add the bin folder of the gcc cross
  compiler to your path.
Build the kernel
```bash
$ cd linux-flounder
$ make prepare
$ make sublime_defconfig
$ make otapackage -j4
  ```
  The OTA package should be inside the android directory.
  
  To change the directory of the created OTA package, set the
  environment variable `KBUILD_FLOUNDER_OUTPUT` to the absolute path
  of your preferred output directory.
