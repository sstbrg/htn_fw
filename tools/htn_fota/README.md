# htn_fota: HITAN firmware tools

Update anchors and tags over the air (FOTA over UWB), flash them over USB,
and build and archive firmware releases.

| Tool | What it does |
|---|---|
| `fw_wizard.py` | Step-by-step GUI for over-the-air updates. Start here. |
| `htn_fw.py` | Command line: board status, USB flashing, FOTA, discovery. |
| `fw_release.py` | Bump the version, build, and save releases; download GitHub releases. |

## Setup

Linux (tested on Ubuntu 24.04). Run from the repository root:

```bash
sudo apt install python3-venv python3-tk
python3 -m venv tools/htn_fota/.venv
tools/htn_fota/.venv/bin/pip install -r tools/htn_fota/requirements.txt
```

Your user must be allowed to open serial ports (member of the `dialout` group).

To build firmware you also need the ARM toolchain:

```bash
sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib
```

All commands below run from the repository root. `PY` is short for the venv's Python:

```bash
PY=tools/htn_fota/.venv/bin/python
```

## Update a board over the air (GUI)

```bash
$PY tools/htn_fota/fw_wizard.py
```

You need one board on USB to send the firmware (the **FOTA loader**). It can
be any tag or anchor running 3.1.0 or newer. The board you update (the
**target**) needs no cable.

1. **Connect FOTA loader.** Plug the loader into USB and press **Scan USB**.
   Pick it in the list. A battery-powered loader needs at least 30 %.
2. **FOTA loader mode.** Press **Enable FOTA loader mode**. The loader stops its
   normal work and its status LED blinks fast (on anchors: a battery-bar LED).
3. **Choose firmware.** Pick a saved release, press **Download latest from
   GitHub**, **Build new release...**, or **Browse .bin...**. In **Build new
   release...**, tick **Also publish on GitHub** to commit the version bump,
   push, and have CI publish the release (see Releases below).
4. **Find the target.** Press **Scan for targets**. Every board in UWB range
   answers with its id, type, firmware and battery. Pick the one to update.
   Boards on 3.1.0 don't answer a scan: type their id and press **Connect**.
5. **Flash and verify.** Press **Start update** and wait for **Update OK**.
   Then press **Leave FOTA loader mode** so the loader goes back to normal work.

An update of about 120 KB takes roughly 12 seconds: 5 s countdown, under 1 s
of data, then the target writes flash, reboots, and the loader checks it.

### What protects the target

- The target keeps running its current firmware while the image arrives. It
  stores the image in RAM and writes flash only after the whole image has
  arrived and its CRC matches.
- If the radio link drops, the target gives up after 6 s and carries on with
  its old firmware. Just retry.
- Every chunk is acknowledged; lost chunks are resent.
- After writing, the target reads the flash back and rewrites it (up to 3
  times) if anything differs. The bootloader checks the CRC again at boot.
- The loader asks the rebooted target for its CRC and reports **Update OK**
  only if it matches the image.
- A target or loader with a battery gauge refuses below 30 %. Tags always
  have one; an anchor reports its gauge if a battery pack is wired to it.
  Boards with no gauge (USB-powered anchors) report 255, shown as "no gauge",
  and are always allowed.

The one unrecoverable-over-the-air case is losing power during the few seconds
of the flash write. The board then waits in its bootloader; flash it over USB
(`htn_fw.py flash`) or with an ST-Link.

### Data rate

The handshake always uses the normal 110 kbps UWB setting that every board
listens on. The data phase starts at 6.8 Mbps (fast, a few metres) and, if the
link drops, retries at 850 kbps and then 110 kbps (slow, long range). The
wizard's **Data rate** box can start lower.

### LEDs

Two LEDs per board show FOTA. On a tag: the **status** LED and the **link-up**
LED. On an anchor (rev D has no status/link LEDs): **battery-bar LED 0**
(PE14) as status and **battery-bar LED 3** (PF15) as link.

| Phase | Board | Status LED | Link LED |
|---|---|---|---|
| FOTA loader mode | loader | fast blink | off |
| Image loading from the PC | loader | fast blink | fast blink |
| Sending over UWB | loader | fast blink | fast blink |
| Handshake accepted, 5 s countdown | target | slow blink | off |
| Receiving over UWB (under 1 s at 6.8 Mbps) | target | slow blink | fast blink |
| Writing flash | target | on | on |
| Rebooted into the new firmware | target | off | off |

`htn_fw.py led-test` lights every LED a board can drive, one at a time, to see
which ones are fitted.

### Power

The target must be powered. Rev D anchors run from USB power only (a charger
or power bank is enough; no data connection needed); their battery input does
not supply the board. Tags run from their battery.

