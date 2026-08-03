#!/bin/sh
# doctor.sh - read-only diagnostics for kedeicon. Changes nothing.
# Run:  sh doctor.sh          (some checks are richer with sudo)
# Paste the output into a GitHub issue when asking for help.

echo "===== kedeicon doctor ====="
echo "date:   $(date)"
echo "model:  $(tr -d '\0' < /proc/device-tree/model 2>/dev/null)"
echo "kernel: $(uname -r) $(uname -m)"
echo "os:     $(. /etc/os-release 2>/dev/null; echo "$PRETTY_NAME")"
echo

echo "--- [1] SPI devices (need spidev0.0 and spidev0.1) ---"
ls -l /dev/spidev* 2>/dev/null || echo "MISSING: no /dev/spidev* -> is dtparam=spi=on set? did you reboot?"
echo

echo "--- [2] conflicting kernel modules (must be EMPTY) ---"
if lsmod | grep -E 'kedei|ads7846|fbtft|ili9486'; then
  echo "PROBLEM: a kernel LCD/touch driver is loaded; it will corrupt the panel."
else
  echo "OK: no kedei/ads7846/fbtft modules loaded"
fi
echo

echo "--- [3] Plymouth (must NOT be running: it steals console keyboard input) ---"
if pgrep -a plymouthd; then
  echo "PROBLEM: plymouthd is running -> typed keys will not echo on the console."
  echo "         fix: sudo sh setup.sh && sudo reboot"
else
  echo "OK: plymouthd not running"
fi
echo "plymouth-start.service: $(systemctl is-enabled plymouth-start.service 2>&1)"
grep -o 'plymouth.enable=0' /proc/cmdline >/dev/null 2>&1 \
  && echo "OK: plymouth.enable=0 present in kernel cmdline" \
  || echo "NOTE: plymouth.enable=0 not in cmdline (masking alone may be enough)"
echo

echo "--- [4] boot config (SPI on, LCD/touch overlays off, HDMI intact) ---"
CONFIG=/boot/firmware/config.txt; [ -f "$CONFIG" ] || CONFIG=/boot/config.txt
grep -nE 'dtparam=spi|dtoverlay=' "$CONFIG" 2>/dev/null | grep -viE '^\s*[0-9]+:#' || echo "(none active)"
echo

echo "--- [5] kedeicon service ---"
echo "active: $(systemctl is-active kedeicon 2>&1)   enabled: $(systemctl is-enabled kedeicon 2>&1)"
journalctl -u kedeicon -o cat --no-pager 2>/dev/null | grep 'kedeicon: dev=' | tail -1 \
  || echo "(no startup line; try: sudo journalctl -u kedeicon -n 20)"
echo

echo "--- [6] console being mirrored ---"
echo "active VT: $(cat /sys/class/tty/tty0/active 2>/dev/null)"
echo "processes on tty1:"; ps -t tty1 -o pid,stat,comm --no-headers 2>/dev/null || echo "(need sudo)"
if [ -r /dev/vcsa1 ]; then
  python3 - <<'PY' 2>/dev/null
d=open('/dev/vcsa1','rb').read(); rows,cols,cx,cy=d[0],d[1],d[2],d[3]; cells=d[4:]
row=''.join(chr(cells[(cy*cols+c)*2]) if 32<=cells[(cy*cols+c)*2]<127 else ' ' for c in range(cols)).rstrip()
print("console: %dx%d  cursor at col=%d row=%d" % (cols,rows,cx,cy))
print("cursor line: |%s|" % row)
PY
else
  echo "(/dev/vcsa1 not readable - run with sudo to see console state)"
fi
echo

echo "--- [7] keyboards seen by the kernel ---"
awk '/Name=/{n=$0} /Handlers=/{if ($0 ~ /kbd/) print n}' /proc/bus/input/devices 2>/dev/null | sed 's/N: Name=//' \
  || echo "(none)"
echo
echo "===== end ====="
