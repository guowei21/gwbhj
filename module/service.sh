#!/system/bin/sh
# GWBHJ — Post-boot service

until [ "$(getprop sys.boot_completed)" = "1" ]; do
    sleep 2
done

sleep 5

MODDIR=/data/adb/modules/gwbhj_jailbreak
chmod 644 "$MODDIR/whitelist.txt" 2>/dev/null
chcon u:object_r:system_file:s0 "$MODDIR/whitelist.txt" 2>/dev/null

if [ -f "$MODDIR/serial.txt" ]; then
    chmod 644 "$MODDIR/serial.txt"
    chcon u:object_r:system_file:s0 "$MODDIR/serial.txt" 2>/dev/null
fi
