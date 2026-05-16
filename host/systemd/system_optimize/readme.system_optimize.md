# 🚀 AMROllie 系統優化與分析工具

本目錄 (`system_optimize/`) 存放了用於檢查與分析 Ollie 上位機 (RPI3B) 系統效能與健康狀態的腳本。這些工具可幫助開發者確認系統是否已配置在最佳狀態，並快速排除資源瓶頸。

---

## 🛠️ 腳本清單與功能介紹

### 1. 系統狀態全檢腳本 (`check_optimizations.sh`)

這是系統優化的核心檢查工具。它提供了一鍵式的綜合健康檢查，特別針對 ROS 2 與樹莓派的硬體限制進行了客製化。

*   **目的**：驗證機器人作業系統是否已針對效能進行了必要的優化，並顯示當前的硬體資源消耗情況。
*   **檢查項目涵蓋**：
    1.  ⏱️ **開機耗時**：檢視 `systemd-analyze` 總結。
    2.  🎯 **預設 Target**：確認是否處於較省資源的無 GUI 模式 (`multi-user.target`)。
    3.  🧠 **ZRAM 狀態**：確保記憶體壓縮機制正常運作，以彌補實體 RAM 的不足。
    4.  🚀 **CPU Governor**：檢查 CPU 是否鎖定在 `performance` 模式避免降頻影響即時性。
    5.  🌐 **IPv6 狀態**：確保 IPv6 已關閉以加速 ROS 2 DDS 的探索 (Discovery) 過程。
    6.  🗑️ **無用服務遮蔽**：確認已停用如 `cloud-init`、`snapd` 等耗資源服務。
    7.  📊 **即時資源使用**：列出 CPU 負載、記憶體/Swap 使用率、硬碟空間，以及**樹莓派專屬的 CPU 溫度與供電降頻警告 (Throttled status)**。
    8.  🐳 **Docker 狀態**：列出正在執行的容器。
    9.  🔥 **Top 3 資源消耗**：列出當前佔用 CPU 與記憶體最多的進程。
*   **用法**：
    ```bash
    cd ~/workspace/AMROllie/host/systemd/system_optimize
    ./check_optimizations.sh
    ```

### 2. 開機速度分析腳本 (`run_boot_analyze.sh`)

當發現系統開機異常緩慢，或者剛剛新增了新的 systemd 服務時，用來分析開機流程的工具。

*   **目的**：快速列出開機時間的瓶頸，幫助開發者抓出拖慢開機速度的「兇手」。
*   **功能背後執行的指令**：
    1.  `systemd-analyze`：顯示核心、initrd 與 user space 的開機總耗時。
    2.  `systemd-analyze blame`：列出開機耗時前 10 名的服務。
    3.  `systemd-analyze critical-chain`：印出開機的「關鍵路徑」，顯示各服務間的依賴與等待時間。
*   **用法**：
    ```bash
    cd ~/workspace/AMROllie/host/systemd/system_optimize
    ./run_boot_analyze.sh
    ```

---

## 💡 使用時機建議

*   **剛燒錄完系統或重新配置環境後**：執行 `check_optimizations.sh` 確保所有優化步驟皆有確實生效。
*   **出車前 (Pre-flight Check)**：快速跑一次 `check_optimizations.sh` 檢查硬體供電 (Throttled) 與 CPU 溫度是否正常。
*   **修改了 `.service` 或開機程序後**：執行 `run_boot_analyze.sh` 確保新的設定沒有嚴重拖慢系統啟動時間。