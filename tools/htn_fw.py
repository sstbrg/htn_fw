#!/usr/bin/env python3
"""HITAN firmware tool: status, UART bootloader flashing, FOTA over UWB.

Every port is opened at 921600 8N1 (the only baud the boards accept; a wrong
baud can leave a board's UART deaf until power-cycled).

  htn_fw.py --port /dev/ttyUSB0 status
  htn_fw.py --port /dev/ttyUSB0 flash Tag/Debug/Tag.bin          # UART bootloader
  htn_fw.py --port /dev/ttyUSB0 fota 6 Anchor/Debug/Anchor.bin   # this board flashes UWB target 6
  htn_fw.py --port /dev/ttyUSB0 discover                         # list FOTA targets in UWB range
  htn_fw.py --port /dev/ttyUSB0 query 6                          # ask target 6 for its version/CRC
  htn_fw.py --port /dev/ttyUSB0 fota-exit                        # leave FOTA flasher mode

UART frame: [opcode, length, payload..., checksum], length = whole frame,
checksum = sum(frame[:-1]) & 0xFF.

Bootloader (bootloader_hitan): 11 STATUS, 16 START_PROG (erase bank 2),
17 WRITE_BUFFER (128 B), 18 HEADER_FILE (crc, len), 10 RESET.
Apps: 11 STATUS, 15 UPDATE_FW (jump to bootloader), 0x40-0x47 FOTA (fota.h).
Image CRC: STM32 CRC-32 (poly 0x04C11DB7, init 0xFFFFFFFF, no reflection),
byte-fed, over the image padded with 0xFF to 255 pages (0x1FE000 bytes).
"""
import argparse
import struct
import sys
import time

import serial

BAUD = 921600
OP_RESET, OP_STATUS, OP_UPDATE_FW = 10, 11, 15
OP_START_PROG, OP_WRITE_BUFFER, OP_HEADER_FILE, OP_ERROR = 16, 17, 18, 0xEE
OP_FOTA_ENTER, OP_FOTA_LOAD_BEGIN, OP_FOTA_LOAD_DATA, OP_FOTA_START = 0x40, 0x41, 0x42, 0x43
OP_FOTA_STATUS, OP_FOTA_QUERY, OP_FOTA_ABORT, OP_FOTA_EXIT = 0x44, 0x45, 0x46, 0x47
OP_FOTA_DISCOVER, OP_FOTA_DISCOVER_LIST = 0x48, 0x49
CHUNK = 128
CRC_LEN = 255 * 8192
APP_BASE = 0x08200000

RATES = {"110k": 0, "850k": 1, "6m8": 2}
FOTA_ERR = ["ok", "busy", "wrong device kind", "bad size", "bad session", "CRC mismatch",
            "bad image (vector table)", "bad state", "no target answered", "link lost",
            "verify failed (target CRC differs)", "aborted", "bad rate", "battery too low"]
FL_STATE = ["idle", "loading", "loaded", "hello", "countdown", "data", "commit",
            "verify-wait", "verify", "query", "discover", "done", "failed"]
RATE_NAME = {0: "110k", 1: "850k", 2: "6m8"}


def frame(opcode, payload=b""):
    body = bytes([opcode, len(payload) + 3]) + payload
    return body + bytes([sum(body) & 0xFF])


def crc32_mpeg2(data):
    table = []
    for i in range(256):
        c = i << 24
        for _ in range(8):
            c = ((c << 1) ^ 0x04C11DB7) if c & 0x80000000 else (c << 1)
        table.append(c & 0xFFFFFFFF)
    crc = 0xFFFFFFFF
    for b in data:
        crc = ((crc << 8) & 0xFFFFFFFF) ^ table[((crc >> 24) ^ b) & 0xFF]
    return crc


def image_crc(img):
    return crc32_mpeg2(img + b"\xFF" * (CRC_LEN - len(img)))


def image_kind(img):
    """0 = anchor, 1 = tag, from the version string compiled into the image."""
    for marker, kind in ((b"anc3.", 0), (b"tag3.", 1), (b"sNC3.", 1), (b"sC3.", 0)):
        if marker in img:
            return kind
    return None


def image_version(img):
    for marker in (b"anc3.", b"tag3.", b"sNC3.", b"sC3."):
        i = img.find(marker)
        if i >= 0:
            return img[i:i + 9].split(b"\0")[0].decode(errors="replace")
    return "?"


