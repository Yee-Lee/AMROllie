#! /bin/bash

systemd-analyze

systemd-analyze blame | head -n 10

systemd-analyze critical-chain
