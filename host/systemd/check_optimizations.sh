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

echo -e "\n========================================="
echo "    🎉 檢查完成！請確認上述項目皆顯示 ✅    "
echo "========================================="