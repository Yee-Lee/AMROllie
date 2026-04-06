# Ultrasonic Test

這是一個位於 `example/ultrasonic_test/` 獨立測試專案，專門用於驗證雙超音波感測器（Dual Ultrasonic Sensors）與 `ReactiveBrake` 模組的硬體連接和數據讀取狀態。

## 測試目的
1. 確認超音波感測器與 ESP32 板的接線無誤。
2. 透過 `ReactiveBrake` 模組來初始化感測器並觸發讀取。
3. 確認每 100ms 能夠從左右感測器穩定地獲取正確的距離數值（以公分 cm 為單位）。

## 編譯與上傳方式
這個測試專案已被設定在 `platformio.ini` 的 `[env:ultrasonic_test]` 環境中。

### 透過 CLI 指令
請在專案根目錄下執行以下指令：
```bash
pio run -e ultrasonic_test -t upload
```

### 透過 VSCode PlatformIO 擴充功能
1. 在左下角或 PlatformIO 面板中，點選 `env:main` 並切換至 `env:ultrasonic_test`。
2. 點擊 `Build` 進行編譯。
3. 點擊 `Upload` 上傳至 ESP32。

## 預期結果
上傳完成後，開啟 Serial Monitor (Baud rate 設定為 `115200`)，您應該會看到以下開機啟動訊息與持續更新的距離數據：

```text
Ultrasonic Sensor Serial Test Started! (Dual Sensors)
Sonar Left:  12.3 cm | Sonar Right:  15.0 cm
Sonar Left:  12.4 cm | Sonar Right:  15.1 cm
...
```

若感測器未能正確讀取，數值可能會顯示 `0.0 cm` 或是異常極大值，請檢查您的硬體接線是否穩固。
