# kedeicon — KeDei 3.5" v5.0 console mirror (userspace)

Userspace C daemon that mirrors the Linux text console (**tty1**) onto a
**KeDei 3.5" v5.0** TFT (ILI9486 clone behind three 74HC595) over
`/dev/spidev0.1`, using the proven `kedei_test.c` SPI protocol. No kernel driver.

## Why userspace (not a kernel driver)

A tiny in-kernel DRM driver (`kedei35fb`) was tried first. The panel inits and
small updates render fine, but **full-frame writes corrupt into noise** — a
bcm2835-SPI problem with thousands of tiny transfers issued from kernel context.
The *same* protocol from userspace (`spidev` + `ioctl`) paints cleanly. So this
is 100% userspace. **Do not revive the kernel driver.**

## Result

- Landscape, **80×26 character grid** (Terminus 6×12) — the full 80×25 console fits.
- VGA 16-color attributes (e.g. the shell prompt shows green).
- Partial updates: only changed cells are re-sent; the initial black clear is the
  only full-frame write. Idle CPU is negligible.
- Runs as a systemd service, enabled at boot.

## Hardware / addressing notes

- The panel is driven through three 74HC595 shift registers — a **non-standard**
  protocol (not raw ILI9486). Each 2/3-byte group is one `SPI_IOC_MESSAGE`; CS
  pulses per group. **Never batch** the transfers.
- The controller's **clean native canvas is 480(col)×320(page)** — exactly what
  `kedei_test.c` fills without corruption. Addressing it as 320×480 leaves 160
  columns unwritten (a **noise band**) and rotates/compresses content. So the
  physical framebuffer is always **480×320** and portrait reading (if wanted) is
  done by **rotating the text in software** (`KEDEI_ROT`). MADCTL stays `0xEA`.
- The controller ignores the MADCTL transpose bit, so orientation is a software
  concern here, not a hardware one.

## Files

| File | Purpose |
|------|---------|
| `setup.sh` | One-shot system prep: spidev-clean SPI bus + disables Plymouth. Idempotent, backups + diffs. |
| `doctor.sh` | Read-only diagnostics (7 checks, verdict per item). Paste its output into issues. |
| `kedeicon.c` | The daemon. SPI framing copied verbatim from `kedei_test.c`. |
| `genfont.py` | Extracts a console PSF font (width ≤ 8) into `font.h`. No hand-authored glyphs. Defaults to `Lat15-Terminus12x6` (6×12 → 80 cols); pass a font name to override. |
| `font.h` | **Generated** on the Pi by `genfont.py` (`#define FONT_W/FONT_H` + `fontdata[256][H]`). Not committed; regenerated at build. |
| `kedeicon.service` | systemd unit (After=multi-user.target, Restart=on-failure). |
| `Makefile` | `make` / `make install`. |
| `deploy.sh` | Build + install + enable + restart on the Pi. |

## Tunables (systemd `Environment=`, no rebuild)

| Var | Default | Meaning |
|-----|---------|---------|
| `KEDEI_DEV` | `/dev/spidev0.1` | SPI node (LCD is CE1). |
| `KEDEI_ROT` | `0` | Orientation. `0`/`2` = landscape (80×26); `1`/`3` = portrait (53×40). Odd/even pairs are 180° flips of each other. |
| `KEDEI_INTERVAL_MS` | `40` | Console poll period (clamped to ≥16). 40 ms ≈ 25 Hz gives live-feeling echo while typing. |
| `KEDEI_VCSA` | `/dev/vcsa1` | Console source (tty1). `vcsaN` = ttyN. |
| `KEDEI_COLOR` | `1` | `1` = VGA attr colors, `0` = mono white-on-black. |
| `KEDEI_FULL_REFRESH_S` | `60` | Periodic `lcd_init()` + full repaint, so a desynced panel recovers by itself (diff-only drawing would otherwise never repaint it). `0` disables. `SIGUSR1` forces one: `sudo pkill -USR1 kedeicon`. Costs ~2% CPU at 60 s. |

Change orientation live:
```bash
sudo sed -i 's/^Environment=KEDEI_ROT=.*/Environment=KEDEI_ROT=2/' /etc/systemd/system/kedeicon.service
sudo systemctl daemon-reload && sudo systemctl restart kedeicon
```

## Prerequisites (spidev-clean mode)

`config.txt` must expose a clean SPI bus for the LCD only:
- `dtparam=spi=on` **enabled**.
- `dtoverlay=kedei35-lcdonly` / `kedei35-drm` **commented out** (no kernel LCD driver).
- `ads7846` overlay **commented out** — the XPT2046 touch shares SCLK with the
  74HC595 and corrupts the LCD stream if its driver runs in parallel. (Touch is a
  future add-on read directly from `/dev/spidev0.0` inside this same daemon.)
- `dtoverlay=vc4-kms-v3d` left intact (HDMI unaffected).

Verify: `ls /dev/spidev*` → `0.0` and `0.1`; `lsmod | grep -E 'kedei35fb|ads7846'` → empty.

## Prerequisite #2: Plymouth must be dead

On a Raspberry Pi OS image, `plymouthd` (the boot splash) can survive boot as
`plymouthd --mode=boot --attach-to-session` and **keep consuming console keyboard
input**. The failure looks exactly like a display bug and wastes hours:

- typed characters never echo next to the prompt,
- the cursor blinks but never advances,
- **yet commands still execute** (bash receives some of the input),
- writes *to* the console (command output, `printf > /dev/tty1`) display fine.

How we proved it was not the daemon: sampling `/dev/vcsa1` (the console's own text
buffer) every 80 ms for two minutes while typing showed **zero changes** — the
cursor never moved in the console itself. The panel was faithfully mirroring a
console that genuinely had nothing new to show. Killing `plymouthd` fixed it.

`setup.sh` masks every `plymouth-*.service`, strips `quiet`/`splash` from
`cmdline.txt`, and appends `plymouth.enable=0` (kernel-level kill switch).
`doctor.sh` section 3 checks all three.

## Build & deploy

From this folder on the Pi:
```bash
sh deploy.sh
```
Or manually:
```bash
python3 genfont.py > font.h
make
sudo make install
sudo systemctl enable --now kedeicon
```

Sanity-check the panel first with the userspace tester (`kedei_test.c`):
paints RED→BLUE→GREEN if the bus and panel are healthy.

## Troubleshooting

- **Noise band / rotated text** → wrong addressing; ensure physical 480×320 (this
  is baked in) and a valid `KEDEI_ROT`.
- **Blank panel** → check `journalctl -u kedeicon`; confirm `/dev/spidev0.1` exists
  and no `kedei35fb`/`ads7846` modules are loaded.
- **Text cut at 60/40 cols** → font fell back to 8×16; confirm
  `genfont.py: used ...Terminus12x6 (6x12)` in the build log.

## Follow-up (not in v1)

- Touch: read XPT2046 on `/dev/spidev0.0` inside the same loop (bus is exclusive,
  no `ads7846`), expose via uinput or log coordinates.
