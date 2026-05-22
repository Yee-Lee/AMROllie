
#!/bin/bash
echo "🔄 準備手動開啟藍牙功能..."

# 解除硬體電源封鎖
sudo rfkill unblock bluetooth

# 啟動藍牙守護進程
sudo systemctl start bluetooth.service

# 等待一秒讓硬體初始化
sleep 1 

# 確保藍牙控制器開啟
sudo bluetoothctl power on

if systemctl is-active --quiet bluetooth.service; then
    echo "✅ 藍牙已成功開啟並接管硬體！"
else
    echo "❌ 藍牙啟動失敗。"
fi

