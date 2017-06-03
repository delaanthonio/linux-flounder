#!/sbin/sh

# Audio patch
ui_print "patchsys: applying audio patch"
mount /vendor
cp -f "/tmp/rt5677_elf_vad" "/vendor/firmware/rt5677_elf_vad"
chmod 0644 "/vendor/firmware/rt5677_elf_vad"

# Battery patch
ui_print "patchsys: applying battery patch"
cp -f "/tmp/healthd_7.1.1" "/sbin/healthd"
chmod 0751 "/sbin/healthd"
