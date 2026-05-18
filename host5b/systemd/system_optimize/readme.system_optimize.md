# 🚀 AMROllie 系統優化腳本工具 (system_optimize)

本目錄存放了用於快速檢查與分析 Raspberry Pi 5B 系統健康狀態的腳本。這些腳本是對應 `docs/optimize_system.md` (工業級效能優化指南) 的實用工具。

---

## 🛠️ 腳本說明

### 1. 系統狀態全檢腳本 (`check_optimizations.sh`)
一鍵式健康檢查，驗證各項工業級優化是否已正確套用。
*   **涵蓋內容**：開機耗時、Target 模式、ZRAM 狀態、CPU 模式 (Governor)、IPv6 狀態、被遮蔽服務檢查 (如 snapd, unattended-upgrades)、硬體供電/溫度警告，以及即時 Top 3 資源消耗。
*   **用法**：
    ```bash
    cd ~/workspace/AMROllie/host5b/systemd/system_optimize
    ./check_optimizations.sh
    ```

### 2. 開機速度分析腳本 (`run_boot_analyze.sh`)
用來分析並抓出拖慢開機速度的「兇手」。
*   **涵蓋內容**：執行 `systemd-analyze blame` (耗時排名) 與 `critical-chain` (關鍵相依路徑)。
*   **用法**：
    ```bash
    cd ~/workspace/AMROllie/host5b/systemd/system_optimize
    ./run_boot_analyze.sh
    ```

---
*💡 詳細的優化原理與設定指令，請參閱專案根目錄下的 `docs/optimize_system.md`。*