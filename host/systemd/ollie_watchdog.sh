#!/bin/bash

# AMROllie Active Watchdog
# 雙重檢查機制：
# 1. 檢查 ollie_microros.service 是否為 active 狀態
# 2. 若 active，進一步檢查 /odom topic 是否能在 3 秒內收到數據

# --- 環境設定 ---
export ROS_DOMAIN_ID=30
LOG_FILE="/var/log/ollie_watchdog.log"
SERVICE_NAME="ollie_microros.service"

# Source ROS 2 核心環境
if [ -f "/opt/ros/humble/setup.bash" ]; then
    source /opt/ros/humble/setup.bash
fi

# 動態取得當前腳本的絕對路徑目錄，藉此推導 workspace 路徑
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
HOST_DIR="$(dirname "$SCRIPT_DIR")"
WS_SETUP="$HOST_DIR/ros2_ws/install/local_setup.bash"

# Source Ollie 工作空間環境
if [ -f "$WS_SETUP" ]; then
    source "$WS_SETUP"
fi

# --- 輔助函數：重啟服務並寫入日誌 ---
restart_service() {
    local REASON=$1
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] [WATCHDOG] ERROR: ${REASON}. Restarting ${SERVICE_NAME}..." >> "$LOG_FILE"
    
    systemctl restart "$SERVICE_NAME"
    
    if [ $? -eq 0 ]; then
        echo "[$(date '+%Y-%m-%d %H:%M:%S')] [WATCHDOG] SUCCESS: Service restarted successfully." >> "$LOG_FILE"
    else
        echo "[$(date '+%Y-%m-%d %H:%M:%S')] [WATCHDOG] CRITICAL: Failed to restart service." >> "$LOG_FILE"
    fi
}

# --- 檢查階段 1: 服務進程狀態 (快篩) ---
SERVICE_STATUS=$(systemctl is-active "$SERVICE_NAME")

if [ "$SERVICE_STATUS" != "active" ]; then
    restart_service "Service status is '$SERVICE_STATUS'"
    exit 0 # 重啟後腳本結束，等下一分鐘再檢查
fi

# --- 檢查階段 2: 實際數據通訊 (深蹲) ---
# 使用 timeout 3 秒嘗試讀取一筆資料
# --once: 收到一筆後就退出
# --no-arr: 減少輸出負擔
timeout 3 ros2 topic echo /odom --once --no-arr > /dev/null 2>&1
EXIT_CODE=$?

if [ $EXIT_CODE -ne 0 ]; then
    restart_service "/odom timeout or no data (Exit: $EXIT_CODE), possible dead-lock"
    exit 0
fi

# 如果走到這裡，代表狀態正常 (可選：平時不寫入 Log 以節省空間)
# echo "[$(date '+%Y-%m-%d %H:%M:%S')] [WATCHDOG] OK: System is healthy." >> "$LOG_FILE"
exit 0
