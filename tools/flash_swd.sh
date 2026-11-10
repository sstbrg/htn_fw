#!/usr/bin/env bash
# Flash an Anchor or Tag release over ST-Link SWD (st-flash, stlink-tools >= 1.8.0) and store the
# board revision (#21).
#
# Usage: tools/flash_swd.sh anchor|tag --hw REV [VERSION | FILE.hex] [-y] [--under-reset] [--force]
#
#   tools/flash_swd.sh tag --hw D                    # latest release in releases/tag/
#   tools/flash_swd.sh anchor --hw C 4.1.0           # releases/anchor/4.1.0/Anchor.hex
#   tools/flash_swd.sh tag --hw D Tag/Debug/Tag.hex  # a local build
#
#   --hw REV        board revision, required: C, C2, D or 0-7. Stored in the self-id page, it wins over
#                   the strap pins (anc4.1.0 / tag4.2.0 and later; older firmware ignores it)
#   -y              no confirmation prompt
#   --under-reset   connect under reset (NRST wired); for a sleeping or unresponsive board
#   --force         flash even when the board runs the other kind of firmware
#
# Only the .hex is written for the app: it carries its addresses (app at 0x08200000, flash bank 2).
# A .bin is refused. The revision goes into slot 4 of the self-id page (bank 1, page 255 at
# 0x081FE000, next to the id, antenna delays and crystal trim): its 16 bytes are read, slot 4
# replaced, written back and compared. The bootloader (bank 1 from 0x08000000) is never touched.
# A sleeping tag drops its debug port: power-cycle it and run this within its first seconds awake,
# or use --under-reset.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
usage() { echo "usage: $0 anchor|tag --hw C|C2|D|0-7 [VERSION | FILE.hex] [-y] [--under-reset] [--force]" >&2; exit 2; }
ID_PAGE=0x081FE000	# self-id page: 8 x u16 slots, slot 4 = board revision

kind="${1:-}"; shift || true
case "$kind" in
	anchor) proj=Anchor ;;
	tag)    proj=Tag ;;
	*) usage ;;
esac

src="" yes=0 under_reset=() force=0 rev=""
while [ $# -gt 0 ]; do
	case "$1" in
		-y|--yes) yes=1 ;;
		--under-reset) under_reset=(--connect-under-reset) ;;
		--force) force=1 ;;
		--hw) shift; rev="${1:-}" ;;
		-*) usage ;;
		*) src="$1" ;;
	esac
	shift
done
case "${rev^^}" in
	C) rev=0 ;; C2) rev=1 ;; D) rev=2 ;;
	[0-7]) ;;
	"") echo "--hw is required: the board revision (C, C2, D or 0-7) as printed on the board" >&2; usage ;;
	*) echo "bad --hw '$rev': use C, C2, D or 0-7" >&2; usage ;;
esac

# image: a .hex path, a release version, or the newest release
if [[ "$src" == *.hex ]]; then
	hex="$src"
elif [[ "$src" == *.bin ]]; then
	echo "refusing a .bin: it has no addresses (written at 0x08000000 it would overwrite the bootloader); use the .hex" >&2
	exit 1
else
	if [ -n "$src" ]; then
		dir="$root/releases/$kind/$(sed -E 's/^[a-zA-Z]+//' <<<"$src")"	# anc4.0.2 / tag4.1.1 / 4.1.1
	else
		dir="$(ls -d "$root/releases/$kind"/*/ 2>/dev/null | sort -V | tail -1)"
		[ -n "$dir" ] || { echo "no releases in releases/$kind/ (tools/htn_fota/fw_release.py build $kind)" >&2; exit 1; }
	fi
	hex="${dir%/}/$proj.hex"
fi
[ -f "$hex" ] || { echo "missing $hex" >&2; exit 1; }

