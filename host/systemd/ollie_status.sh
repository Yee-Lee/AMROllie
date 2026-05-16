#!/bin/bash

# AMROllie System Status Checker

SERVICES=("ollie_microros" "ollie_description" "ollie_lidar")
GUARDS=("ollie_watchdog")

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
for G in "${GUARDS[@]}"; do
    STATUS=$(systemctl is-active "$G.service" 2>/dev/null)
    case $STATUS in
        active)   echo -e "🛡️ active   - $G (Watchdog Protection)" ;;
        failed)   echo -e "🔴 failed   - $G" ;;
        inactive) echo -e "⚪ inactive - $G (Protection Disabled)" ;;
        *)        echo -e "❓ unknown  - $G ($STATUS)" ;;
    esac
done
