#!/usr/bin/env python3
"""HITAN FOTA wizard: step-by-step firmware update over the air.

  python3 tools/fw_wizard.py

Steps:
  1. Connect the board that will be the FOTA loader (any tag or anchor) to USB.
  2. Switch it to FOTA loader mode.
  3. Pick a saved release (releases/<kind>/<version>/) or build a new one.
  4. Reach the target: LoRa wake-up first, then UWB.
  5. Handshake, transfer, write, verify.

Serial ports are always opened at 921600 (the boards' only baud).
"""
import json
import queue
import threading
import time
import tkinter as tk
from pathlib import Path
from tkinter import filedialog, messagebox, ttk

import serial
import serial.tools.list_ports

import fw_release
import htn_fw

MIN_BATTERY = 30        # % for battery-powered boards (tags); anchors report 255 = mains
KIND_NAME = {0: "anchor", 1: "tag"}
RATE_NAMES = {"6.8 Mbps (fast, short range)": "6m8", "850 kbps": "850k", "110 kbps (long range)": "110k"}


def probe(port):
    """Ask whatever is on `port` who it is. Never raises."""
    info = {"port": port, "mode": "none", "text": "no answer", "kind": None, "sw": "", "id": None,
            "hw": None, "battery": None, "fota": False, "fota_mode": False}
    try:
        with serial.Serial(port, htn_fw.BAUD, timeout=0.05) as ser:
            r = htn_fw.xfer(ser, htn_fw.OP_STATUS, timeout=0.6)
            if not r or r[0] != htn_fw.OP_STATUS:
                return info
            p = r[1]
            sw = p[:9].split(b"\0")[0].decode(errors="replace")
            hw, self_id = p[9], int.from_bytes(p[10:12], "little")
            info.update(sw=sw, hw=hw, id=self_id)
            if hw == 0xFF:
                info.update(mode="bootloader", text=f"bootloader {sw} (no app running)")
                return info
            kind = "tag" if sw.startswith(("tag", "sNC")) else "anchor"
            info.update(mode="app", kind=kind)
            if kind == "tag" and len(p) > 15:
                info["battery"] = p[15]
            s = htn_fw.fota_status(ser)
            if s:
                info["fota"] = True
                info["fota_mode"] = bool(s["mode"])
            batt = f", battery {info['battery']}%" if info["battery"] is not None else ""
            fota = " [FOTA loader mode]" if info["fota_mode"] else ("" if info["fota"] else " (no FOTA support: needs 3.1.0+)")
            info["text"] = f"{kind} id {self_id}, {sw}, HW {hw}{batt}{fota}"
    except (serial.SerialException, OSError) as e:
        info["text"] = f"cannot open: {e}"
    return info


def ftdi_ports():
    ports = [p for p in serial.tools.list_ports.comports() if p.vid == 0x0403]
    return sorted(ports, key=lambda p: p.device)


