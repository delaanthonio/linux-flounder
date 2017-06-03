#!/sbin/sh

# Battery patch
ui_print "patchsys: applying battery patch"
cp -f "/tmp/healthd_7.1.1" "/sbin/healthd"
chmod 0751 "/sbin/healthd"