def check_image(img):
    if len(img) > CRC_LEN:
        sys.exit(f"image {len(img)} B exceeds {CRC_LEN} B")
    sp, pc = struct.unpack_from("<II", img, 0)
    if not (0x20000000 <= sp <= 0x20270000 and APP_BASE <= pc < 0x083FE000):
        sys.exit(f"vector table looks wrong: SP={sp:#x} PC={pc:#x} (not a 0x08200000 app?)")


def read_frame(ser, timeout):
    """Read one frame; returns (opcode, payload) or None on timeout/garbage."""
    deadline = time.monotonic() + timeout
    buf = b""
    while time.monotonic() < deadline:
        buf += ser.read(max(1, ser.in_waiting))
        if len(buf) >= 2 and len(buf) >= buf[1] >= 3:
            f = buf[:buf[1]]
            if sum(f[:-1]) & 0xFF != f[-1]:
                print(f"  bad checksum in reply: {f.hex()}", file=sys.stderr)
                return None
            return f[0], f[2:-1]
    if buf:
        print(f"  partial reply: {buf.hex()}", file=sys.stderr)
    return None


def xfer(ser, opcode, payload=b"", timeout=1.0):
    ser.reset_input_buffer()
    ser.write(frame(opcode, payload))
    ser.flush()
    return read_frame(ser, timeout)


def status(ser):
    r = xfer(ser, OP_STATUS)
    if not r or r[0] != OP_STATUS:
        return None
    p = r[1]
    sw = p[:9].split(b"\0")[0].decode(errors="replace")
    return sw, p[9], struct.unpack_from("<H", p, 10)[0]


def print_status(tag, st):
    sw, hw, self_id = st
    print(f"{tag}: SW={sw!r} HW={hw} self_id={self_id}"
          + (" (bootloader)" if hw == 0xFF else ""))


def cmd_status(ser, _a):
    st = status(ser)
    if not st:
        print("no STATUS reply")
        return 1
    print_status("device", st)
    return 0


def cmd_flash(ser, a):
    img = open(a.bin, "rb").read()
    check_image(img)
    padded = img + b"\xFF" * ((-len(img)) % CHUNK)
    crc = image_crc(img)
    print(f"image {a.bin}: {image_version(img)}, {len(img)} B, {len(padded) // CHUNK} chunks, crc={crc:#010x}")

    st = status(ser)
    if st and st[1] != 0xFF:
        print_status("app", st)
        if not a.yes and input("enter bootloader and reflash? [y/N] ").strip().lower() != "y":
            return 1
        print(f"UPDATE_FW reply: {xfer(ser, OP_UPDATE_FW)}")
        time.sleep(1.0)
        st = status(ser)
    if not st or st[1] != 0xFF:
        print(f"not in bootloader (status={st})")
        return 1
    print_status("bootloader", st)

    print("START_PROG (mass erase bank 2)...")
    r = xfer(ser, OP_START_PROG, timeout=15.0)
    if not r or r[0] != OP_START_PROG:
        print(f"erase failed: {r}")
        return 1
    n = len(padded) // CHUNK
    t0 = time.monotonic()
    for i in range(n):
        r = xfer(ser, OP_WRITE_BUFFER, padded[i * CHUNK:(i + 1) * CHUNK])
        if not r or r[0] != OP_WRITE_BUFFER:
            print(f"\nwrite failed at chunk {i} (addr {APP_BASE + i * CHUNK:#x}): {r}")
            return 1
        if i % 50 == 0 or i == n - 1:
            print(f"\r  {i + 1}/{n}", end="", flush=True)
    print(f"  ({time.monotonic() - t0:.1f}s)")

    print("HEADER_FILE (verify CRC, jump to app)...")
    r = xfer(ser, OP_HEADER_FILE, struct.pack("<II", crc, len(img)), timeout=5.0)
    if not r or r[0] != OP_HEADER_FILE or r[1][0] != 0:
        print(f"CRC verify failed: {r}")
        return 1
    print("CRC OK, app starting")
    time.sleep(2.0)
    st = status(ser)
    print_status("after", st) if st else print("no STATUS from app yet")
    return 0


def fota_status(ser):
    r = xfer(ser, OP_FOTA_STATUS)
    if not r or r[0] != OP_FOTA_STATUS or len(r[1]) < 44:
        return None
    p = r[1]
    keys = "mode state last_err rate target size loaded acked retries elapsed_ms t_valid t_status t_kind t_hw t_battery".split()
    v = struct.unpack_from("<BBBBHIIIIIBBBBB", p, 0)
    s = dict(zip(keys, v))
    s["t_sw"] = p[31:40].split(b"\0")[0].decode(errors="replace")
    s["t_app_crc"] = struct.unpack_from("<I", p, 40)[0]
    return s


