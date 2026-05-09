#!/bin/bash

SERVICES=("ollie_microros" "ollie_description" "ollie_lidar")

echo "--- Ollie Service Status ---"
for S in "${SERVICES[@]}"; do
    STATUS=$(systemctl is-active "$S.service" 2>/dev/null)
    case $STATUS in
        active)   echo -e "🟢 active   - $S" ;;
        failed)   echo -e "🔴 failed   - $S" ;;
        inactive) echo -e "⚪ inactive - $S" ;;
        *)        echo -e "❓ unknown  - $S ($STATUS)" ;;
    esac
done
