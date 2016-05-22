#!/sbin/sh
#     Copyright (C) 2016 Dela Anthonio <dell.anthonio@gmail.com>
#
#     This program is free software: you can redistribute it and/or modify
#     it under the terms of the GNU General Public License as published by
#     the Free Software Foundation, either version 3 of the License, or
#     (at your option) any later version.
#
#     This program is distributed in the hope that it will be useful,
#     but WITHOUT ANY WARRANTY; without even the implied warranty of
#     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#     GNU General Public License for more details.
#
#     You should have received a copy of the GNU General Public License
#     along with this program.  If not, see <http://www.gnu.org/licenses/>.

mkdir /tmp/ramdisk
cp /tmp/boot.img-ramdisk.gz /tmp/ramdisk/
cd /tmp/ramdisk/
gunzip -c /tmp/ramdisk/boot.img-ramdisk.gz | cpio -i
rm /tmp/ramdisk/boot.img-ramdisk.gz
rm /tmp/boot.img-ramdisk.gz

#Don't force encryption
if  grep -qr forceencrypt /tmp/ramdisk/fstab.flounder; then
   sed -i "s/forceencrypt/encryptable/" /tmp/ramdisk/fstab.flounder
fi
if  grep -qr forceencrypt /tmp/ramdisk/fstab.flounder64; then
   sed -i "s/forceencrypt/encryptable/" /tmp/ramdisk/fstab.flounder64
fi

#Disable dm_verity
if  grep -qr verify=/dev/block/platform/sdhci-tegra.3/by-name/MD1 /tmp/ramdisk/fstab.flounder; then
   sed -i "s/\,verify\=\/dev\/block\/platform\/sdhci\-tegra\.3\/by\-name\/MD1//" /tmp/ramdisk/fstab.flounder
fi
if  grep -qr verify=/dev/block/platform/sdhci-tegra.3/by-name/MD1 /tmp/ramdisk/fstab.flounder64; then
   sed -i "s/\,verify\=\/dev\/block\/platform\/sdhci\-tegra\.3\/by\-name\/MD1//" /tmp/ramdisk/fstab.flounder64
fi
rm /tmp/ramdisk/verity_key

#F2FS on /data
if  ! grep -q '/data.*f2fs' /tmp/ramdisk/fstab.flounder; then
   sed -i 's@.*by-name/UDA.*@//dev/block/platform/sdhci-tegra.3/by-name/UDA /data f2fs noatime,noacl,errors=recover wait,check,encryptable=/dev/block/platform/sdhci-tegra.3/by-name/MD1\n&@' /tmp/ramdisk/fstab.flounder
fi
if  ! grep -q '/data.*f2fs' /tmp/ramdisk/fstab.flounder64; then
   sed -i 's@.*by-name/UDA.*@//dev/block/platform/sdhci-tegra.3/by-name/UDA /data f2fs noatime,noacl,errors=recover wait,check,encryptable=/dev/block/platform/sdhci-tegra.3/by-name/MD1\n&@' /tmp/ramdisk/fstab.flounder64
fi

#F2FS on /cache
if  ! grep -q '/cache.*f2fs' /tmp/ramdisk/fstab.flounder; then
   sed -i 's@.*by-name/CAC.*@/dev/block/platform/sdhci-tegra.3/by-name/CAC        /cache          f2fs    rw,noatime,nosuid,nodev,discard,nodiratime,inline_data,active_logs=4 wait,check\n&@' /tmp/ramdisk/fstab.flounder
fi
if  ! grep -q '/cache.*f2fs' /tmp/ramdisk/fstab.flounder64; then
   sed -i 's@.*by-name/CAC.*@/dev/block/platform/sdhci-tegra.3/by-name/CAC        /cache          f2fs    rw,noatime,nosuid,nodev,discard,nodiratime,inline_data,active_logs=4 wait,check\n&@' /tmp/ramdisk/fstab.flounder64
fi

#Start sublime script
if [ $(grep -c "import /init.sublime.rc" /tmp/ramdisk/init.rc) == 0 ]; then
   sed -i "/import \/init\.trace\.rc/aimport /init.sublime.rc" /tmp/ramdisk/init.rc
fi

#copy sublime scripts
cp /tmp/sublime.sh /tmp/ramdisk/sbin/sublime.sh
chmod 755 /tmp/ramdisk/sbin/sublime.sh
cp /tmp/init.sublime.rc /tmp/ramdisk/init.sublime.rc


#Start zram
#if [ $(grep -c "max_comp_streams" /tmp/ramdisk/init.flounder.rc) == 0 ]; then
#   sed -i "/Turn on swap/a    write /sys/block/zram0/max_comp_streams 2" /tmp/ramdisk/init.flounder.rc
#   sed -i "/max_comp_streams/a    write /sys/block/zram0/comp_algorithm lz4" /tmp/ramdisk/init.flounder.rc
#fi
#if [ $(grep -c "max_comp_streams" /tmp/ramdisk/init.flounder64.rc) == 0 ]; then
#   sed -i "/Turn on swap/a\ \ \ \ write /sys/block/zram0/max_comp_streams 2" /tmp/ramdisk/init.flounder64.rc
#   sed -i "/max_comp_streams/a\ \ \ \ write /sys/block/zram0/comp_algorithm lz4" /tmp/ramdisk/init.flounder64.rc
#fi


find . | cpio -o -H newc | gzip > /tmp/boot.img-ramdisk.gz
rm -r /tmp/ramdisk
