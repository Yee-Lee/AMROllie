#!/bin/bash

# AMROllie System Status Checker

SERVICES=("ollie_microros" "ollie_description" "ollie_lidar")
TIMERS=("ollie_watchdog")

echo "--- Ollie Core Services ---"
for S in "${SERVICES[@]}"; do
    STATUS=$(systemctl is-active "$S.service" 2>/dev/null)
    case $STATUS in
        active)   echo -e "🟢 active   - $S" ;;
        failed)   echo -e "🔴 failed   - $S" ;;
        inactive) echo -e "⚪ inactive - $S" ;;
        *)        echo -e "❓ unknown  - $S ($STATUS)" ;;
    esac
done

echo ""
echo "--- Ollie System Guards ---"
for T in "${TIMERS[@]}"; do
    STATUS=$(systemctl is-active "$T.timer" 2>/dev/null)
    case $STATUS in
        active)   echo -e "🛡️ active   - $T (Watchdog Protection)" ;;
        inactive) echo -e "⚪ inactive - $T (Protection Disabled)" ;;
        *)        echo -e "❓ unknown  - $T ($STATUS)" ;;
    esac
done
