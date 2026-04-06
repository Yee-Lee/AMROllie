# AMROllie Firmware Standalone

本專案為 AMROllie 的韌體獨立版本（Firmware Standalone），基於 PlatformIO 與 ESP32 開發。

## 目錄架構原則

為了維持專案的整潔與便於維護，本專案的目錄結構遵循以下原則：

- `src/`：**核心韌體代碼**。這裡只放置實際要在機器人主板上運行的正式、編譯所需的主程式與相關模組。`env:main` 編譯時預設只編譯此目錄下的代碼。
- `example/`：**測試與驗證代碼**。用於單獨測試特定感測器、馬達或模組的獨立程式碼（例如：超音波感測器測試）。每個測試應放置於獨立的子目錄中（如 `example/ultrasonic_test/`）。
- `lib/`：**自定義或第三方硬體依賴函式庫**。若是專案特有的函式庫，可放置於此。
- `include/`：**全域標頭檔**（若有需要）。
- `docs/`：**專案文件**。包含架構說明、重構原則、以及各項測試的說明文件。

## 配置與參數管理 (Configuration)

專案將所有硬體腳位（Pins）、實體機構參數、PID 控制參數及網路設定，統一抽離集中至 `src/config.h` 進行管理。
- **`config.h.default`**：為配置檔的預設範本。初次取得專案後，請將其複製並命名為 `config.h`，再填入正確的 WiFi SSID 與密碼。
- `config.h` 應被加入 `.gitignore` 中，以避免將個人的網路敏感資訊提交至版本控制系統。

## Web 伺服器架構 (Web Server UI)

為保持主程式整潔與高度可擴充性，本專案採用階層式的 Web Server 設計 (`main --> WebServerManager --> Pages`)：

- **核心管理 (`WebServerManager.h`)**：專注於網路基礎建設，包含 WiFi 連線、mDNS (`esp32-ollie.local`) 註冊，以及啟動並管理 `AsyncWebServer` 實體。
- **頁面模組 (`src/web/` 目錄)**：每個 UI 頁面與功能（例如入口首頁 `indexPage.h`、遙控介面 `JoystickPage.h`、數據監控 `statPage.h`）均被封裝為獨立模組。
- **擴充方式**：若要新增新的網頁介面，只需在 `src/web/` 下建立新的頁面類別，實作 `attachToServer(AsyncWebServer* server)` 方法來註冊 HTTP 路由或 WebSocket，並在 `main.cpp` 中將其掛載到 `webServer` 即可。
  
> 💡 **Tip:** 若於 `example/` 建立單獨測試環境，亦可隨意 include 指定的 Web Page 模組來快速搭建測試網頁。

## 編譯專案的原則

專案的編譯環境設定統一管理於 `platformio.ini` 中，遵循以下規則：

1. **共用設定 (`[common]`)**：
   所有的編譯環境應繼承自 `[common]` 區塊，該區塊包含了共用的開發板設定（`board = esp32dev`）、框架（`framework = arduino`）以及共用的函式庫依賴（`lib_deps`）。
2. **主要環境 (`[env:main]`)**：
   這是專案預設的編譯環境（`default_envs = main`），預設會編譯 `src/` 底下的所有源碼。
3. **測試環境 (`[env:<test_name>]`)**：
   針對 `example/` 底下的獨立測試專案，我們會在 `platformio.ini` 中建立對應的環境（例如 `[env:ultrasonic_test]`）。
   - 透過修改 `build_src_filter` 來**排除** `src/` 的主程式碼（例如 `-<*>` 或 `-<main.cpp>`）。
   - 將編譯路徑指向對應的測試目錄（例如 `+<../example/ultrasonic_test/>`）。
   - 這樣可以確保測試專案能獨立編譯，不會與主韌體邏輯產生衝突。

## 後續重構 (Refactor) 指南

在進行重構時，請確保：
1. 新增的測試程式碼**不要**混入 `src/` 目錄，應獨立建立在 `example/` 下。
2. `src/` 中的模組應盡量降低耦合度，方便在 `example/` 中被單獨 include 進行測試。
3. 任何新增的環境設定，請遵循上述的 `platformio.ini` 編輯原則。
