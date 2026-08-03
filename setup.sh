#!/bin/sh
# setup.sh - prepare a Raspberry Pi for kedeicon (KeDei 3.5" v5.0 console mirror).
#
# Idempotent. Makes timestamped backups and prints a diff of every file it edits.
# Run:  sudo sh setup.sh          (then reboot, then: sh deploy.sh)
#
# What it does, and why:
#   1. spidev-clean SPI bus  - enables dtparam=spi=on and comments out any kernel
#      LCD/touch overlay (kedei*, fbtft, ili9486, ads7846). The panel is driven from
#      USERSPACE; a kernel driver on the same bus corrupts the image, and the ads7846
#      touch driver shares SCLK with the panel's 74HC595 chain and garbles the stream.
#   2. disables Plymouth     - the boot splash can stay attached to the console after
#      boot and SWALLOW KEYBOARD INPUT: you get a blinking cursor that never moves and
#      typed characters never echo, even though commands still run. See README.
#   It never touches vc4-kms-v3d (HDMI keeps working).

set -e
[ "$(id -u)" = "0" ] || { echo "run as root: sudo sh $0"; exit 1; }

CONFIG=/boot/firmware/config.txt
[ -f "$CONFIG" ] || CONFIG=/boot/config.txt
CMDLINE=/boot/firmware/cmdline.txt
[ -f "$CMDLINE" ] || CMDLINE=/boot/cmdline.txt
STAMP=$(date +%Y%m%d-%H%M%S)

echo "=== kedeicon setup ==="
echo "config:  $CONFIG"
echo "cmdline: $CMDLINE"
echo

# ---------------------------------------------------------------- 1. SPI bus
cp -a "$CONFIG" "$CONFIG.bak.kedeicon.$STAMP"

# Comment out kernel LCD/touch overlays that would fight for the SPI bus.
sed -i -E 's/^[[:space:]]*(dtoverlay=.*(kedei|fbtft|ili9486|ads7846).*)$/#\1  # disabled by kedeicon setup/I' "$CONFIG"

# Ensure SPI is on (uncomment an existing line, or append one).
if grep -qE '^[[:space:]]*dtparam=spi=on' "$CONFIG"; then
  :
elif grep -qE '^[[:space:]]*#[[:space:]]*dtparam=spi=on' "$CONFIG"; then
  sed -i -E '0,/^[[:space:]]*#[[:space:]]*dtparam=spi=on/s//dtparam=spi=on/' "$CONFIG"
else
  printf '\n# enabled by kedeicon setup\ndtparam=spi=on\n' >> "$CONFIG"
fi

echo "--- config.txt diff ---"
diff "$CONFIG.bak.kedeicon.$STAMP" "$CONFIG" || true
echo "(backup: $CONFIG.bak.kedeicon.$STAMP)"
echo

# ---------------------------------------------------------- 2. kill Plymouth
echo "--- disabling Plymouth (it steals console keyboard input) ---"
plymouth quit 2>/dev/null || true
pkill -TERM plymouthd 2>/dev/null || true
for u in plymouth-start plymouth-quit plymouth-quit-wait plymouth-read-write \
         plymouth-switch-root plymouth-switch-root-initramfs \
         plymouth-halt plymouth-reboot plymouth-poweroff plymouth-kexec; do
  systemctl mask "$u.service" >/dev/null 2>&1 || true
done
echo "masked all plymouth units"

cp -a "$CMDLINE" "$CMDLINE.bak.kedeicon.$STAMP"
# cmdline.txt MUST stay a single line: edit line 1 in place, never append a newline.
sed -i -e 's/[[:space:]]*\bsplash\b//g' -e 's/[[:space:]]*\bquiet\b//g' "$CMDLINE"
grep -q 'plymouth.enable=0' "$CMDLINE" || sed -i '1s/[[:space:]]*$//;1s/$/ plymouth.enable=0/' "$CMDLINE"

echo "--- cmdline.txt diff ---"
diff "$CMDLINE.bak.kedeicon.$STAMP" "$CMDLINE" || true
echo "(backup: $CMDLINE.bak.kedeicon.$STAMP)"
echo

# ------------------------------------------------------------------ 3. report
echo "=== current state ==="
ls /dev/spidev* 2>/dev/null || echo "/dev/spidev*: none yet (appears after reboot)"
lsmod | grep -E 'kedei|ads7846|fbtft' || echo "no conflicting kernel modules loaded"
echo
echo "=== NEXT STEPS ==="
echo "  1. sudo reboot"
echo "  2. sh doctor.sh          # verify: spidev nodes, no plymouth, clean bus"
echo "  3. gcc -O2 -o kedei_test kedei_test.c && sudo ./kedei_test   # panel proof-of-life"
echo "  4. sh deploy.sh          # build + install + start the daemon"
