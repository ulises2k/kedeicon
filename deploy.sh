#!/bin/sh
# deploy.sh - build + install + (re)start kedeicon on the Pi.
# Run from the kedeicon/ source dir on the Pi:  sh deploy.sh
# Assumes spidev-clean mode (see KEDEICON.md): /dev/spidev0.1 present, no kedei35fb/ads7846.
set -e
cd "$(dirname "$0")"

echo "[kedeicon] generating font (from system console font)..."
python3 genfont.py > font.h

echo "[kedeicon] building..."
gcc -O2 -Wall -Wextra -o kedeicon kedeicon.c

echo "[kedeicon] installing binary + service (sudo)..."
sudo install -m 0755 kedeicon /usr/local/bin/kedeicon
sudo install -m 0644 kedeicon.service /etc/systemd/system/kedeicon.service
sudo systemctl daemon-reload
sudo systemctl enable kedeicon.service
sudo systemctl restart kedeicon.service
sleep 1
echo "[kedeicon] status:"
sudo systemctl --no-pager --full status kedeicon.service | head -20
