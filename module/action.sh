#!/system/bin/sh
# GWBHJ — Interactive menu with volume keys
# Usage: sh /data/adb/modules/gwbhj_jailbreak/action.sh

MODULE_DIR="/data/adb/modules/gwbhj_jailbreak"
SERIAL_FILE="$MODULE_DIR/serial.txt"
ANDROID_ID_FILE="$MODULE_DIR/android_id.txt"
FREEZE_LIST="$MODULE_DIR/freeze_list.txt"

wait_key() {
    while true; do
        local ev
        ev=$(timeout 60 getevent -lq 2>/dev/null | grep -m1 "KEY_VOLUME")
        if [ -n "$ev" ]; then
            case "$ev" in
                *VOLUMEUP*) echo "up"; return ;;
                *VOLUMEDOWN*) echo "down"; return ;;
            esac
        fi
    done
}

reset_serial() {
    local serial
    serial=$(cat /dev/urandom | tr -dc 'A-Za-z0-9' | head -c 16 2>/dev/null)
    if [ -z "$serial" ]; then
        serial="HW$(date +%s%N | md5sum | head -c 14)"
    fi
    echo "$serial" > "$SERIAL_FILE"
    chmod 644 "$SERIAL_FILE"
    chcon u:object_r:system_file:s0 "$SERIAL_FILE" 2>/dev/null
    echo ""
    echo "  序列号已重置为: $serial"
    echo ""
    sleep 1
}

reset_android_id() {
    local aid
    aid=$(cat /dev/urandom | tr -dc '0-9' | head -c 19 2>/dev/null)
    if [ -z "$aid" ]; then
        aid="$(date +%s%N | head -c 19)"
    fi
    echo "$aid" > "$ANDROID_ID_FILE"
    chmod 644 "$ANDROID_ID_FILE"
    chcon u:object_r:system_file:s0 "$ANDROID_ID_FILE" 2>/dev/null
    echo ""
    echo "  ANDROID_ID 已重置为: $aid"
    echo ""
    sleep 1
}

jailbreak() {
    echo ""
    echo "  正在应用越狱环境..."
    echo ""

    echo 0 > /proc/sys/kernel/kptr_restrict
    echo 0 > /proc/sys/kernel/dmesg_restrict
    echo 0 > /proc/sys/fs/suid_dumpable
    echo 0 > /proc/sys/kernel/core_pattern
    restorecon /dev/__properties__/u:object_r:adbd_config_prop:s0 2>/dev/null
    restorecon /dev/__properties__/u:object_r:shell_prop:s0 2>/dev/null

    echo "  内核参数已修改"
    echo "  正在回收 PID..."

    local last_pid
    last_pid=$(sh -c 'echo $PPID')
    local wrapped=0
    while true; do
        : &
        local current_pid=$!
        if [ "$current_pid" -lt "$last_pid" ]; then
            wrapped=1
        fi
        if [ "$wrapped" -eq 1 ] && [ "$current_pid" -ge 1700 ]; then
            break
        fi
        last_pid=$current_pid
    done

    echo "  PID 回收完成"
    echo "  正在软重启..."
    sleep 1

    if command -v ksud >/dev/null 2>&1; then
        /data/adb/ksud soft-reboot
    elif command -v magisk >/dev/null 2>&1; then
        setprop sys.powerctl reboot,soft
    else
        setprop sys.powerctl reboot,soft
    fi
}

