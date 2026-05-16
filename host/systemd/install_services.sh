#!/bin/bash

# AMROllie Systemd Service Installer
# 此腳本會根據當前環境路徑與使用者，將 .template 轉換為實際的 .service，並註冊到系統中。

# 獲取絕對路徑與使用者名稱
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
SYSTEMD_DIR="$SCRIPT_DIR/system"
CURRENT_USER=$(whoami)

echo "--- AMROllie Systemd Installer ---"
echo "Target User: $CURRENT_USER"
echo "Target Path: $SCRIPT_DIR"
echo "----------------------------------"

# 檢查 system 目錄是否存在
if [ ! -d "$SYSTEMD_DIR" ]; then
    echo "Error: Directory $SYSTEMD_DIR not found!"
    exit 1
fi

cd "$SYSTEMD_DIR" || exit

# 1. 處理所有 .template 檔案
for TEMPLATE_FILE in *.template; do
    if [ -f "$TEMPLATE_FILE" ]; then
        # 去除 .template 副檔名作為輸出檔名
        SERVICE_FILE="${TEMPLATE_FILE%.template}"
        
        echo "Generating $SERVICE_FILE..."
        
        # 使用 sed 取代佔位符
        # 取代 <your_username> 為實際使用者
        # 取代 /home/<your_username>/workspace/AMROllie/host 為腳本上一層 (host) 的絕對路徑
        HOST_DIR="$(dirname "$SCRIPT_DIR")"
        
        sed -e "s|<your_username>|$CURRENT_USER|g" \
            -e "s|/home/<your_username>/workspace/AMROllie/host|$HOST_DIR|g" \
            "$TEMPLATE_FILE" > "$SERVICE_FILE"
    fi
done

echo "----------------------------------"
echo "Template generation complete."
echo "Proceeding to systemd registration (requires sudo)..."

# 2. 註冊服務 (軟連結)
SERVICES=("ollie_microros.service" "ollie_description.service" "ollie_lidar.service" "ollie_watchdog.service" "ollie_watchdog.timer")

for S in "${SERVICES[@]}"; do
    if [ -f "$SYSTEMD_DIR/$S" ]; then
        echo "Linking $S to /etc/systemd/system/"
        # 強制覆蓋舊的軟連結
        sudo ln -sf "$SYSTEMD_DIR/$S" "/etc/systemd/system/$S"
    else
        echo "Warning: Generated file $S not found, skipping."
    fi
done

# 3. 賦予 Watchdog 腳本執行權限
chmod +x "$SCRIPT_DIR/ollie_watchdog.sh"

# 4. Reload Systemd
echo "Reloading systemd daemon..."
sudo systemctl daemon-reload

echo "----------------------------------"
echo "✅ Installation successful!"
echo "You can now enable and start the services. Example:"
echo "  sudo systemctl enable --now ollie_microros.service"
echo "  sudo systemctl enable --now ollie_watchdog.timer"
echo ""
echo "Run './ollie_status.sh' to check the status."
