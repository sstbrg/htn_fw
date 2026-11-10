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
  htn_fw.py --port /dev/ttyUSB0 wake 19                          # LoRa wake-up of sleeping tag 19 (anchor on --port)
  htn_fw.py --port /dev/ttyUSB0 hw --set D                       # board revision of the board on --port (#21)
  htn_fw.py --port /dev/ttyUSB0 fota-hw 57 D                     # board revision of UWB target 57

Board revision (#21): C = 0, C2 = 1, D = 2 (or 0-7). Stored in the board's self-id page, it wins
over the strap pins (a Rev D board with an open strap reads as Rev C). `flash` and `fota` take
--hw to store it once the new firmware runs (anc4.1.0 / tag4.2.0 and later).

UART frame: [opcode, length, payload..., checksum], length = whole frame,
checksum = sum(frame[:-1]) & 0xFF.

Bootloader (bootloader_hitan): 11 STATUS, 16 START_PROG (erase bank 2),
17 WRITE_BUFFER (128 B), 18 HEADER_FILE (crc, len), 10 RESET.
Apps: 11 STATUS, 15 UPDATE_FW (jump to bootloader), 0x40-0x47 FOTA (fota.h).
Anchors: 5 SEARCH_TWR {tag_id}, 6 WAKEUP_MODE {tag_id} (one LoRa wake-up packet).
Image CRC: STM32 CRC-32 (poly 0x04C11DB7, init 0xFFFFFFFF, no reflection),
byte-fed, over the image padded with 0xFF to 255 pages (0x1FE000 bytes).
"""
import argparse
import re
import struct
import sys
import time

import serial

BAUD = 921600
OP_SEARCH_TWR, OP_WAKEUP = 5, 6
OP_RESET, OP_STATUS, OP_UPDATE_FW = 10, 11, 15
OP_START_PROG, OP_WRITE_BUFFER, OP_HEADER_FILE, OP_ERROR = 16, 17, 18, 0xEE
OP_FOTA_ENTER, OP_FOTA_LOAD_BEGIN, OP_FOTA_LOAD_DATA, OP_FOTA_START = 0x40, 0x41, 0x42, 0x43
OP_FOTA_STATUS, OP_FOTA_QUERY, OP_FOTA_ABORT, OP_FOTA_EXIT = 0x44, 0x45, 0x46, 0x47
OP_FOTA_DISCOVER, OP_FOTA_DISCOVER_LIST = 0x48, 0x49
OP_LED_TEST = 0x4B
OP_ANT_DELAY = 20
OP_HW_REV = 27
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
HW_REV = {"C": 0, "C2": 1, "D": 2}
HW_REV_MAX = 7
HW_REBOOT_S = 2.5      # a board reboots after storing a new revision; a tag then stays awake 6 s


def parse_rev(text):
    """Board revision from a name (C, C2, D) or a number 0-7."""
    t = str(text).strip().upper()
    if t in HW_REV:
        return HW_REV[t]
    if t.isdigit() and int(t) <= HW_REV_MAX:
        return int(t)
    raise argparse.ArgumentTypeError(f"board revision {text!r}: use {', '.join(HW_REV)} or 0-{HW_REV_MAX}")


def stores_hw(sw):
    """True when firmware `sw` stores a board revision (anc4.1.0 / tag4.2.0 and later)."""
    m = re.fullmatch(r"(anc|tag)(\d+)\.(\d+)\.(\d+)", sw or "")
    if not m:
        return False
    return tuple(map(int, m.groups()[1:])) >= ((4, 1, 0) if m.group(1) == "anc" else (4, 2, 0))


def rev_name(rev):
    names = {v: k for k, v in HW_REV.items()}
    return f"Rev {names[rev]} ({rev})" if rev in names else f"revision {rev}"


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


# Version string compiled into the image: anc<major>.x.y / tag<major>.x.y, legacy sC3.x / sNC3.x
VERSION_RE = re.compile(rb"(anc|tag|sNC|sC)\d+\.\d+\.\d+\0")
VERSION_KIND = {b"anc": 0, b"sC": 0, b"tag": 1, b"sNC": 1}


def image_kind(img):
    """0 = anchor, 1 = tag, from the version string compiled into the image."""
    m = VERSION_RE.search(img)
    return VERSION_KIND[m.group(1)] if m else None


def image_version(img):
    m = VERSION_RE.search(img)
    return m.group(0)[:-1].decode() if m else "?"


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


def hw_rev(ser, rev=None):
    """Read the board revision of the board on `ser` (UART opcode 27), or store `rev` (0xFF clears it).
    Returns {status, active, stored, straps} or None when the firmware has no opcode 27."""
    r = xfer(ser, OP_HW_REV, b"" if rev is None else bytes([rev]))
    if not r or r[0] != OP_HW_REV or len(r[1]) < 4:
        return None
    return dict(zip(("status", "active", "stored", "straps"), r[1][:4]))


def hw_line(h):
    stored = "none (the strap pins decide)" if h["stored"] == 0xFF else rev_name(h["stored"])
    return f"board revision {rev_name(h['active'])}; stored {stored}; strap pins read {rev_name(h['straps'])}"


def set_hw_uart(ser, rev, log=print):
    """Store board revision `rev` on the board on `ser` and confirm it after the reboot. True on success."""
    h = hw_rev(ser)
    if h is None:
        log("no reply to the board revision command: needs anc4.1.0 / tag4.2.0 or later")
        return False
    if h["active"] == rev and h["stored"] == rev:
        log(hw_line(h))
        return True
    h = hw_rev(ser, rev)
    if h is None or h["status"] != 0:
        log(f"storing the board revision failed ({h})")
        return False
    time.sleep(HW_REBOOT_S)
    h = hw_rev(ser)
    if h is None:
        log("no reply after the reboot")
        return False
    log(hw_line(h))
    return h["active"] == rev and h["stored"] == rev


def cmd_hw(ser, a):
    if a.set is not None:
        return 0 if set_hw_uart(ser, a.set) else 1
    h = hw_rev(ser, 0xFF if a.clear else None)
    if h is None:
        print("no reply: the board revision command needs anc4.1.0 / tag4.2.0 or later")
        return 1
    if a.clear:
        time.sleep(HW_REBOOT_S)
        h = hw_rev(ser) or h
    print(hw_line(h))
    return 0 if h["status"] == 0 else 1


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
    if a.hw is not None and not set_hw_uart(ser, a.hw):
        print(f"FAILED to store {rev_name(a.hw)}")
        return 1
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


def fota_query(ser, target, timeout=4, hw=None):
    """Ask UWB target `target` for its version/CRC (enters flasher mode). Returns status or None.
    With `hw`, the target also stores that board revision and reboots (anc4.1.0 / tag4.2.0 targets)."""
    fota_enter(ser)
    p = fota_cmd(ser, OP_FOTA_QUERY, struct.pack("<H", target) + (b"" if hw is None else bytes([hw])))
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


def fota_set_hw(ser, target, rev, log=print):
    """Store board revision `rev` on UWB target `target` and confirm it after the target reboots.
    True on success. A target before anc4.1.0 / tag4.2.0 ignores the request: the check fails."""
    s = fota_query(ser, target, hw=rev)     # stores it; reboots only when it changed
    if not s:
        log(f"no answer from UWB id {target}")
        return False
    if not stores_hw(s["t_sw"]):
        log(f"UWB id {target} runs {s['t_sw']!r}: storing the board revision needs anc4.1.0 / tag4.2.0 or later")
        return False
    if s["t_status"] != 0:
        log(f"UWB id {target} could not store the board revision ({err_text(s['t_status'])})")
        return False
    time.sleep(HW_REBOOT_S)
    s = fota_query(ser, target)
    if not s or s["t_hw"] != rev:
        got = "no answer" if not s else f"it reports {rev_name(s['t_hw'])}, firmware {s['t_sw']}"
        log(f"UWB id {target}: board revision not confirmed ({got}); needs anc4.1.0 / tag4.2.0 or later")
        return False
    log(f"UWB id {target}: {rev_name(rev)} stored and in use")
    return True


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


WAKE_RETRY = 2.0       # s between LoRa wake-ups: the sleeping tag's LoRa receiver is on ~1 s in 2
WAKE_POLL = 0.3        # s between SEARCH_TWR polls that check whether the tag is heard


def tag_heard(ser, tag_id):
    """One SEARCH_TWR to `tag_id`; True when the reply reports it heard over UWB or LoRa.
    The anchor answers with the latest exchange and polls the tag again."""
    r = xfer(ser, OP_SEARCH_TWR, struct.pack("<H", tag_id), timeout=0.4)
    if not r or r[0] != OP_SEARCH_TWR or len(r[1]) < 5 or struct.unpack_from("<H", r[1], 2)[0] != tag_id:
        return False
    s = r[1][4]                                    # data_status: high nibble UWB, low nibble LoRa, 0 = received
    return (s & 0xF0) == 0 or (s & 0x0F) == 0


def wake_tag(ser, tag_id, timeout=30.0, log=print):
    """Wake sleeping tag `tag_id` through the anchor on `ser`: a LoRa wake-up every WAKE_RETRY s until the
    tag is heard (then it stays awake 15 min after the last poll it answers). Leaves FOTA flasher mode
    first: the flasher owns the radios. Returns True when the tag was heard."""
    st = status(ser)
    if not st:
        raise FotaError("no STATUS reply from the board on this port")
    if st[1] == 0xFF or not st[0].startswith(("anc", "sC")):
        raise FotaError(f"the board on this port runs {st[0]!r}: only an anchor can send LoRa wake-ups")
    s = fota_status(ser)
    if s and s["mode"]:
        fota_exit(ser)
    tag_heard(ser, tag_id)     # drop it: the first reply reports everything since the anchor's last reply
    deadline = time.monotonic() + timeout
    sent, last_sent = 0, None
    while time.monotonic() < deadline:
        time.sleep(WAKE_POLL)
        if tag_heard(ser, tag_id):
            log(f"tag {tag_id} is awake" + (f" ({sent} wake-up(s) sent)" if sent else ""))
            return True
        if last_sent is None or time.monotonic() - last_sent >= WAKE_RETRY:
            r = xfer(ser, OP_WAKEUP, struct.pack("<H", tag_id), timeout=0.4)
            if not r or r[0] != OP_WAKEUP:
                raise FotaError(f"WAKEUP_MODE: no reply ({r})")
            sent, last_sent = sent + 1, time.monotonic()
            log(f"LoRa wake-up {sent} to tag {tag_id}")
    log(f"tag {tag_id} not heard after {timeout:.0f} s: check its id, battery and distance")
    return False


def cmd_wake(ser, a):
    return 0 if wake_tag(ser, a.tag, a.timeout) else 1


def cmd_discover(ser, a):
    found = fota_discover(ser)
    if not a.stay:
        fota_exit(ser)
    if not found:
        print("no FOTA targets answered (they need 3.1.1+, and tags must be awake)")
        return 1
    for d in found:
        batt = "no gauge" if d["battery"] == 255 else f"{d['battery']}%"
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
    if ok and a.hw is not None and not fota_set_hw(ser, a.target, a.hw):
        ok = False
    if not a.stay:
        fota_exit(ser)
    return 0 if ok else 1


def cmd_fota_hw(ser, a):
    ok = fota_set_hw(ser, a.target, a.rev)
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


LED_NAMES = ["battery bar 0 (PE14)", "battery bar 1 (PE11)", "battery bar 2 (PE8)", "battery bar 3 (PF15)",
             "BIT fail", "BIT pass", "link", "RGB red (PE5)", "RGB green (PE6)", "RGB blue (PF8)",
             "LoRa RX (PC9)", "LoRa TX (PA8)"]


def cmd_led_test(ser, a):
    """Light each LED the firmware can drive, one at a time, so the fitted ones can be identified."""
    p = fota_cmd(ser, OP_LED_TEST, bytes([0, 0]))
    count = p[1]
    if p[0] != 0 or not count:
        print("LED test not supported by this firmware")
        return 1
    for i in range(count):
        name = LED_NAMES[i] if i < len(LED_NAMES) else f"LED {i}"
        print(f"{i:2}: {name} ...", end="", flush=True)
        fota_cmd(ser, OP_LED_TEST, bytes([i, 1]))
        time.sleep(a.hold)
        fota_cmd(ser, OP_LED_TEST, bytes([i, 0]))
        print(" off")
        time.sleep(0.4)
    return 0


def cmd_antenna_delay(ser, a):
    """Read or set the per-device antenna delays (DW1000 units, ~15.65 ps) stored in flash."""
    payload = b"" if a.set is None else struct.pack("<HH", *a.set)
    r = xfer(ser, OP_ANT_DELAY, payload)
    if not r or r[0] != OP_ANT_DELAY or len(r[1]) < 5:
        print("no reply: antenna delays need firmware 3.1.5+")
        return 1
    status, tx, rx = r[1][0], *struct.unpack_from("<HH", r[1], 1)
    show = lambda v: "default" if v in (0, 0xFFFF) else str(v)
    print(("write failed; " if status else "") + f"TX antenna delay {show(tx)}, RX antenna delay {show(rx)}")
    return 1 if status else 0


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
    f.add_argument("--hw", type=parse_rev, help="board revision to store once the new firmware runs (C, C2, D, 0-7)")
    fo = sub.add_parser("fota", help="use the board on --port as FOTA flasher")
    fo.add_argument("target", type=int, help="target UWB id (Status self_id)")
    fo.add_argument("bin")
    fo.add_argument("--rate", choices=RATES, default="6m8", help="starting data rate; steps down on link loss")
    fo.add_argument("--kind", choices=("anchor", "tag"))
    fo.add_argument("--hw", type=parse_rev, help="board revision to store on the target afterwards (C, C2, D, 0-7)")
    fo.add_argument("--stay", action="store_true", help="stay in FOTA flasher mode afterwards")
    fh = sub.add_parser("fota-hw", help="store the board revision of a UWB target (anc4.1.0 / tag4.2.0+)")
    fh.add_argument("target", type=int, help="target UWB id (Status self_id)")
    fh.add_argument("rev", type=parse_rev, help="C, C2, D or 0-7")
    fh.add_argument("--stay", action="store_true", help="stay in FOTA flasher mode afterwards")
    hw = sub.add_parser("hw", help="read or store the board revision of the board on --port (anc4.1.0 / tag4.2.0+)")
    hwg = hw.add_mutually_exclusive_group()
    hwg.add_argument("--set", type=parse_rev, metavar="REV", help="C, C2, D or 0-7")
    hwg.add_argument("--clear", action="store_true", help="forget the stored revision: the strap pins decide")
    q = sub.add_parser("query", help="ask a UWB target for its version and CRC")
    q.add_argument("target", type=int)
    q.add_argument("--stay", action="store_true")
    d = sub.add_parser("discover", help="list FOTA targets in UWB range")
    d.add_argument("--stay", action="store_true")
    lt = sub.add_parser("led-test", help="light each board LED in turn to see which are fitted")
    lt.add_argument("--hold", type=float, default=1.5, help="seconds each LED stays on")
    ad = sub.add_parser("antenna-delay", help="read or set per-device antenna delays (3.1.5+)")
    ad.add_argument("--set", nargs=2, type=int, metavar=("TX", "RX"), help="0 or 65535 = default")
    sub.add_parser("fota-exit")
    w = sub.add_parser("wake", help="wake a sleeping tag over LoRa (the board on --port must be an anchor)")
    w.add_argument("tag", type=int, help="tag id (Status self_id)")
    w.add_argument("--timeout", type=float, default=30.0, help="seconds to keep trying")
    sub.add_parser("selftest")
    a = ap.parse_args()
    if a.cmd == "selftest":
        assert crc32_mpeg2(b"123456789") == 0x0376E6E7
        assert frame(OP_STATUS) == bytes([11, 3, 14])
        assert frame(OP_WAKEUP, struct.pack("<H", 19)) == bytes([6, 5, 19, 0, 30])
        for img, kind, ver in ((b"x\0anc4.0.0\0", 0, "anc4.0.0"), (b"tag3.3.1\0", 1, "tag3.3.1"),
                               (b"sC3.0.40\0", 0, "sC3.0.40"), (b"anc4.\0", None, "?")):
            assert image_kind(img) == kind and image_version(img) == ver, img
        assert [parse_rev(x) for x in ("C", "c2", "D", "5")] == [0, 1, 2, 5]
        assert stores_hw("tag4.2.0") and stores_hw("anc4.1.0") and stores_hw("anc10.0.0")
        assert not stores_hw("tag4.1.1") and not stores_hw("anc4.0.2") and not stores_hw("sC3.0.67")
        print("ok")
        return 0
    cmds = {"status": cmd_status, "flash": cmd_flash, "fota": cmd_fota, "query": cmd_query,
            "fota-exit": cmd_fota_exit,
            "discover": cmd_discover, "led-test": cmd_led_test,
            "antenna-delay": cmd_antenna_delay, "wake": cmd_wake, "hw": cmd_hw, "fota-hw": cmd_fota_hw}
    with serial.Serial(a.port, BAUD, timeout=0.05) as ser:
        try:
            return cmds[a.cmd](ser, a)
        except FotaError as e:
            sys.exit(str(e))


if __name__ == "__main__":
    sys.exit(main())