class Wizard(tk.Tk):
    STEPS = ["1  Connect FOTA loader", "2  FOTA loader mode", "3  Choose firmware",
             "4  Find the target", "5  Flash and verify"]

    def __init__(self):
        super().__init__()
        self.title("HITAN FOTA wizard")
        self.geometry("980x700")
        self.minsize(820, 600)
        self.q = queue.Queue()
        self.busy = False
        self.loader = None          # probe() dict of the FOTA loader board
        self.release = None         # dict with kind/version/path/crc/size
        self.target = None          # fota status dict from the query
        self.step = 0

        style = ttk.Style(self)
        if "clam" in style.theme_names():
            style.theme_use("clam")
        style.configure("Step.TLabel", font=("TkDefaultFont", 11), padding=(10, 6))
        style.configure("StepOn.TLabel", font=("TkDefaultFont", 11, "bold"), padding=(10, 6),
                        foreground="#ffffff", background="#2f6fb3")
        style.configure("StepDone.TLabel", font=("TkDefaultFont", 11), padding=(10, 6), foreground="#2e7d32")
        style.configure("Title.TLabel", font=("TkDefaultFont", 15, "bold"))
        style.configure("Big.TButton", font=("TkDefaultFont", 11, "bold"), padding=8)

        outer = ttk.Frame(self, padding=8)
        outer.pack(fill="both", expand=True)
        top = ttk.Frame(outer)
        top.pack(fill="both", expand=True)

        side = ttk.Frame(top, padding=(0, 0, 10, 0))
        side.pack(side="left", fill="y")
        self.step_labels = []
        for name in self.STEPS:
            lbl = ttk.Label(side, text=name, style="Step.TLabel", width=24)
            lbl.pack(fill="x", pady=1)
            self.step_labels.append(lbl)
        ttk.Separator(side).pack(fill="x", pady=8)
        self.summary = ttk.Label(side, text="", justify="left", wraplength=210)
        self.summary.pack(fill="x")

        self.body = ttk.Frame(top)
        self.body.pack(side="left", fill="both", expand=True)

        nav = ttk.Frame(outer, padding=(0, 6))
        nav.pack(fill="x")
        self.back_btn = ttk.Button(nav, text="< Back", command=self.back)
        self.back_btn.pack(side="left")
        self.next_btn = ttk.Button(nav, text="Next >", command=self.next, style="Big.TButton")
        self.next_btn.pack(side="right")

        prog = ttk.Frame(outer)
        prog.pack(fill="x")
        self.prog_text = ttk.Label(prog, text="idle")
        self.prog_text.pack(anchor="w")
        self.prog = ttk.Progressbar(prog, maximum=100)
        self.prog.pack(fill="x", pady=(2, 6))

        logf = ttk.LabelFrame(outer, text="Log", padding=4)
        logf.pack(fill="both", expand=False)
        self.log_box = tk.Text(logf, height=10, wrap="word", font=("TkFixedFont", 9), state="disabled")
        sb = ttk.Scrollbar(logf, command=self.log_box.yview)
        self.log_box.configure(yscrollcommand=sb.set)
        sb.pack(side="right", fill="y")
        self.log_box.pack(fill="both", expand=True)
        for tag, color in (("ok", "#2e7d32"), ("warn", "#b26a00"), ("err", "#c62828"), ("info", "#222222")):
            self.log_box.tag_configure(tag, foreground=color)

        self.after(100, self.pump)
        self.show_step(0)
        # come up in front of the editor/terminal it was started from
        self.lift()
        self.attributes("-topmost", True)
        self.after(800, lambda: self.attributes("-topmost", False))

    # ---------------- plumbing ----------------
    def log(self, text, level="info"):
        self.q.put(("log", level, text))

    def progress(self, pct, text):
        self.q.put(("progress", pct, text))

    def run(self, job, done=None):
        """Run job() in a worker thread; done(result) runs on the UI thread."""
        if self.busy:
            return
        self.busy = True
        self.update_nav()

        def worker():
            try:
                res = job()
                self.q.put(("done", done, res, None))
            except Exception as e:  # report every failure in the log, never crash the UI
                self.q.put(("done", done, None, e))
        threading.Thread(target=worker, daemon=True).start()

    def pump(self):
        try:
            while True:
                msg = self.q.get_nowait()
                if msg[0] == "log":
                    _, level, text = msg
                    self.log_box.configure(state="normal")
                    self.log_box.insert("end", time.strftime("%H:%M:%S  ") + text + "\n", level)
                    self.log_box.see("end")
                    self.log_box.configure(state="disabled")
                elif msg[0] == "progress":
                    self.prog["value"] = msg[1]
                    self.prog_text.configure(text=msg[2])
                elif msg[0] == "done":
                    _, cb, res, err = msg
                    self.busy = False
                    if err:
                        self.log(f"ERROR: {err}", "err")
                        messagebox.showerror("Error", str(err))
                    elif cb:
                        cb(res)
                    self.update_nav()
        except queue.Empty:
            pass
        self.after(100, self.pump)

    def update_summary(self):
        lines = []
        if self.loader:
            lines.append(f"Loader: {self.loader['kind']} {self.loader['id']} on {Path(self.loader['port']).name}")
        if self.release:
            lines.append(f"Firmware: {self.release['version']}")
        if self.target:
            lines.append(f"Target: {KIND_NAME.get(self.target['t_kind'], '?')} {self.target_id} "
                         f"({self.target['t_sw']})")
        self.summary.configure(text="\n".join(lines))

    def update_nav(self):
        ok = {0: self.loader is not None and self.loader.get("fota"),
              1: self.loader is not None and self.loader.get("fota_mode"),
              2: self.release is not None,
              3: self.target is not None,
              4: False}[self.step]
        self.next_btn.configure(state="normal" if ok and not self.busy else "disabled")
        self.back_btn.configure(state="normal" if self.step > 0 and not self.busy else "disabled")
        for i, lbl in enumerate(self.step_labels):
            lbl.configure(style="StepOn.TLabel" if i == self.step else
                          ("StepDone.TLabel" if i < self.step else "Step.TLabel"))
        self.update_summary()

    def clear_body(self, title, hint):
        for w in self.body.winfo_children():
            w.destroy()
        ttk.Label(self.body, text=title, style="Title.TLabel").pack(anchor="w")
        ttk.Label(self.body, text=hint, wraplength=640, justify="left").pack(anchor="w", pady=(4, 10))

    def show_step(self, n):
        self.step = n
        [self.step_connect, self.step_mode, self.step_firmware, self.step_target, self.step_flash][n]()
        self.update_nav()

    def next(self):
        if self.step < len(self.STEPS) - 1:
            self.show_step(self.step + 1)

    def back(self):
        if self.step > 0:
            self.show_step(self.step - 1)

    # ---------------- step 1 ----------------
    def step_connect(self):
        self.clear_body("Connect the FOTA loader",
                        "Plug the board that will send the firmware (any tag or anchor running 3.1.0 or newer) "
                        "into USB, then press Scan. Select it in the list.")
        cols = ("port", "serial", "device")
        self.dev_tree = ttk.Treeview(self.body, columns=cols, show="headings", height=8, selectmode="browse")
        for c, w in zip(cols, (110, 110, 520)):
            self.dev_tree.heading(c, text=c.capitalize())
            self.dev_tree.column(c, width=w, anchor="w")
        self.dev_tree.pack(fill="both", expand=True)
        self.dev_tree.bind("<<TreeviewSelect>>", self.on_pick_loader)
        row = ttk.Frame(self.body)
        row.pack(fill="x", pady=6)
        ttk.Button(row, text="Scan USB", command=self.scan, style="Big.TButton").pack(side="left")
        self.devices = {}
        self.scan()

    def scan(self):
        self.progress(0, "scanning USB serial ports...")

        def job():
            return [(p, probe(p.device)) for p in ftdi_ports()]

        def done(res):
            self.dev_tree.delete(*self.dev_tree.get_children())
            self.devices = {}
            for p, info in res:
                info["serial"] = p.serial_number or ""
                iid = self.dev_tree.insert("", "end", values=(Path(p.device).name, info["serial"], info["text"]))
                self.devices[iid] = info
            self.log(f"found {len(res)} USB serial device(s)" if res else "no USB serial devices found: plug a board in",
                     "info" if res else "warn")
            self.progress(0, "idle")
            usable = [iid for iid, d in self.devices.items() if d["fota"]]
            if len(usable) == 1:
                self.dev_tree.selection_set(usable[0])
        self.run(job, done)

    def on_pick_loader(self, _e=None):
        sel = self.dev_tree.selection()
        self.loader = self.devices.get(sel[0]) if sel else None
        if self.loader:
            d = self.loader
            if d["mode"] == "bootloader":
                self.log(f"{d['port']} is in the bootloader: flash an app over USB first", "warn")
            elif not d["fota"]:
                self.log(f"{d['port']}: {d['sw']} has no FOTA support, flash 3.1.0+ over USB first", "warn")
            elif d["battery"] is not None and d["battery"] < MIN_BATTERY:
                self.log(f"loader battery {d['battery']}% is below {MIN_BATTERY}%: charge it first", "err")
                self.loader = dict(d, fota=False)
            else:
                self.log(f"loader: {d['text']}", "ok")
        self.update_nav()

    # ---------------- step 2 ----------------
    def step_mode(self):
        d = self.loader
        self.clear_body("Switch it to FOTA loader mode",
                        f"{d['kind'].capitalize()} {d['id']} on {d['port']} will stop its normal work and act as the "
                        "flasher. Its status LED (battery-bar LED on anchors) then blinks fast; another LED blinks "
                        "while it sends data.")
        row = ttk.Frame(self.body)
        row.pack(fill="x", pady=6)
        ttk.Button(row, text="Enable FOTA loader mode", style="Big.TButton", command=self.enable_loader).pack(side="left")
        ttk.Button(row, text="Leave FOTA loader mode", command=self.disable_loader).pack(side="left", padx=8)
        self.mode_lbl = ttk.Label(self.body, text="FOTA loader mode: " + ("ON" if d["fota_mode"] else "off"))
        self.mode_lbl.pack(anchor="w", pady=8)

    def enable_loader(self):
        port = self.loader["port"]

        def job():
            with serial.Serial(port, htn_fw.BAUD, timeout=0.05) as ser:
                htn_fw.fota_enter(ser)
                s = htn_fw.fota_status(ser)
                return bool(s and s["mode"])

        def done(on):
            self.loader["fota_mode"] = on
            self.mode_lbl.configure(text="FOTA loader mode: " + ("ON" if on else "off"))
            self.log("FOTA loader mode ON" if on else "board did not enter FOTA loader mode", "ok" if on else "err")
        self.run(job, done)

    def disable_loader(self):
        port = self.loader["port"]

        def job():
            with serial.Serial(port, htn_fw.BAUD, timeout=0.05) as ser:
                htn_fw.fota_exit(ser)

        def done(_):
            self.loader["fota_mode"] = False
            if hasattr(self, "mode_lbl") and self.mode_lbl.winfo_exists():
                self.mode_lbl.configure(text="FOTA loader mode: off")
            self.log("left FOTA loader mode, board back to normal operation", "ok")
        self.run(job, done)

    # ---------------- step 3 ----------------
    def step_firmware(self):
        self.clear_body("Choose the firmware",
                        "Pick a saved release, build a new one (bumps the version, builds and saves it under "
                        "releases/), or browse for a .bin file.")
        cols = ("kind", "version", "size", "crc", "built", "note")
        self.rel_tree = ttk.Treeview(self.body, columns=cols, show="headings", height=9, selectmode="browse")
        for c, w in zip(cols, (60, 90, 80, 100, 150, 260)):
            self.rel_tree.heading(c, text=c.capitalize())
            self.rel_tree.column(c, width=w, anchor="w")
        self.rel_tree.pack(fill="both", expand=True)
        self.rel_tree.bind("<<TreeviewSelect>>", self.on_pick_release)
        row = ttk.Frame(self.body)
        row.pack(fill="x", pady=6)
        ttk.Button(row, text="Download latest from GitHub", command=self.fetch_github).pack(side="left")
        ttk.Button(row, text="Build new release...", command=self.build_dialog).pack(side="left", padx=8)
        ttk.Button(row, text="Browse .bin...", command=self.browse_bin).pack(side="left")
        ttk.Button(row, text="Refresh", command=self.fill_releases).pack(side="left", padx=8)
        self.fill_releases()

    def fetch_github(self):
        self.progress(30, "downloading the latest GitHub release...")

        def job():
            return fw_release.fetch(log=self.log)

        def done(dirs):
            self.progress(0, "idle")
            if not dirs:
                self.log("the latest GitHub release has no firmware assets", "warn")
            self.fill_releases(select_dir=dirs[0] if dirs else None)
            self.on_pick_release()
        self.run(job, done)

    def fill_releases(self, select_dir=None):
        self.rel_tree.delete(*self.rel_tree.get_children())
        self.rels = {}
        for d in fw_release.releases():
            iid = self.rel_tree.insert("", "end", values=(d["kind"], d["version"], d["size"], d["crc"],
                                                          d["built"].replace("T", " "), d.get("note", "")))
            self.rels[iid] = d
            if select_dir and d["dir"] == select_dir:
                self.rel_tree.selection_set(iid)

    def on_pick_release(self, _e=None):
        sel = self.rel_tree.selection()
        if sel:
            d = self.rels[sel[0]]
            self.release = {"kind": d["kind"], "version": d["version"], "path": Path(d["path"]),
                            "crc": d["crc"], "size": d["size"]}
            self.log(f"firmware: {d['version']} ({d['size']} B, crc {d['crc']})", "ok")
        self.update_nav()

    def browse_bin(self):
        f = filedialog.askopenfilename(title="Firmware image", filetypes=[("Firmware", "*.bin"), ("All", "*")])
        if not f:
            return
        img = Path(f).read_bytes()
        kind = htn_fw.image_kind(img)
        if kind is None:
            messagebox.showerror("Unknown image", "Cannot tell anchor vs tag from the version string in this file.")
            return
        self.release = {"kind": KIND_NAME[kind], "version": htn_fw.image_version(img), "path": Path(f),
                        "crc": f"0x{htn_fw.image_crc(img):08x}", "size": len(img)}
        self.log(f"firmware: {self.release['version']} from {f}", "ok")
        self.update_nav()

    def build_dialog(self):
        dlg = tk.Toplevel(self)
        dlg.title("Build new release")
        dlg.transient(self)
        dlg.grab_set()
        f = ttk.Frame(dlg, padding=12)
        f.pack(fill="both", expand=True)
        kind = tk.StringVar(value=self.loader_target_kind_guess())
        part = tk.StringVar(value="patch")
        note = tk.StringVar()
        ttk.Label(f, text="Board").grid(row=0, column=0, sticky="w")
        ttk.Combobox(f, textvariable=kind, values=("anchor", "tag"), state="readonly", width=10).grid(row=0, column=1, sticky="w")
        cur = ttk.Label(f, text="")
        cur.grid(row=0, column=2, sticky="w", padx=8)
        ttk.Label(f, text="Bump").grid(row=1, column=0, sticky="w", pady=6)
        ttk.Combobox(f, textvariable=part, values=("patch", "minor", "major", "keep version"), state="readonly",
                     width=12).grid(row=1, column=1, sticky="w")
        ttk.Label(f, text="Changelog note").grid(row=2, column=0, sticky="w")
        ttk.Entry(f, textvariable=note, width=50).grid(row=2, column=1, columnspan=2, sticky="we")

        def refresh(*_):
            cur.configure(text=f"current: {fw_release.version_string(kind.get())}")
        kind.trace_add("write", refresh)
        refresh()

        def go():
            k, pt, nt = kind.get(), part.get(), note.get().strip() or None
            dlg.destroy()
            self.progress(0, f"building {k}...")

            def job():
                if pt != "keep version":
                    fw_release.bump(k, pt, note=nt, log=self.log)
                self.progress(30, f"building {k}...")
                if not fw_release.build(k, log=self.log):
                    raise RuntimeError(f"{k} build failed (see log)")
                self.progress(90, "saving release...")
                return fw_release.save(k, nt, log=self.log)

            def done(out):
                self.progress(100, f"release saved: {out.name}")
                self.fill_releases(select_dir=out)
                self.on_pick_release()
            self.run(job, done)
        ttk.Button(f, text="Build", command=go, style="Big.TButton").grid(row=3, column=2, sticky="e", pady=(12, 0))

    def loader_target_kind_guess(self):
        return self.release["kind"] if self.release else "anchor"

    # ---------------- step 4 ----------------
    def step_target(self):
        r = self.release
        self.clear_body("Find the target",
                        f"Enter the id of the {r['kind']} to update (its Status self_id). The loader first tries to "
                        "wake it over LoRa, then talks to it over UWB. Keep the target within UWB range "
                        "(a few metres for 6.8 Mbps).")
        row = ttk.Frame(self.body)
        row.pack(fill="x", pady=4)
        ttk.Label(row, text="Target id:").pack(side="left")
        self.tid = tk.StringVar(value=str(getattr(self, "target_id", "")))
        ttk.Entry(row, textvariable=self.tid, width=8).pack(side="left", padx=6)
        ttk.Label(row, text="Data rate:").pack(side="left", padx=(16, 0))
        self.rate = tk.StringVar(value=next(iter(RATE_NAMES)))
        ttk.Combobox(row, textvariable=self.rate, values=list(RATE_NAMES), state="readonly", width=26).pack(side="left", padx=6)
        ttk.Button(self.body, text="Connect to target", style="Big.TButton", command=self.connect_target).pack(anchor="w", pady=8)
        self.tinfo = ttk.Label(self.body, text="", justify="left", font=("TkFixedFont", 10))
        self.tinfo.pack(anchor="w", pady=6)

    def connect_target(self):
        try:
            tid = int(self.tid.get())
        except ValueError:
            messagebox.showerror("Target id", "Enter the target's numeric id.")
            return
        self.target = None
        port = self.loader["port"]
        want_kind = self.release["kind"]

        def job():
            self.progress(10, "LoRa wake-up...")
            self.log("LoRa wake-up: not in firmware 3.1.0 yet, skipping (anchors don't sleep; a tag must be awake)",
                     "warn")
            self.progress(40, "UWB handshake...")
            with serial.Serial(port, htn_fw.BAUD, timeout=0.05) as ser:
                for attempt in range(3):
                    s = htn_fw.fota_query(ser, tid)
                    if s:
                        return s
                    self.log(f"UWB: no answer from id {tid} (attempt {attempt + 1}/3)", "warn")
            return None

        def done(s):
            self.progress(0, "idle")
            if not s:
                self.log(f"target {tid} did not answer over UWB: check id, power and distance", "err")
                self.tinfo.configure(text="no answer")
                return
            kind = KIND_NAME.get(s["t_kind"], "?")
            batt = "mains" if s["t_battery"] == 255 else f"{s['t_battery']}%"
            same = f"0x{s['t_app_crc']:08x}" == self.release["crc"]
            self.tinfo.configure(text=f"answered over UWB:\n  {kind} {tid}\n  firmware {s['t_sw']}\n  HW {s['t_hw']}\n"
                                      f"  battery {batt}\n  app CRC 0x{s['t_app_crc']:08x}")
            if kind != want_kind:
                self.log(f"target is a {kind} but the firmware is for a {want_kind}: pick another release", "err")
                return
            if s["t_battery"] != 255 and s["t_battery"] < MIN_BATTERY:
                self.log(f"target battery {s['t_battery']}% is below {MIN_BATTERY}%: charge it first", "err")
                return
            if same:
                self.log("target already runs exactly this image (same CRC); flashing again is allowed", "warn")
            self.log(f"target {kind} {tid}: {s['t_sw']}, battery {batt}", "ok")
            self.target_id = tid
            self.target = s
        self.run(job, done)

    # ---------------- step 5 ----------------
    def step_flash(self):
        r, s = self.release, self.target
        self.clear_body("Flash and verify",
                        f"Send {r['version']} ({r['size']} B) to {KIND_NAME.get(s['t_kind'])} {self.target_id} "
                        f"(now {s['t_sw']}). The target keeps running its current firmware until the whole image "
                        "has arrived and its CRC matches; then it writes flash, reboots, and the loader checks it.")
        row = ttk.Frame(self.body)
        row.pack(fill="x", pady=6)
        self.start_btn = ttk.Button(row, text="Start update", style="Big.TButton", command=self.start_flash)
        self.start_btn.pack(side="left")
        ttk.Button(row, text="Update another target", command=lambda: self.show_step(3)).pack(side="left", padx=8)
        ttk.Button(row, text="Leave FOTA loader mode", command=self.disable_loader).pack(side="left")
        self.result = ttk.Label(self.body, text="", font=("TkDefaultFont", 13, "bold"))
        self.result.pack(anchor="w", pady=12)

    def start_flash(self):
        port, tid = self.loader["port"], self.target_id
        img = self.release["path"].read_bytes()
        kind = 0 if self.release["kind"] == "anchor" else 1
        rate = htn_fw.RATES[RATE_NAMES[self.rate.get()]] if hasattr(self, "rate") else 2
        self.result.configure(text="")
        phase_pct = {"hello": 22, "countdown": 25, "verify-wait": 92, "verify": 96, "commit": 90}
        last = {"state": None}

        def job():
            t0 = time.monotonic()
            with serial.Serial(port, htn_fw.BAUD, timeout=0.05) as ser:
                self.log(f"loading {len(img)} B into the loader's RAM...")
                htn_fw.fota_load(ser, img, tid, kind, rate,
                                 progress=lambda d, t: self.progress(20 * d / t, f"loading into loader: {d}/{t} B"))

                def on_status(st, state):
                    if state != last["state"]:
                        last["state"] = state
                        self.log({"hello": "handshake with target...",
                                  "countdown": "target accepted; it starts receiving in 5 s",
                                  "data": "sending over UWB...",
                                  "commit": "all data acknowledged; target checks CRC",
                                  "verify-wait": "target is writing flash and rebooting...",
                                  "verify": "asking the rebooted target for its CRC..."}.get(state, state))
                    if state == "data":
                        pct = 25 + 65 * st["acked"] / max(1, st["size"])
                        self.progress(pct, f"UWB: {st['acked']}/{st['size']} B, {st['retries']} resent")
                    elif state in phase_pct:
                        self.progress(phase_pct[state], state)
                s = htn_fw.fota_run(ser, on_status)
            s["took"] = time.monotonic() - t0
            return s

        def done(s):
            if s["state_name"] == "done":
                self.progress(100, "done")
                msg = (f"Verified: target {tid} runs {s['t_sw']} (CRC 0x{s['t_app_crc']:08x}), "
                       f"{s['retries']} frames resent, {s['took']:.0f} s")
                self.log(msg, "ok")
                self.result.configure(text="Update OK", foreground="#2e7d32")
            else:
                err = htn_fw.err_text(s["last_err"])
                self.log(f"update FAILED: {err}. The target keeps its previous firmware unless it reported a "
                         "write; check it with 'Find the target'.", "err")
                self.result.configure(text=f"FAILED: {err}", foreground="#c62828")
                self.progress(0, "failed")
        self.run(job, done)


def main():
    app = Wizard()
    app.mainloop()


if __name__ == "__main__":
    main()