After an update, or when a session is aborted, both boards return to the
normal ranging UWB settings; the fast data-rate settings only exist during a
transfer.

## Command line

```bash
$PY tools/htn_fota/htn_fw.py --port /dev/ttyUSB0 status
$PY tools/htn_fota/htn_fw.py --port /dev/ttyUSB0 discover          # boards in UWB range
$PY tools/htn_fota/htn_fw.py --port /dev/ttyUSB0 query 6           # one board's version and CRC
$PY tools/htn_fota/htn_fw.py --port /dev/ttyUSB0 fota 6 releases/anchor/3.1.4/Anchor.bin
$PY tools/htn_fota/htn_fw.py --port /dev/ttyUSB0 fota-exit         # leave FOTA loader mode
$PY tools/htn_fota/htn_fw.py --port /dev/ttyUSB0 flash releases/tag/3.1.3/Tag.bin   # over USB
$PY tools/htn_fota/htn_fw.py --port /dev/ttyUSB1 led-test          # which LEDs are fitted
```

`--port` is the loader (for `fota`, `discover`, `query`) or the board itself
(for `status`, `flash`). Stable port names are under `/dev/serial/by-id/`.
`fota` options: `--rate 6m8|850k|110k` (starting rate), `--stay` (keep the
loader in FOTA mode).

`flash` uses the board's USB bootloader and works on any firmware version. It
is how the first FOTA-capable firmware gets onto a board, and how the loader
itself is updated.

## Releases

```bash
$PY tools/htn_fota/fw_release.py list
$PY tools/htn_fota/fw_release.py new anchor --part patch --note "what changed"   # bump + build + save
$PY tools/htn_fota/fw_release.py build tag                                        # build + save, no bump
$PY tools/htn_fota/fw_release.py fetch                                            # latest GitHub release
$PY tools/htn_fota/fw_release.py publish anchor                                   # commit bump, push, CI publishes
```

A release is saved under `releases/<anchor|tag>/<version>/` with the `.bin`,
`.hex`, `.elf`, `.map` and a `manifest.json` (version, bootloader CRC, size,
sha256, git commit, toolchain). `releases/` is not in git.

On GitHub, every pull request to `main` builds both boards and fails if Anchor
or Tag code changed without a version bump. Merging creates a release
`fw-anc<ver>-tag<ver>` with the images; `fetch` and the wizard download it.

`publish` (and the wizard's **Also publish on GitHub**) releases what is
committed, never a local build: it refuses if other firmware changes are
uncommitted, commits the version bump as `release: <version>`, and pushes. On
`main` the push publishes the release; on any other branch it starts the CI
workflow by hand and the release is a pre-release. Starting the workflow by
hand only works once `.github/workflows/firmware.yml` is on `main`.

Building by hand: `tools/build.sh Anchor`, `tools/build.sh Tag`,
`tools/build.sh bootloader_hitan` (outputs in `<Project>/Debug/`).

## Troubleshooting

| Symptom | What to do |
|---|---|
| Port missing, or `error -71` in `journalctl -k` | Replug the USB cable or try another one. |
| `no STATUS reply` | Board off, asleep, or a bad cable. Tags sleep when idle: wake or power-cycle them. |
| Board stopped answering on USB after a tool was opened at a wrong baud | Power-cycle it. These tools always use 921600; never open the port at another baud. |
| Scan finds nothing | Target firmware older than 3.1.1, a tag asleep, or out of range. Try typing the id. |
| `link lost` | Move the boards closer, or start at a lower data rate. |
| `battery too low` | Charge the loader or the target above 30 %. |
| `wrong device kind` | Anchor firmware picked for a tag or the reverse. |
| Board stuck in the bootloader (`status` shows HW=255) | `htn_fw.py flash <image>` over USB. |

## Firmware side

The protocol and the target/loader logic are in `Anchor/Core/Src/fota.c` and
`Anchor/Core/Inc/fota.h` (identical copies in `Tag/`); board specifics (id,
LEDs, battery, UWB restore) are in each project's `fota_port.cpp`.

| UART opcode | Purpose |
|---|---|
| 0x40 / 0x47 | enter / leave FOTA loader mode |
| 0x41, 0x42 | load the image into the loader's RAM |
| 0x43, 0x44, 0x46 | start, status, abort the UWB session |
| 0x45 | query one target |
| 0x48, 0x49 | discover targets, read the list |
| 0x4B | LED test |
| 0x4F | DW1000 SPI self-test |

A tag stays awake while it counts down as a target. Minimum versions: FOTA
target and loader 3.1.0, discovery 3.1.1, battery check on anchors 3.1.2.

LoRa wake-up for sleeping tags is not implemented yet: the wizard skips it, so
a tag has to be awake (on USB, or ranging) to be updated.