freeze_apps() {
    if [ ! -f "$FREEZE_LIST" ]; then
        echo ""
        echo "  错误: 冻结列表不存在"
        echo "  路径: $FREEZE_LIST"
        echo ""
        sleep 2
        return
    fi

    echo ""
    echo "  正在获取用户列表..."
    echo ""

    rm -f /tmp/gwbhj_users.txt
    pm list users 2>/dev/null | while IFS= read -r line; do
        local uid
        uid=$(echo "$line" | sed 's/.*UserInfo{//' | sed 's/:.*//' | head -1)
        case "$uid" in
            ''|*[!0-9]*) continue ;;
        esac
        if [ "$uid" = "0" ]; then continue; fi
        local uname
        uname=$(echo "$line" | sed 's/.*name=//' | sed 's/[,}].*//' | head -1)
        if [ -z "$uname" ]; then
            uname="用户$uid"
        fi
        echo "$uid|$uname"
    done > /tmp/gwbhj_users.txt

    local user_count
    user_count=$(wc -l < /tmp/gwbhj_users.txt 2>/dev/null)
    if [ -z "$user_count" ] || [ "$user_count" -eq 0 ]; then
        echo "  未检测到非主用户（多开用户）"
        echo "  请确认已创建多开空间"
        echo ""
        sleep 2
        rm -f /tmp/gwbhj_users.txt
        return
    fi

    local sel=0
    while true; do
        echo "================================"
        echo "    选择目标用户"
        echo "================================"
        echo ""
        local idx=0
        while IFS='|' read -r uid uname; do
            if [ "$idx" -eq "$sel" ]; then
                echo "  > 用户$uid ($uname)"
            else
                echo "    用户$uid ($uname)"
            fi
            idx=$((idx + 1))
        done < /tmp/gwbhj_users.txt
        echo ""
        echo "  音量下：切换"
        echo "  音量上：确认"
        echo "================================"

        local key
        key=$(wait_key)
        if [ "$key" = "down" ]; then
            sel=$(( (sel + 1) % user_count ))
        elif [ "$key" = "up" ]; then
            break
        fi
    done

    local target_uid=""
    local target_name=""
    local idx=0
    while IFS='|' read -r uid uname; do
        if [ "$idx" -eq "$sel" ]; then
            target_uid="$uid"
            target_name="$uname"
            break
        fi
        idx=$((idx + 1))
    done < /tmp/gwbhj_users.txt
    rm -f /tmp/gwbhj_users.txt

    if [ -z "$target_uid" ] || [ "$target_uid" = "0" ]; then
        echo ""
        echo "  安全拦截: 目标用户为主用户(id=0)，已阻止操作"
        echo ""
        sleep 2
        return
    fi

    echo ""
    echo "  目标: 用户$target_uid ($target_name)"
    echo "  正在扫描冻结状态..."
    echo ""

    local installed_list
    installed_list=$(pm list packages --user "$target_uid" 2>/dev/null | sed 's/package://')
    local disabled_list
    disabled_list=$(pm list packages -d --user "$target_uid" 2>/dev/null | sed 's/package://')

    local should_freeze=0
    local already_frozen=0
    local not_installed=0
    local unfrozen_list=""

    while IFS= read -r pkg; do
        [ -z "$pkg" ] && continue
        if ! echo "$installed_list" | grep -q "^${pkg}$"; then
            not_installed=$((not_installed + 1))
            continue
        fi
        should_freeze=$((should_freeze + 1))
        if echo "$disabled_list" | grep -q "^${pkg}$"; then
            already_frozen=$((already_frozen + 1))
        else
            if [ -z "$unfrozen_list" ]; then
                unfrozen_list="$pkg"
            else
                unfrozen_list="$unfrozen_list $pkg"
            fi
        fi
    done < "$FREEZE_LIST"

    local not_frozen=$((should_freeze - already_frozen))

    echo "================================"
    echo "    冻结状态报告"
    echo "================================"
    echo ""
    echo "  目标用户: 用户$target_uid ($target_name)"
    echo "  列表中未安装: $not_installed 个（已跳过）"
    echo "  应冻结: $should_freeze 个"
    echo "  已冻结: $already_frozen 个"
    echo "  应冻未冻: $not_frozen 个"
    echo ""

    if [ "$not_frozen" -eq 0 ]; then
        echo "  所有目标均已冻结，无需操作"
        echo ""
        sleep 2
        return
    fi

    local confirm=0
    while true; do
        echo "  继续冻结应冻未冻结的 $not_frozen 个应用？"
        echo ""
        if [ "$confirm" -eq 0 ]; then
            echo "  > 是"
            echo "    否"
        else
            echo "    是"
            echo "  > 否"
        fi
        echo ""
        echo "  音量下：切换"
        echo "  音量上：确认"
        echo "================================"

        local key
        key=$(wait_key)
        if [ "$key" = "down" ]; then
            confirm=$(( (confirm + 1) % 2 ))
        elif [ "$key" = "up" ]; then
            break
        fi
    done

    if [ "$confirm" -eq 1 ]; then
        echo ""
        echo "  已取消冻结操作"
        echo ""
        sleep 1
        return
    fi

    echo ""
    echo "  正在冻结应用..."
    echo ""

    local done_count=0
    local fail_count=0
    for pkg in $unfrozen_list; do
        pm disable --user "$target_uid" "$pkg" >/dev/null 2>&1
        if [ $? -eq 0 ]; then
            done_count=$((done_count + 1))
            echo "  已冻结: $pkg"
        else
            fail_count=$((fail_count + 1))
            echo "  失败: $pkg"
        fi
    done

    echo ""
    echo "================================"
    echo "  冻结完成"
    echo "  成功: $done_count 个"
    echo "  失败: $fail_count 个"
    echo "================================"
    echo ""
    sleep 2
}

MAIN_CURRENT=0
MAIN_COUNT=5

show_main_menu() {
    clear 2>/dev/null
    echo "================================"
    echo "      过强标黑机"
    echo "================================"
    echo ""
    if [ "$MAIN_CURRENT" -eq 0 ]; then echo "  > 1. 修改序列号"; else echo "    1. 修改序列号"; fi
    if [ "$MAIN_CURRENT" -eq 1 ]; then echo "  > 2. 修改ANDROID_ID"; else echo "    2. 修改ANDROID_ID"; fi
    if [ "$MAIN_CURRENT" -eq 2 ]; then echo "  > 3. 越狱隐藏环境"; else echo "    3. 越狱隐藏环境"; fi
    if [ "$MAIN_CURRENT" -eq 3 ]; then echo "  > 4. 冻结多开系统软件"; else echo "    4. 冻结多开系统软件"; fi
    if [ "$MAIN_CURRENT" -eq 4 ]; then echo "  > 5. 退出"; else echo "    5. 退出"; fi
    echo ""
    echo "  音量下：切换"
    echo "  音量上：确认"
    echo "================================"
}

while true; do
    show_main_menu
    key=$(wait_key)
    if [ "$key" = "down" ]; then
        MAIN_CURRENT=$(( (MAIN_CURRENT + 1) % MAIN_COUNT ))
    elif [ "$key" = "up" ]; then
        case $MAIN_CURRENT in
            0) reset_serial ;;
            1) reset_android_id ;;
            2) jailbreak ;;
            3) freeze_apps ;;
            4) echo ""; echo "  退出"; exit 0 ;;
        esac
    fi
done