class FotaError(Exception):
    pass


def err_text(code):
    return FOTA_ERR[code] if code < len(FOTA_ERR) else f"error {code}"


def fota_cmd(ser, op, payload=b""):
    r = xfer(ser, op, payload)
    if not r or r[0] != op:
        raise FotaError(f"opcode {op:#x}: no reply ({r}); is this board running anc/tag 3.1.0+?")
    return r[1]


def fota_enter(ser):
    p = fota_cmd(ser, OP_FOTA_ENTER)
    if p[0] != 0:
        raise FotaError(f"FOTA_ENTER refused: {err_text(p[0])}")


def fota_exit(ser):
    p = fota_cmd(ser, OP_FOTA_EXIT)
    if p[0] != 0:
        raise FotaError(f"FOTA_EXIT: {err_text(p[0])}")


def target_line(s):
    kind = {0: "anchor", 1: "tag"}.get(s["t_kind"], "?")
    return (f"target: SW={s['t_sw']!r} kind={kind} HW={s['t_hw']} battery={s['t_battery']} "
            f"app_crc={s['t_app_crc']:#010x}")


def fota_load(ser, img, target, kind, rate, progress=None):
    """Enter flasher mode and copy the image into the flasher's RAM.
    progress(done_bytes, total_bytes) is called as chunks are acknowledged."""
    fota_enter(ser)
    crc = image_crc(img)
    p = fota_cmd(ser, OP_FOTA_LOAD_BEGIN, struct.pack("<HBBII", target, kind, rate, len(img), crc))
    if p[0] != 0:
        raise FotaError(f"LOAD_BEGIN refused: {err_text(p[0])}")
    off = 0
    while off < len(img):
        p = fota_cmd(ser, OP_FOTA_LOAD_DATA, struct.pack("<I", off) + img[off:off + CHUNK])
        status_code, nxt = p[0], struct.unpack_from("<I", p, 1)[0]
        if status_code not in (0, 3):
            raise FotaError(f"LOAD_DATA failed at {off}: {err_text(status_code)}")
        off = nxt
        if progress:
            progress(off, len(img))
    return crc


def fota_run(ser, on_status=None, timeout=120):
    """Start the UWB session on a loaded flasher and wait until done/failed.
    on_status(status_dict, state_name) is called on every poll. Returns the final status."""
    p = fota_cmd(ser, OP_FOTA_START)
    if p[0] != 0:
        raise FotaError(f"START refused: {err_text(p[0])}")
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        time.sleep(0.25)
        s = fota_status(ser)
        if not s:
            continue
        state = FL_STATE[s["state"]] if s["state"] < len(FL_STATE) else str(s["state"])
        s["state_name"] = state
        if on_status:
            on_status(s, state)
        if state in ("done", "failed"):
            return s
    raise FotaError("FOTA session did not finish in time")


def fota_query(ser, target, timeout=4):
    """Ask UWB target `target` for its version/CRC (enters flasher mode). Returns status or None."""
    fota_enter(ser)
    p = fota_cmd(ser, OP_FOTA_QUERY, struct.pack("<H", target))
    if p[0] != 0:
        raise FotaError(f"QUERY refused: {err_text(p[0])}")
    deadline = time.monotonic() + timeout
    s = None
    while time.monotonic() < deadline:
        time.sleep(0.2)
        s = fota_status(ser)
        if s and FL_STATE[s["state"]] != "query":
            break
    return s if s and s["t_valid"] else None


def fota_discover(ser, timeout=6):
    """Broadcast DISCOVER (enters flasher mode); returns a list of dicts for every target that answered."""
    fota_enter(ser)
    p = fota_cmd(ser, OP_FOTA_DISCOVER)
    if p[0] != 0:
        raise FotaError(f"DISCOVER refused: {err_text(p[0])}")
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        time.sleep(0.3)
        s = fota_status(ser)
        if s and FL_STATE[s["state"]] != "discover":
            break
    found, start = [], 0
    while True:
        p = fota_cmd(ser, OP_FOTA_DISCOVER_LIST, bytes([start]))
        total, _, count = p[0], p[1], p[2]
        for i in range(count):
            e = p[3 + 18 * i:3 + 18 * (i + 1)]
            ident, kind, hw, batt = struct.unpack_from("<HBBB", e, 0)
            found.append({"id": ident, "kind": {0: "anchor", 1: "tag"}.get(kind, "?"), "hw": hw, "battery": batt,
                          "sw": e[5:14].split(b"\0")[0].decode(errors="replace"),
                          "app_crc": struct.unpack_from("<I", e, 14)[0]})
        start += count
        if count == 0 or start >= total:
            return found


