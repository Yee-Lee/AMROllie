# Ollie AMR 上位機 (Raspberry Pi 5B) 工業級效能優化指南

針對 Raspberry Pi 5B 與 Ubuntu 24.04 (Noble)，為了達到**工業化等級的穩定性與可預期性**，我們必須徹底消除系統中任何可能在背景偷偷佔用 CPU 或 I/O 的任務（如自動更新、套件快取、背景索引）。

> **💡 優化哲學**：「消除所有不可控因素，將每一滴算力與 I/O 頻寬留給機器人即時運算。」

---

## 階段 1：基礎環境清理 (徹底移除自動維護機制)

Ubuntu 預設包含許多針對桌面的背景維護任務，這些在工業機器人上會造成嚴重的 I/O 競爭。

### 1.1 移除 Snapd 與 Cloud-init
Snap 會掛載大量虛擬檔案系統，而 Cloud-init 會在開機時耗費過多時間。
```bash
# 移除 Snapd
sudo systemctl stop snapd
sudo systemctl disable snapd
sudo apt-get purge -y snapd
sudo rm -rf /snap /var/snap /var/lib/snapd /var/cache/snapd

# 移除 Cloud-init
sudo touch /etc/cloud/cloud-init.disabled
sudo apt-get purge -y cloud-init
sudo rm -rf /etc/cloud /var/lib/cloud
```

### 1.2 停用自動更新與非必要伺服器套件
```bash
sudo apt-get purge -y unattended-upgrades modemmanager multipath-tools
sudo apt-get autoremove -y
```

---

## 階段 2：I/O 與開機速度優化 (針對工業化部署)

### 2.1 關閉耗時的背景排程
磁區整理 (fstrim) 與說明文件索引重建 (man-db) 會在啟動時瘋狂榨乾 SD 卡/SSD 的效能。
```bash
sudo systemctl disable fstrim.timer man-db.timer dpkg-db-backup.timer
sudo systemctl mask fstrim.service man-db.service dpkg-db-backup.service
```

### 2.2 關閉網路連線等待
允許系統先進入多使用者模式 (Multi-user) 再於背景連網，可縮短開機時間。
```bash
sudo systemctl disable systemd-networkd-wait-online.service
sudo systemctl mask systemd-networkd-wait-online.service
```

### 2.3 關閉雙重日誌寫入 (RSyslog)
Systemd 預設已經有 `journald`。關閉 `rsyslog` 可減少對存儲介質的寫入負擔。
```bash
sudo systemctl disable rsyslog.service
sudo systemctl mask rsyslog.service
```

---

## 階段 3：核心硬體與網路調校

### 3.1 固定 CPU 效能模式 (Performance Governor)
強制將 CPU 鎖定在最高時脈。
**⚠️ 注意：必須安裝主動式散熱器 (Active Cooler) 才能啟用此項！**
```bash
sudo apt install cpufrequtils
echo 'GOVERNOR="performance"' | sudo tee /etc/default/cpufrequtils
sudo systemctl restart cpufrequtils
```

### 3.2 關閉 IPv6
ROS 2 的 DDS 在 IPv6 下常有探索問題，建議關閉以維持通訊穩定。
```bash
sudo sh -c 'echo "net.ipv6.conf.all.disable_ipv6 = 1" >> /etc/sysctl.conf'
sudo sh -c 'echo "net.ipv6.conf.default.disable_ipv6 = 1" >> /etc/sysctl.conf'
sudo sh -c 'echo "net.ipv6.conf.lo.disable_ipv6 = 1" >> /etc/sysctl.conf'
sudo sysctl -p
```

---

## 階段 4：記憶體與穩定性管理 (ZRAM)

即便 RPi 5B 記憶體充足，改用 **ZRAM** (壓縮記憶體) 取代傳統 Swap 仍能保護 SD 卡並提升系統在極端負載下的響應速度。

```bash
sudo apt-get install -y zram-tools
```
編輯設定檔 `/etc/default/zramswap`：
```ini
ALGO=lz4
PERCENT=50
```
重啟服務：`sudo systemctl restart zramswap`

---

## 階段 5：進階診斷工具

完成上述優化後，應定期使用以下工具監控系統是否符合「工業化」預期：
- **`systemd-analyze blame`**：檢查開機瓶頸。
- **`htop`**：監控背景是否有不預期的 Process 突然竄起。
- **`zramctl`**：檢查 Swap 運作狀態。
