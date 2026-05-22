#!/bin/bash
# 手動喚醒 Docker 服務腳本 (供除錯或退路機制使用)

echo "🐳 正在啟動 Docker 引擎相關服務 (僅當次開機有效)..."
sudo systemctl start docker.service docker.socket containerd.service

# 檢查 Docker 服務是否成功運行
if systemctl is-active --quiet docker.service; then
    echo "✅ Docker 服務已成功啟動！"
    echo "👉 您現在可以執行 ./run_docker.sh 來啟動 micro-ROS Agent 容器了。"
else
    echo "❌ Docker 服務啟動失敗！"
    echo "請執行 'journalctl -u docker.service --no-pager' 查看錯誤日誌。"
fi