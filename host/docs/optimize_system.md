# Ollie AMR 上位機 (Raspberry Pi 3B) 效能優化指南

Raspberry Pi 3B 僅具備 1GB RAM，卻需要同時負擔 ROS 2、micro-ROS Agent、LiDAR 驅動、SLAM 建圖、Nav2 導航以及藍牙搖桿解析。為了避免系統資源枯竭（OOM, Out of Memory）與 CPU 滿載導致的嚴重延遲，本指南記錄了針對 Ubuntu 22.04 Server 的極致瘦身與效能優化步驟。

> **💡 核心優化哲學**：「預設關閉非必要服務，子模組隨選啟用（On-Demand），並將每一滴算力留給 ROS 2。」

---

## 階段 1：基礎核心瘦身 (移除多餘套件與背景排程)

Ubuntu 預設包含許多針對雲端伺服器的套件與背景維護任務，這些在邊緣運算設備上會造成嚴重的 I/O 競爭與記憶體消耗。

### 1.1 徹底移除 Snapd 與 Cloud-init
Snap 套件管理器會掛載大量虛擬檔案系統，而 Cloud-init 會在開機時耗費過多時間等待網路驗證。
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

**驗證方法**：執行 `systemctl status cloud-init` 應顯示服務找不到。執行 `ls -d /etc/cloud` 應顯示「沒有此一檔案或目錄」。

### 1.3 移除其他非必要服務
例如數據機管理 (ModemManager)、多重路徑儲存 (Multipathd) 以及無人值守自動更新。
```bash
sudo apt-get purge -y unattended-upgrades modemmanager multipath-tools
sudo apt-get autoremove -y
```

**驗證方法**：執行 `systemctl status ModemManager multipathd unattended-upgrades`，應提示這些服務皆無法找到或無法辨識。

## 階段 2：記憶體管理優化 (ZRAM 取代傳統 Swap)

SD 卡的隨機讀寫速度極慢，若 ROS 2 導航時觸發傳統 Swap (將硬碟當記憶體用)，系統會直接卡死。我們改用 **ZRAM**（將記憶體壓縮當作 Swap），用極少量的 CPU 運算換取大量的記憶體空間與流暢度。

```bash
sudo apt-get install -y zram-tools
```
編輯設定檔 `/etc/default/zramswap`：
```ini
ALGO=lz4
PERCENT=50
```
重啟服務套用：
```bash
sudo systemctl restart zramswap
```

**驗證方法**：執行 `zramctl`，應能看到 `/dev/zram0` 且其 ALGORITHM 欄位顯示為 `lz4`。執行 `swapon --show` 應顯示 `/dev/zram0` 正在被系統當作 Swap 空間使用。

## 階段 3：CPU 與網路核心調整

### 3.1 固定 CPU 效能模式 (Performance Governor)
預設 CPU 會為了省電而降頻。我們強制將其鎖定在最高時脈。
```bash
sudo apt-get install -y cpufrequtils
echo 'GOVERNOR="performance"' | sudo tee /etc/default/cpufrequtils
sudo systemctl restart cpufrequtils
```

**驗證方法**：執行 `cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor`，其輸出結果應明確顯示為 `performance`。

### 3.2 關閉 IPv6
ROS 2 的 DDS 在 IPv6 下常有探索 (Discovery) 問題，且增加網路堆疊的負擔，建議關閉。
編輯 `/etc/sysctl.conf` 並在底部加入：
```ini
net.ipv6.conf.all.disable_ipv6 = 1
net.ipv6.conf.default.disable_ipv6 = 1
net.ipv6.conf.lo.disable_ipv6 = 1
```
套用設定：`sudo sysctl -p`

**驗證方法**：執行 `ip a` 檢查網路介面，各個網卡中應不再顯示 `inet6` 的位址；或執行 `cat /proc/sys/net/ipv6/conf/all/disable_ipv6`，輸出應為 `1`。

## 階段 4：Docker 與 ROS 2 負載控管

### 4.1 限制 Docker 日誌大小
避免 micro-ROS Agent 長期運作後，Docker 噴出過多 Log 塞滿 SD 卡與記憶體。
建立或編輯 `/etc/docker/daemon.json`：
```json
{
  "log-driver": "json-file",
  "log-opts": {
    "max-size": "10m",
    "max-file": "3"
  }
}
```
重新啟動 Docker：`sudo systemctl restart docker`

**驗證方法**：執行 `docker info --format '{{.LoggingDriver}}'` 應輸出 `json-file`。後續運行容器時，也可以使用 `docker inspect <container_name> | grep -A 5 LogConfig` 確認 `max-size` 等參數已套用到個別容器中。

### 4.2 ROS 2 通訊層 (DDS) 最佳化配置
預設的 FastRTPS 為了支援大網路環境，會保留大量共用記憶體。我們可以針對本機通訊 (`localhost`) 與單一網段進行 XML 參數調校（待 Nav2 部署時建立對應的 `fastdds_profile.xml` 限制參與者記憶體分配）。

## 階段 5：保留必要功能檢查

經過上述優化，請確保以下我們 AMR 仰賴的核心功能 **依然保持開啟**：
- **Bluetooth 服務** (`bluetooth.service`)：供 PS4 搖桿 (`joy_linux`) 配對與操作。
- **Docker 服務** (`docker.service`)：供 micro-ROS-agent 運行。
- **Systemd-udevd** (`systemd-udevd.service`)：供 USB TTL 與 LiDAR 綁定名稱 (`/dev/ollie_core`, `/dev/ollie_lidar`)。
- **WiFi 網路** (透過 `netplan` 或 `NetworkManager`)：供 SSH 與 Foxglove 遠端監控。

