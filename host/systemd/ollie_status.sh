#!/bin/bash

# 定義顯示狀態的邏輯
echo "--- Ollie Services Status ---"
found=0

# 抓取系統服務狀態
while read -r unit load active sub rest; do
    [ -z "$unit" ] && continue
    found=1
    if [ "$active" == "active" ]; then
        # 正常運行：綠色圓點
        printf "  \e[32m●\e[0m %-30s -> \e[32m%s\e[0m\n" "$unit" "$sub"
    else
        # 異常或停止：紅色圓點
        printf "  \e[31m●\e[0m %-30s -> \e[31m%s (%s)\e[0m\n" "$unit" "$active" "$sub"
    fi
done < <(systemctl list-units --type=service --all "ollie*" --no-pager --no-legend 2>/dev/null)

if [ $found -eq 0 ]; then
    echo "  [!] 未發現任何 Ollie 相關服務"
fi
echo "-----------------------------"