# the image must start in bank 2 (extended linear address 0x0820)
first_ela="$(grep -m1 '^:02000004' "$hex" | cut -c10-13)"
[ "$first_ela" = "0820" ] || { echo "$hex does not start at 0x08200000 (address record $first_ela): not an app image" >&2; exit 1; }
new_ver="$(grep -aoE '"version": *"[^"]+"' "$(dirname "$hex")/manifest.json" 2>/dev/null | cut -d'"' -f4 || true)"
echo "image: $hex${new_ver:+ ($new_ver)}; board revision $rev"

command -v openocd >/dev/null || { echo "openocd is needed to read the board and store the revision" >&2; exit 1; }
ocd() { timeout 60 openocd -f interface/stlink.cfg -f target/stm32u5x.cfg -c init "$@" -c shutdown >/dev/null 2>&1; }

# target present?
probe="$(st-info --probe "${under_reset[@]}" 2>&1 || true)"
if ! grep -q "chipid: *0x481" <<<"$probe"; then
	echo "no STM32U5A5 on SWD (chip id $(grep -oE 'chipid: *0x[0-9a-f]+' <<<"$probe" | awk '{print $2}'))." >&2
	echo "Check power and the SWD cable; a sleeping tag needs a power cycle first, or --under-reset." >&2
	exit 1
fi

# what runs on it now (read-only: OpenOCD reads flash without halting or resetting)
tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
cur_kind="?" cur_ver="?"
if ocd -c "dump_image $tmp/app.bin 0x08200000 0x30000"; then
	grep -aq "Tag Version" "$tmp/app.bin" && cur_kind=tag
	grep -aq "Anchor Version" "$tmp/app.bin" && cur_kind=anchor
	cur_ver="$(grep -aoE '(anc|tag|sC|NC|C)[0-9]+\.[0-9]+\.[0-9]+' "$tmp/app.bin" | head -1 || true)"
	[ -n "$cur_ver" ] || cur_ver="?"
	[ "$(LC_ALL=C tr -d '\377' < "$tmp/app.bin" | wc -c)" -eq 0 ] && cur_kind=empty cur_ver=-
fi
ocd -c "dump_image $tmp/id.bin $ID_PAGE 16" || { echo "cannot read the self-id page" >&2; exit 1; }
board_id="$(od -An -tu2 -N2 "$tmp/id.bin" | tr -d ' ')"
cur_rev="$(od -An -tu2 -j8 -N2 "$tmp/id.bin" | tr -d ' ')"
[ "$cur_rev" -le 7 ] 2>/dev/null || cur_rev="none"
echo "board: id $board_id, $cur_kind firmware $cur_ver, stored revision $cur_rev"
if [ "$cur_kind" != "$kind" ] && [ "$cur_kind" != "?" ] && [ "$cur_kind" != empty ] && [ "$force" -eq 0 ]; then
	echo "this board runs $cur_kind firmware, not $kind: refusing (--force to override)" >&2
	exit 1
fi

if [ "$yes" -eq 0 ]; then
	read -r -p "flash $proj${new_ver:+ $new_ver} and store board revision $rev over SWD? [y/N] " ans
	[[ "$ans" == [yY]* ]] || { echo "aborted"; exit 1; }
fi

st-flash "${under_reset[@]}" --format ihex write "$hex"

# board revision: self-id page slot 4 (bytes 8-9), the other slots kept
if [ "$cur_rev" != "$rev" ]; then
	cp "$tmp/id.bin" "$tmp/id_new.bin"
	printf "\\x$(printf %02x "$rev")\\x00" | dd of="$tmp/id_new.bin" bs=1 seek=8 conv=notrunc status=none
	st-flash write "$tmp/id_new.bin" "$ID_PAGE"
	ocd -c "dump_image $tmp/id_check.bin $ID_PAGE 16" && cmp -s "$tmp/id_new.bin" "$tmp/id_check.bin" \
		|| { echo "the self-id page does not read back as written: check it before resetting" >&2; exit 1; }
	echo "board revision $rev stored (self-id page slot 4; id $board_id and the other slots kept)"
else
	echo "board revision $rev already stored"
fi

st-flash reset
echo "done: $proj${new_ver:+ $new_ver} written and verified, board revision $rev, board reset"
