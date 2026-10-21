#!/usr/bin/env bash
# Build a firmware project from the tracked Debug makefiles (Linux, arm-none-eabi-gcc).
#
# Usage: tools/build.sh Anchor|Tag|bootloader_hitan [make args...]
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
