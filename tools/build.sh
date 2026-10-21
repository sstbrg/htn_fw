#!/usr/bin/env bash
# Build a firmware project from the tracked Debug makefiles (Linux, arm-none-eabi-gcc).
#
# Usage: tools/build.sh Anchor|Tag|bootloader_hitan [make args...]
#
#   tools/build.sh Anchor             # -> Anchor/Debug/Anchor.{elf,hex,bin}, app at 0x08200000
#   tools/build.sh Tag                # -> Tag/Debug/Tag.{elf,hex,bin}, app at 0x08200000
#   tools/build.sh bootloader_hitan   # -> bootloader_hitan/Debug/bootloader_hitan.{elf,hex,bin}, at 0x08000000
#
# Extra arguments go to make, e.g. `tools/build.sh Tag clean`.
#
# STM32CubeIDE writes Debug/objects.list itself; outside the IDE it is
# regenerated here from $(OBJS) so newly added sources get linked.
set -euo pipefail

proj="${1:?usage: $0 Anchor|Tag|bootloader_hitan [make args...]}"
shift
dir="$(cd "$(dirname "$0")/.." && pwd)/$proj/Debug"

make -C "$dir" --no-print-directory \
	--eval='print-objs: ; @printf "\"%s\"\n" $(OBJS)' print-objs > "$dir/objects.list"
make -C "$dir" -j"$(nproc)" "${@:-all}"
