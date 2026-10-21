#!/usr/bin/env bash
# Flash the Anchor firmware (STM32U5A5ZJTxQ) over ST-Link SWD.
#
# Usage:
#   ./flash_anchor.sh              # flash Debug/Anchor.hex via st-flash (address from hex)
#   ./flash_anchor.sh reset        # same, but connect-under-reset (requires NRST wired)
#   ./flash_anchor.sh probe        # just probe ST-Link + target, no flashing
#   ./flash_anchor.sh cubeprog     # fallback via STM32_Programmer_CLI (if installed)
#
# Target: STM32U5A5ZJ (chip id 0x481, Cortex-M33, TrustZone disabled).
#
# MEMORY LAYOUT — IMPORTANT:
#   The app is linked at 0x08200000 (flash BANK 2, 2040K = 255 x 8K pages) per
#   STM32U5A5ZJTXQ_FLASH_MMT_TEMPLATE.ld — NOT at 0x08000000. Bank 1 holds the
#   OTU bootloader. Flashing the .hex uses the embedded addresses, so bank 1
#   (bootloader) is untouched. Do NOT write Anchor.bin to 0x08000000.
#   A factory-blank chip needs the bootloader in bank 1 first, or the boot
#   address option byte pointed at 0x08200000 — the app alone will not boot.
#
# Tool support notes (verified 2026-07-22):
#   - stlink-tools 1.8.0 supports U5A5 non-secure flash (chips/U59x_U5Ax.chip).
#   - Ubuntu OpenOCD 0.12.0 does NOT (flash driver lacks dev id 0x481) — don't bother.
#   - STM32CubeProgrammer is the full-support fallback (option bytes, TrustZone).
set -euo pipefail

cd "$(dirname "$0")"

HEX=Debug/Anchor.hex
ELF=Debug/Anchor.elf
APP_BASE=0x08200000   # bank 2 — from STM32U5A5ZJTXQ_FLASH_MMT_TEMPLATE.ld

mode="${1:-flash}"

case "$mode" in
  probe)
    st-info --probe
    ;;
  flash)
    [ -f "$HEX" ] || { echo "missing $HEX — build first (cd Debug && make)"; exit 1; }
    st-flash --format ihex write "$HEX"
    st-flash reset
    ;;
  reset)
    [ -f "$HEX" ] || { echo "missing $HEX — build first (cd Debug && make)"; exit 1; }
    st-flash --connect-under-reset --format ihex write "$HEX"
    st-flash reset
    ;;
  cubeprog)
    [ -f "$ELF" ] || { echo "missing $ELF — build first (cd Debug && make)"; exit 1; }
    STM32_Programmer_CLI -c port=swd mode=UR -w "$ELF" -v -rst
    ;;
  *)
    echo "usage: $0 [flash|reset|probe|cubeprog]"; exit 2
    ;;
esac
