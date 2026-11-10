#!/usr/bin/env bash
# Flash an Anchor or Tag release over the board's USB serial port, through the UART bootloader
# (tools/htn_fota/htn_fw.py flash), then store the board revision (#21). The port is always opened at
# 921600: any other baud can leave a board's UART deaf until it is power-cycled.
#
# Usage: tools/flash_usb.sh PORT anchor|tag --hw REV [VERSION | FILE.bin] [-y] [--force]
#
#   tools/flash_usb.sh /dev/ttyUSB0 anchor --hw C            # latest release in releases/anchor/
#   tools/flash_usb.sh /dev/ttyUSB1 tag --hw D 4.2.0         # releases/tag/4.2.0/Tag.bin
#   tools/flash_usb.sh /dev/ttyUSB0 tag --hw D Tag/Debug/Tag.bin
#
#   --hw REV  board revision, required: C, C2, D or 0-7. Stored in the self-id page once the new
#             firmware runs, it wins over the strap pins (needs anc4.1.0 / tag4.2.0 or later)
#   -y        no confirmation prompt
#   --force   flash even when the board reports the other kind of firmware
#
# The app jumps to the bootloader (opcode 15), which erases bank 2 only, writes the image, checks
# its CRC and starts it. The bootloader is not touched; the new app then stores the revision in its
# self-id page (opcode 27) and reboots on it. A board already in the bootloader (HW=255) is flashed
# directly. The bootloader picks its UART from the strap pins, so a board whose straps read wrong
# cannot be flashed over USB: use tools/flash_swd.sh or FOTA.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
py="$root/tools/htn_fota/.venv/bin/python"
tool="$root/tools/htn_fota/htn_fw.py"
usage() { echo "usage: $0 PORT anchor|tag --hw C|C2|D|0-7 [VERSION | FILE.bin] [-y] [--force]" >&2; exit 2; }

port="${1:-}"; kind="${2:-}"
[ -n "$port" ] || usage
shift 2 || usage
case "$kind" in
	anchor) proj=Anchor; prefix=anc ;;
	tag)    proj=Tag;    prefix=tag ;;
	*) usage ;;
esac

src="" yes=0 force=0 rev=""
while [ $# -gt 0 ]; do
	case "$1" in
		-y|--yes) yes=1 ;;
		--force) force=1 ;;
		--hw) shift; rev="${1:-}" ;;
		-*) usage ;;
		*) src="$1" ;;
	esac
	shift
done
case "${rev^^}" in
	C|C2|D|[0-7]) ;;
	"") echo "--hw is required: the board revision (C, C2, D or 0-7) as printed on the board" >&2; usage ;;
	*) echo "bad --hw '$rev': use C, C2, D or 0-7" >&2; usage ;;
esac

[ -x "$py" ] || { echo "missing $py: python3 -m venv tools/htn_fota/.venv && tools/htn_fota/.venv/bin/pip install -r tools/htn_fota/requirements.txt" >&2; exit 1; }
[ -e "$port" ] || { echo "no such port: $port" >&2; exit 1; }

# image: a .bin path, a release version, or the newest release
if [[ "$src" == *.bin ]]; then
	bin="$src"
else
	if [ -n "$src" ]; then
		dir="$root/releases/$kind/$(sed -E 's/^[a-zA-Z]+//' <<<"$src")"	# anc4.0.2 / tag4.1.1 / 4.1.1
	else
		dir="$(ls -d "$root/releases/$kind"/*/ 2>/dev/null | sort -V | tail -1)"
		[ -n "$dir" ] || { echo "no releases in releases/$kind/ (tools/htn_fota/fw_release.py build $kind)" >&2; exit 1; }
	fi
	bin="${dir%/}/$proj.bin"
fi
[ -f "$bin" ] || { echo "missing $bin" >&2; exit 1; }
new_ver="$(grep -aoE "${prefix}[0-9]+\.[0-9]+\.[0-9]+" "$bin" | head -1 || true)"
[ -n "$new_ver" ] || { echo "$bin carries no ${prefix}x.y.z version: not a $kind image" >&2; exit 1; }
echo "image: $bin ($new_ver)"

# what the board runs now
st="$("$py" "$tool" --port "$port" status 2>&1 || true)"
echo "board: ${st:-no reply}"
sw="$(grep -oE "SW='[^']*'" <<<"$st" | cut -d"'" -f2 || true)"
hw="$(grep -oE 'HW=[0-9]+' <<<"$st" | cut -d= -f2 || true)"
if [ -z "$sw" ]; then
	echo "no STATUS reply on $port: check the cable and that the board is powered (a sleeping tag answers only while awake)" >&2
	exit 1
fi
cur_kind="?"
case "$sw" in anc*) cur_kind=anchor ;; tag*) cur_kind=tag ;; esac
[ "$hw" = 255 ] && cur_kind=bootloader
if [ "$cur_kind" != "$kind" ] && [ "$cur_kind" != bootloader ] && [ "$force" -eq 0 ]; then
	if [ "$cur_kind" = "?" ]; then
		echo "the board reports '$sw', which does not say anchor or tag (firmware before 3.1.0)."
		[ "$yes" -eq 1 ] && { echo "refusing with -y on an unknown board: run without -y, or --force" >&2; exit 1; }
	else
		echo "this board runs $cur_kind firmware ($sw), not $kind: refusing (--force to override)" >&2
		exit 1
	fi
fi

if [ "$yes" -eq 0 ]; then
	read -r -p "flash $new_ver to the board on $port (now $sw) and store board revision $rev? [y/N] " ans
	[[ "$ans" == [yY]* ]] || { echo "aborted"; exit 1; }
fi

"$py" "$tool" --port "$port" flash "$bin" --yes --hw "$rev"
