#!/bin/bash

echo "🔄 準備開啟藍牙功能..."

# 解除軟體封鎖
sudo rfkill unblock bluetooth

# 啟動硬體底層通訊 (Raspberry Pi 特有)
sudo systemctl start hciuart.service

# 啟動藍牙服務
sudo systemctl start bluetooth

# 檢查藍牙狀態
if systemctl is-active --quiet bluetooth; then
    echo "✅ 藍牙已成功開啟！"
else
    echo "❌ 藍牙開啟失敗，請檢查系統狀態。"
fi
