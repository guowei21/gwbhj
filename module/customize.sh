#!/sbin/sh

SKIPUNZIP=0

print_box_start() {
  ui_print "================================="
  ui_print "       过强标黑机       "
  ui_print "================================="
}

check_zygisk() {
  ZYGISK_NEXT_PATH="/data/adb/modules/zygisksu"
  REZYGISK_PATH="/data/adb/modules/rezygisk"

  ui_print " Checking Zygisk..."

  ZN_ACTIVE=false; RZ_ACTIVE=false
  [ -d "$ZYGISK_NEXT_PATH" ] && [ ! -f "$ZYGISK_NEXT_PATH/disable" ] && ZN_ACTIVE=true
  [ -d "$REZYGISK_PATH" ]    && [ ! -f "$REZYGISK_PATH/disable" ]    && RZ_ACTIVE=true

  if [ "$ROOT_SOLUTION" = "Magisk" ]; then
    ZYGISK_STATUS=$(magisk --sqlite "SELECT value FROM settings WHERE key='zygisk';" 2>/dev/null)
    if [ "$ZYGISK_STATUS" = "value=1" ]; then
      ui_print " Magisk: Built-in Zygisk must be OFF!"
      ui_print " Disable it in Magisk Settings first"
      exit 1
    fi
  fi

  if $RZ_ACTIVE; then
    ui_print " ReZygisk: Active"
  elif $ZN_ACTIVE; then
    ui_print " Zygisk Next: Active"
  else
    ui_print " No active Zygisk implementation found!"
    ui_print " Install ReZygisk or Zygisk Next"
    exit 1
  fi
}

detect_root() {
  if command -v apd >/dev/null; then
    ROOT_SOLUTION="APatch"
  elif command -v ksud >/dev/null; then
    ROOT_SOLUTION="KernelSU"
  elif command -v magisk >/dev/null; then
    ROOT_SOLUTION="Magisk"
  else
    ui_print " No root solution detected!"
    exit 1
  fi
  ui_print " Root: $ROOT_SOLUTION"
}

print_box_start

if ! $BOOTMODE; then
  ui_print " Must install via Magisk/KSU/APatch!"
  exit 1
fi

if [ "$API" -lt 26 ]; then
  ui_print " Requires Android 9.0+"
  exit 1
fi

detect_root
check_zygisk

ABI_LIST=$(getprop ro.product.cpu.abilist)
ui_print " ABIs: $ABI_LIST"

KEEP_ARCH=""
if echo "$ABI_LIST" | grep -q "arm64"; then
    KEEP_ARCH="arm64-v8a"
elif echo "$ABI_LIST" | grep -q "x86_64"; then
    KEEP_ARCH="x86_64"
elif echo "$ABI_LIST" | grep -q "armeabi"; then
    KEEP_ARCH="armeabi-v7a"
elif echo "$ABI_LIST" | grep -q "x86"; then
    KEEP_ARCH="x86"
fi

if [ -z "$KEEP_ARCH" ]; then
    ui_print " Unsupported architecture!"
    exit 1
fi

ui_print " Architecture: $KEEP_ARCH"

for arch in arm64-v8a armeabi-v7a x86 x86_64; do
    if [ "$arch" != "$KEEP_ARCH" ]; then
        rm -f "$MODPATH/zygisk/${arch}.so" 2>/dev/null
    fi
done

chmod 0755 "$MODPATH/service.sh" "$MODPATH/action.sh" "$MODPATH/uninstall.sh" 2>/dev/null
chmod 0644 "$MODPATH/whitelist.txt" "$MODPATH/freeze_list.txt" 2>/dev/null

for file in "$MODPATH/whitelist.txt" "$MODPATH/freeze_list.txt" "$MODPATH/service.sh" "$MODPATH/action.sh"; do
    if [ -f "$file" ]; then
        chcon u:object_r:system_file:s0 "$file" 2>/dev/null
    fi
done

if [ ! -f "$MODPATH/serial.txt" ]; then
    cat /dev/urandom | tr -dc 'A-Za-z0-9' | head -c 16 > "$MODPATH/serial.txt" 2>/dev/null
    chmod 644 "$MODPATH/serial.txt" 2>/dev/null
fi

set_permissions() {
    chmod 0755 "$MODPATH" 2>/dev/null
    chmod 0755 "$MODPATH/service.sh" "$MODPATH/action.sh" "$MODPATH/uninstall.sh" 2>/dev/null
    chmod 0755 "$MODPATH/zygisk" 2>/dev/null
    chmod 0644 "$MODPATH/zygisk"/*.so 2>/dev/null
    chmod 0644 "$MODPATH/module.prop" "$MODPATH/whitelist.txt" "$MODPATH/freeze_list.txt" "$MODPATH/serial.txt" 2>/dev/null
    chcon u:object_r:system_file:s0 "$MODPATH/zygisk" "$MODPATH/zygisk"/*.so 2>/dev/null
}

ui_print "================================="
ui_print "    过强标黑机 Installed!    "
ui_print "================================="
ui_print " Performing soft reboot..."

sleep 2
if command -v ksud >/dev/null 2>&1; then
    /data/adb/ksud soft-reboot
elif command -v magisk >/dev/null 2>&1; then
    setprop sys.powerctl reboot,soft
else
    setprop sys.powerctl reboot,soft
fi
