#!/bin/bash
# Ollie AMR 系統優化狀態檢查腳本

echo "========================================="
echo "    🤖 Ollie AMR 系統優化狀態檢查腳本    "
echo "========================================="

# 1. 檢查開機時間
echo -e "\n[1] ⏱️  系統開機耗時檢查"
systemd-analyze || echo "無法取得開機時間"

# 2. 檢查預設開機 Target
echo -e "\n[2] 🎯 預設開機 Target 檢查"
TARGET=$(systemctl get-default 2>/dev/null)
if [ "$TARGET" = "multi-user.target" ]; then
    echo "✅ 正確：$TARGET (無圖形介面模式)"
else
    echo "❌ 警告：目前為 $TARGET，建議設為 multi-user.target"
fi

# 3. 檢查 ZRAM 狀態
echo -e "\n[3] 🧠 ZRAM 記憶體壓縮檢查"
if swapon --show | grep -q zram; then
    echo "✅ ZRAM 已啟用"
    swapon --show | grep zram
else
    echo "❌ 警告：未偵測到 ZRAM，請確認 zram-tools 是否已安裝與啟動"
fi

# 4. 檢查 CPU 效能模式
echo -e "\n[4] 🚀 CPU Governor 檢查"
GOV_FILE="/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"
if [ -f "$GOV_FILE" ]; then
    GOV=$(cat $GOV_FILE)
    if [ "$GOV" = "performance" ]; then
        echo "✅ 正確：CPU 運行於 $GOV 模式"
    else
        echo "❌ 警告：CPU 運行於 $GOV 模式，建議設為 performance 避免降頻"
    fi
else
    echo "⚠️ 無法讀取 CPU Governor 檔案"
fi

# 5. 檢查 IPv6 是否關閉
echo -e "\n[5] 🌐 IPv6 狀態檢查"
IPV6_FILE="/proc/sys/net/ipv6/conf/all/disable_ipv6"
if [ -f "$IPV6_FILE" ] && [ "$(cat $IPV6_FILE)" -eq 1 ]; then
    echo "✅ 正確：IPv6 已關閉"
else
    echo "❌ 警告：IPv6 尚未關閉，可能會影響 ROS 2 DDS 探索效率"
fi

# 6. 檢查多餘服務與套件是否已遮蔽/移除
echo -e "\n[6] 🗑️  不必要服務與套件狀態檢查"

check_service_masked() {
    STATUS=$(systemctl is-enabled "$1" 2>/dev/null)
    if [ "$STATUS" = "masked" ] || [ "$STATUS" = "Disabled" ] || [ -z "$STATUS" ] || echo "$STATUS" | grep -q "No such file"; then
        echo "✅ $1 已被正確遮蔽或移除"
    else
        echo "❌ 警告：$1 狀態為 $STATUS，建議將其 mask 或停用"
    fi
}

check_service_masked "systemd-networkd-wait-online.service"
check_service_masked "rsyslog.service"
check_service_masked "fstrim.timer"
check_service_masked "man-db.timer"
check_service_masked "networkd-dispatcher.service"
check_service_masked "snapd.service"
check_service_masked "cloud-init.service"

# 7. 系統即時資源使用狀況
echo -e "\n[7] 📊 系統即時資源使用狀況"

# CPU 負載
LOAD=$(cat /proc/loadavg | awk '{print $1", "$2", "$3}')
echo "  - 📈 CPU 負載 (1m, 5m, 15m): $LOAD"

# 總行程數 (Running Tasks)
TASKS=$(ps -e | wc -l)
echo "  - 🔄 總行程數 (Tasks): $TASKS"

# 記憶體使用狀態
MEM_TOTAL=$(free -m | awk '/^Mem:/{print $2}')
MEM_USED=$(free -m | awk '/^Mem:/{print $3}')
echo "  - 🧠 實體記憶體 (RAM): ${MEM_USED}MB / ${MEM_TOTAL}MB"

SWAP_TOTAL=$(free -m | awk '/^Swap:/{print $2}')
SWAP_USED=$(free -m | awk '/^Swap:/{print $3}')
echo "  - 🗄️  交換空間 (Swap/ZRAM): ${SWAP_USED}MB / ${SWAP_TOTAL}MB"

# 系統硬碟空間
DISK_USE=$(df -h / | awk 'NR==2 {print $5}')
DISK_AVAIL=$(df -h / | awk 'NR==2 {print $4}')
echo "  - 💾 系統硬碟空間 (Root): 已用 $DISK_USE, 剩餘 $DISK_AVAIL"

# CPU 溫度 (適用於 Raspberry Pi 等 ARM 設備)
if [ -f /sys/class/thermal/thermal_zone0/temp ]; then
    TEMP=$(cat /sys/class/thermal/thermal_zone0/temp)
    TEMP_C=$(awk "BEGIN {printf \"%.1f\", $TEMP/1000}")
    echo "  - 🌡️  CPU 溫度: ${TEMP_C}°C"
fi

# 樹莓派降頻/電壓不足警告 (針對 Raspberry Pi)
if command -v vcgencmd &> /dev/null; then
    THROTTLED=$(vcgencmd get_throttled)
    if [ "$THROTTLED" != "throttled=0x0" ]; then
        echo "  - ⚠️  硬體警告: 偵測到降頻或電壓不足 ($THROTTLED)！請檢查電源供應器。"
    else
        echo "  - ⚡ 硬體狀態: 供電與頻率正常 (0x0)"
    fi
fi

# 網路與 ROS 環境
IP_ADDR=$(hostname -I | awk '{print $1}')
echo "  - 🌐 區域網路 IP: ${IP_ADDR:-無連線}"
echo "  - 🤖 ROS_DOMAIN_ID: ${ROS_DOMAIN_ID:-未設定}"

echo -e "\n[8] 🐳 Docker 運行狀態"
if systemctl is-active --quiet docker; then
    DOCKER_RUNNING=$(docker ps --format "  - {{.Names}} ({{.Status}})")
    if [ -z "$DOCKER_RUNNING" ]; then
        echo "  - 目前無執行中的容器"
    else
        echo "$DOCKER_RUNNING"
    fi
else
    echo "  - Docker 服務目前未啟動 (符合節能設定)"
fi

echo -e "\n[9] 🔥 資源消耗 Top 3 行程 (Process)"
echo "  [記憶體佔用 Top 3]"
ps -eo pid,user,%mem,comm --sort=-%mem | head -n 4 | awk 'NR>1 {printf "    PID:%-6s User:%-8s Mem:%-5s Cmd:%s\n", $1, $2, $3"%", $4}'
echo "  [CPU 佔用 Top 3]"
ps -eo pid,user,%cpu,comm --sort=-%cpu | head -n 4 | awk 'NR>1 {printf "    PID:%-6s User:%-8s CPU:%-5s Cmd:%s\n", $1, $2, $3"%", $4}'

echo -e "\n========================================="
echo "    🎉 檢查完成！請確認上述項目皆顯示 ✅    "
echo "========================================="