## 階段 6：進階開機速度優化 (Systemd 瓶頸排除)

透過 `systemd-analyze` 工具，我們可以精準找出拖慢開機的服務。這是在做進階優化時最重要的分析手段，共有三個常用的診斷指令：

1. **查看總開機耗時**：
   ```bash
   systemd-analyze
   ```
   *（會顯示 Kernel 載入時間與 Userspace 服務啟動時間總和）*

2. **列出所有服務的啟動耗時 (由慢到快排列)**：
   ```bash
   systemd-analyze blame | head -n 20
   ```
   *（快速抓出耗時最久的單一服務）*

3. **查看關鍵路徑 (找出互相等待導致卡住的流程)**：
   ```bash
   systemd-analyze critical-chain
   ```
   *（分析服務之間的相依性瓶頸）*

透過上述工具診斷，發現網路等待與高 I/O 任務是拖慢 Raspberry Pi 開機的最主要元凶。為了達成 1 分鐘內開機的目標，請關閉以下服務：

### 6.1 關閉網路連線等待
系統預設會阻塞並等待網路完全連線，這會導致 Docker (以及 micro-ROS) 延遲將近 2 分鐘才啟動。對於 AMR 而言，允許系統先開機再於背景慢慢連網是較佳的做法。
```bash
sudo systemctl disable systemd-networkd-wait-online.service
sudo systemctl mask systemd-networkd-wait-online.service
```

**驗證方法**：執行 `systemctl is-enabled systemd-networkd-wait-online.service`，輸出應為 `masked`。重啟後執行 `systemd-analyze blame`，該服務應不復存在或耗時極短。

### 6.2 關閉高磁碟 I/O 的背景排程
`fstrim` (磁區整理) 與 `man-db` (說明文件索引重建) 在啟動時會瘋狂榨乾 SD 卡的讀寫效能，導致其他開機服務卡死（耗時可達 2 ~ 4 分鐘）。在邊緣運算設備上強烈建議關閉。
```bash
sudo systemctl disable fstrim.timer man-db.timer dpkg-db-backup.timer
sudo systemctl mask fstrim.service man-db.service dpkg-db-backup.service
```

**驗證方法**：執行 `systemctl status fstrim.timer man-db.timer`，兩者皆應顯示為 inactive (dead) 且狀態為 masked。重啟後透過 `systemd-analyze blame` 確認它們不再佔用開機時間。

### 6.3 關閉次要背景服務與檢測工具
以下服務在機器人平台上通常非必要，但會佔用大量的 CPU 週期或 I/O，導致其他重要服務（如 Docker）延遲啟動：
- `networkd-dispatcher`：用於回應網路狀態改變並執行腳本（耗時可達 1 分鐘以上）。
- `e2scrub_reap`：Ext4 檔案系統的背景檢查，對 Raspberry Pi 的 SD 卡沒有實質幫助。
- `rpi-eeprom-update`：開機檢查樹莓派韌體更新。

```bash
sudo systemctl disable networkd-dispatcher.service e2scrub_reap.service rpi-eeprom-update.service
sudo systemctl mask networkd-dispatcher.service e2scrub_reap.service rpi-eeprom-update.service
```

**驗證方法**：執行 `systemctl is-enabled networkd-dispatcher.service` 等指令，輸出應為 `masked`。

### 6.4 關閉重複的系統日誌服務與圖形介面
從分析結果可以發現：
- `rsyslog.service` 耗時 26 秒。Systemd 預設已經有 `systemd-journald` 負責高效能、二進制的日誌收集。如果系統又跑了 `rsyslog`，它會再次將日誌寫入傳統的文字檔 (`/var/log/syslog`)，這會造成 SD 卡雙重寫入的嚴重 I/O 負擔。
- 系統的開機目標被設為 `graphical.target` (圖形介面模式)。由於我們是無頭 (Headless) 的機器人伺服器，不需要啟動桌面環境相關的子系統。

*(註：`avahi-daemon.service` 雖然也耗時，但它是提供 `ollie.local` mDNS 網域名稱解析的核心服務，在此保留不關閉。)*

```bash
# 關閉 rsyslog，避免日誌雙重寫入
sudo systemctl disable rsyslog.service
sudo systemctl mask rsyslog.service

# 切換預設開機目標為無圖形介面的多使用者模式
sudo systemctl set-default multi-user.target
```

**驗證方法**：執行 `systemctl get-default`，輸出應為 `multi-user.target`。重啟後執行 `systemd-analyze blame`，`rsyslog.service` 應不再出現。

## 結論與檢測

完成上述設定後，請重新開機 (`sudo reboot`)，並使用 `htop` 與 `systemd-analyze` 指令檢查系統資源：
- 預期總開機時間 (Userspace) 應能大幅縮短，目標為 **1 分鐘以內**。
- 預期開機閒置記憶體佔用應下降至 **150MB ~ 250MB** 左右。
- 系統總行程數 (Tasks) 應大幅減少。
- CPU 頻率應穩定在最高值 (例如 1.2GHz 或 1.4GHz，依 RPi3B 版本而定)。

在此輕量化基礎上，後續啟動 SLAM 與 Nav2 時，RPi3B 才能騰出足夠的資源進行即時的矩陣運算。