def cmd_discover(ser, a):
    found = fota_discover(ser)
    if not a.stay:
        fota_exit(ser)
    if not found:
        print("no FOTA targets answered (they need 3.1.1+, and tags must be awake)")
        return 1
    for d in found:
        batt = "mains" if d["battery"] == 255 else f"{d['battery']}%"
        print(f"id {d['id']:5}  {d['kind']:6}  {d['sw']:10}  HW {d['hw']}  battery {batt:5}  app_crc {d['app_crc']:#010x}")
    return 0


def cmd_fota(ser, a):
    img = open(a.bin, "rb").read()
    check_image(img)
    kind = image_kind(img) if a.kind is None else {"anchor": 0, "tag": 1}[a.kind]
    if kind is None:
        sys.exit("cannot tell anchor vs tag image from its version string; pass --kind")
    print(f"image {a.bin}: {image_version(img)} ({'anchor' if kind == 0 else 'tag'}), "
          f"{len(img)} B, crc={image_crc(img):#010x}; target UWB id {a.target}, rate {a.rate}")
    t0 = time.monotonic()
    fota_load(ser, img, a.target, kind, RATES[a.rate])
    print(f"loaded into flasher RAM in {time.monotonic() - t0:.1f}s")
    last = [None]

    def show(s, state):
        line = (f"{state:12} {s['acked']:7}/{s['size']} B  rate {RATE_NAME.get(s['rate'], '?'):4}  "
                f"retries {s['retries']:4}  {s['elapsed_ms'] / 1000:6.1f}s")
        if line != last[0]:
            print("\r" + line, end="", flush=True)
            last[0] = line
    s = fota_run(ser, show)
    print()
    if s["t_valid"]:
        print(target_line(s))
    ok = s["state_name"] == "done"
    print("FOTA OK: target rebooted with the new image, CRC verified" if ok
          else f"FOTA FAILED: {err_text(s['last_err'])}")
    if not a.stay:
        fota_exit(ser)
    return 0 if ok else 1


def cmd_query(ser, a):
    s = fota_query(ser, a.target)
    if not a.stay:
        fota_exit(ser)
    if s:
        print(target_line(s))
        return 0
    print(f"no answer from UWB id {a.target}")
    return 1


def cmd_fota_exit(ser, _a):
    fota_exit(ser)
    print("left FOTA flasher mode")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", default="/dev/ttyUSB0")
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("status")
    f = sub.add_parser("flash", help="flash over the UART bootloader")
    f.add_argument("bin")
    f.add_argument("-y", "--yes", action="store_true")
    fo = sub.add_parser("fota", help="use the board on --port as FOTA flasher")
    fo.add_argument("target", type=int, help="target UWB id (Status self_id)")
    fo.add_argument("bin")
    fo.add_argument("--rate", choices=RATES, default="6m8", help="starting data rate; steps down on link loss")
    fo.add_argument("--kind", choices=("anchor", "tag"))
    fo.add_argument("--stay", action="store_true", help="stay in FOTA flasher mode afterwards")
    q = sub.add_parser("query", help="ask a UWB target for its version and CRC")
    q.add_argument("target", type=int)
    q.add_argument("--stay", action="store_true")
    d = sub.add_parser("discover", help="list FOTA targets in UWB range")
    d.add_argument("--stay", action="store_true")
    sub.add_parser("fota-exit")
    sub.add_parser("selftest")
    a = ap.parse_args()
    if a.cmd == "selftest":
        assert crc32_mpeg2(b"123456789") == 0x0376E6E7
        assert frame(OP_STATUS) == bytes([11, 3, 14])
        print("ok")
        return 0
    cmds = {"status": cmd_status, "flash": cmd_flash, "fota": cmd_fota, "query": cmd_query,
            "fota-exit": cmd_fota_exit,
            "discover": cmd_discover}
    with serial.Serial(a.port, BAUD, timeout=0.05) as ser:
        try:
            return cmds[a.cmd](ser, a)
        except FotaError as e:
            sys.exit(str(e))


if __name__ == "__main__":
    sys.exit(main())
