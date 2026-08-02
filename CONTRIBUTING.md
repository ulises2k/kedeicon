# Contributing to kedeicon

Thanks for pitching in! This project exists to make the KeDei 3.5" v5.0 usable, so
bug reports, panel-variant confirmations, and small improvements are all welcome.

## Reporting a problem

Open an issue using one of the templates. Before you do, please:

1. Confirm spidev-clean mode: `ls /dev/spidev*` shows `0.0` and `0.1`, and
   `lsmod | grep -E 'kedei|ads7846|fbtft'` prints nothing.
2. Run the proof-of-life tester: `gcc -O2 -o kedei_test kedei_test.c && sudo ./kedei_test`.
   If it doesn't fill RED → BLUE → GREEN, it's a wiring/config/bus issue, not the daemon.
3. Include `journalctl -u kedeicon` output and a photo of the panel if you can.

## Development

- Pure C + POSIX, no external libs. Build with `make` (needs `python3` to bake the
  font from a system console PSF via `genfont.py`).
- Keep the SPI framing in `kedeicon.c` byte-for-byte identical to `kedei_test.c` —
  it is the reverse-engineered 74HC595 protocol and must not be "optimized".
- The physical canvas is always 480×320; orientation is handled in software
  (`KEDEI_ROT`). Don't reintroduce 320×480 addressing (it causes the noise band).
- Test on real hardware before opening a PR, and describe what you saw on the panel.

## Ideas that would help

- Touch (XPT2046 on `/dev/spidev0.0`) with calibration + `uinput`.
- Confirmations for other KeDei sizes/revisions — please share the PCB marking and
  what settings worked.
- A status-dashboard render mode (IP / load / temp / uptime).

## Code of conduct

Be kind and constructive. We're all here to make a stubborn little screen light up.
