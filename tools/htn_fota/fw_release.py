#!/usr/bin/env python3
"""Bump, build and archive Anchor/Tag firmware releases.

  fw_release.py list
  fw_release.py bump anchor [--part patch|minor|major | --set 3.2.0] [--note "what changed"]
  fw_release.py build tag                # build + save releases/tag/<version>/
  fw_release.py new anchor --part minor --note "..."   # bump + build + save
  fw_release.py publish anchor          # commit the bump, push, CI publishes the GitHub release

A release directory holds the .bin/.hex/.elf/.map and manifest.json
(version, bootloader CRC, size, sha256, git commit, toolchain, date).
The version lives in <Project>/Core/Inc/main.h as `#define VERSION "anc3.1.0"`
(tag: "tag3.1.0"); the status reply carries it in a 9-byte field.
"""
import argparse
import datetime
import hashlib
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]   # repo root (tools/htn_fota/..)
sys.path.insert(0, str(Path(__file__).resolve().parent))
import htn_fw  # noqa: E402  (image_crc)

PROJECTS = {
    "anchor": {"dir": "Anchor", "prefix": "anc"},
    "tag": {"dir": "Tag", "prefix": "tag"},
}
VERSION_RE = re.compile(r'#define VERSION "([a-zA-Z]+)(\d+)\.(\d+)\.(\d+)([^"]*)"')
SW_FIELD = 9  # bytes in the status reply


def main_h(kind):
    return ROOT / PROJECTS[kind]["dir"] / "Core" / "Inc" / "main.h"


def read_version(kind):
    text = main_h(kind).read_bytes().decode("latin-1")
    m = VERSION_RE.search(text)
    if not m:
        raise SystemExit(f"{main_h(kind)}: no #define VERSION \"<prefix>X.Y.Z\" found")
    return m.group(1), tuple(int(m.group(i)) for i in (2, 3, 4)), m.group(5)


def version_string(kind):
    prefix, (a, b, c), suffix = read_version(kind)
    return f"{prefix}{a}.{b}.{c}{suffix}"


def bump(kind, part="patch", set_to=None, note=None, log=print):
    """Rewrite VERSION in main.h (keeps the file's line endings); returns the new string."""
    path = main_h(kind)
    raw = path.read_bytes()
    text = raw.decode("latin-1")
    old_prefix, (a, b, c), _ = read_version(kind)
    prefix = PROJECTS[kind]["prefix"]
    if set_to:
        a, b, c = (int(x) for x in set_to.split("."))
    elif part == "major":
        a, b, c = a + 1, 0, 0
    elif part == "minor":
        b, c = b + 1, 0
    else:
        c += 1
    new = f"{prefix}{a}.{b}.{c}"
    if len(new) > SW_FIELD:
        raise SystemExit(f"{new!r} is longer than the {SW_FIELD}-byte status SW field")
    text = VERSION_RE.sub(f'#define VERSION "{new}"', text, count=1)
    if note:
        # changelog: last line of the version comment block before "*/"
        eol = "\r\n" if "\r\n" in text else "\n"
        i = text.find("#define VERSION")
        j = text.rfind("*/", 0, i)
        if j > 0:
            text = text[:j] + f"* {new}\t{note}{eol} " + text[j:]
    path.write_bytes(text.encode("latin-1"))
    log(f"{path.relative_to(ROOT)}: VERSION -> {new}")
    return new


def build(kind, log=print):
    """Run tools/build.sh; streams output lines to log(); returns True on success."""
    proj = PROJECTS[kind]["dir"]
    p = subprocess.Popen([str(ROOT / "tools" / "build.sh"), proj], cwd=ROOT,
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    errors = 0
    for line in p.stdout:
        line = line.rstrip()
        if " error" in line or "Error " in line or "Finished building target" in line or "warning:" in line:
            log(line)
        errors += " error" in line
    return p.wait() == 0 and errors == 0


def git_info():
    def run(*a):
        return subprocess.run(["git", *a], cwd=ROOT, capture_output=True, text=True).stdout.strip()
    return {"commit": run("rev-parse", "--short", "HEAD"), "branch": run("rev-parse", "--abbrev-ref", "HEAD"),
            "dirty": bool(run("status", "--porcelain", "--untracked-files=no", "--", "Anchor", "Tag"))}


def save(kind, note=None, log=print):
    """Copy the current build outputs to releases/<kind>/<version>/ and write manifest.json."""
    proj = PROJECTS[kind]["dir"]
    dbg = ROOT / proj / "Debug"
    binf = dbg / f"{proj}.bin"
    if not binf.exists():
        raise SystemExit(f"{binf} missing: build first")
    img = binf.read_bytes()
    ver = version_string(kind)
    if ver.encode() not in img:
        raise SystemExit(f"{binf} does not contain {ver!r}: rebuild after bumping")
    out = ROOT / "releases" / kind / ver[len(PROJECTS[kind]["prefix"]):]
    out.mkdir(parents=True, exist_ok=True)
    for ext in ("bin", "hex", "elf", "map"):
        f = dbg / f"{proj}.{ext}"
        if f.exists():
            shutil.copy2(f, out / f.name)
    tool = subprocess.run(["arm-none-eabi-gcc", "--version"], capture_output=True, text=True).stdout.splitlines()
    manifest = {
        "kind": kind, "version": ver, "size": len(img), "crc": f"0x{htn_fw.image_crc(img):08x}",
        "sha256": hashlib.sha256(img).hexdigest(), "built": datetime.datetime.now().isoformat(timespec="seconds"),
        "git": git_info(), "toolchain": tool[0] if tool else "?", "note": note or "",
        "bin": binf.name,
    }
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    log(f"saved {out.relative_to(ROOT)}  ({len(img)} B, crc {manifest['crc']}"
        + (", working tree had uncommitted changes" if manifest["git"]["dirty"] else "") + ")")
    return out


def releases():
    """All saved releases, newest version first."""
    rel = []
    for m in (ROOT / "releases").glob("*/*/manifest.json"):
        try:
            d = json.loads(m.read_text())
        except ValueError:
            continue
        d["dir"] = m.parent
        d["path"] = m.parent / d.get("bin", "")
        rel.append(d)

    def key(d):
        nums = re.findall(r"\d+", d["version"])
        return (d["kind"], tuple(int(n) for n in nums))
    return sorted(rel, key=key, reverse=True)


SOURCES = ("Core", "Debug", "Drivers", "Middlewares")  # per project; a change here needs a VERSION bump


def ci_check_bump(base):
    """Fail if a project's sources changed against `base` but its VERSION did not."""
    bad = []
    for kind, p in PROJECTS.items():
        paths = [f"{p['dir']}/{s}" for s in SOURCES] + [f"{p['dir']}/*.ld"]
        changed = subprocess.run(["git", "diff", "--quiet", f"{base}...HEAD", "--", *paths], cwd=ROOT).returncode != 0
        old = subprocess.run(["git", "show", f"{base}:{p['dir']}/Core/Inc/main.h"], cwd=ROOT,
                             capture_output=True).stdout.decode("latin-1")
        m = VERSION_RE.search(old)
        old_ver = f"{m.group(1)}{m.group(2)}.{m.group(3)}.{m.group(4)}{m.group(5)}" if m else None
        new_ver = version_string(kind)
        print(f"{kind}: sources {'changed' if changed else 'unchanged'}, VERSION {old_ver} -> {new_ver}")
        if changed and old_ver == new_ver:
            bad.append(f"{kind} sources changed but VERSION is still {new_ver}: bump it (tools/htn_fota/fw_release.py bump {kind})")
    for b in bad:
        print(f"::error::{b}")
    return 1 if bad else 0


def release_tag():
    return f"fw-{version_string('anchor')}-{version_string('tag')}"


def ci_publish(prerelease=False):
    """Create the GitHub release for the current versions from releases/ (after `build`). Skips if it exists."""
    tag = release_tag()
    if subprocess.run(["gh", "release", "view", tag], cwd=ROOT, capture_output=True).returncode == 0:
        print(f"release {tag} already exists: versions not bumped, nothing published")
        return 0
    assets, notes = [], [f"Firmware built from {git_info()['commit']} by CI.", "", "| board | version | size | CRC (bootloader) | sha256 |", "|---|---|---|---|---|"]
    stage = ROOT / "releases" / "_publish"
    stage.mkdir(parents=True, exist_ok=True)
    for kind, p in PROJECTS.items():
        ver = version_string(kind)
        d = ROOT / "releases" / kind / ver[len(p["prefix"]):]
        man = json.loads((d / "manifest.json").read_text())
        for ext in ("bin", "hex", "elf"):
            src = d / f"{p['dir']}.{ext}"
            dst = stage / f"{ver}.{ext}"
            shutil.copy2(src, dst)
            assets.append(dst)
        mdst = stage / f"{ver}.manifest.json"
        shutil.copy2(d / "manifest.json", mdst)
        assets.append(mdst)
        notes.append(f"| {kind} | {ver} | {man['size']} B | {man['crc']} | `{man['sha256'][:16]}…` |")
    notes += ["", "Flash over USB: `tools/htn_fota/htn_fw.py --port <port> flash <ver>.bin`.",
              "Over the air: `tools/htn_fota/fw_wizard.py` (Download from GitHub), or `tools/htn_fota/htn_fw.py fota <id> <ver>.bin`.",
              "The .hex carries its own addresses (0x08200000); never write a .bin to 0x08000000 (bootloader)."]
    subprocess.run(["gh", "release", "create", tag, "--title", tag.replace("fw-", "Firmware "),
                    "--notes", "\n".join(notes), *(["--prerelease"] if prerelease else []), *map(str, assets)],
                   cwd=ROOT, check=True)
    print(f"published {tag}")
    return 0


def publish(kind, log=print):
    """Commit the VERSION bump of `kind`, push the branch and let CI build and publish the GitHub release.

    The release must match committed code, so this refuses if anything else in
    the firmware projects is uncommitted. On main the push itself triggers the
    release; on other branches the workflow is started by hand (pre-release),
    which GitHub allows once .github/workflows/firmware.yml is on main.
    Returns a short description of what was started."""
    def git(*a, check=True):
        r = subprocess.run(["git", *a], cwd=ROOT, capture_output=True, text=True)
        if check and r.returncode:
            raise RuntimeError(f"git {' '.join(a)}: {r.stderr.strip() or r.stdout.strip()}")
        return r.stdout.rstrip()    # keep leading spaces: `status --porcelain` lines start with " M"
    mh = str(main_h(kind).relative_to(ROOT))
    dirty = [l[3:] for l in git("status", "--porcelain", "--untracked-files=no", "--", "Anchor", "Tag",
                                "bootloader_hitan").splitlines()]
    others = [f for f in dirty if f != mh]
    if others:
        raise RuntimeError("commit or stash these firmware changes first, the release must match committed code: "
                           + ", ".join(others[:5]) + (" ..." if len(others) > 5 else ""))
    ver = version_string(kind)
    branch = git("rev-parse", "--abbrev-ref", "HEAD")
    if mh in dirty:
        git("commit", "-m", f"release: {ver}", "--", mh)
        log(f"committed version bump {ver} on {branch}")
    git("push")
    log(f"pushed {branch}")
    tag = release_tag()
    if branch == "main":
        return f"pushed main: CI builds and publishes {tag}"
    r = subprocess.run(["gh", "workflow", "run", "firmware.yml", "--ref", branch], cwd=ROOT,
                       capture_output=True, text=True)
    if r.returncode:
        raise RuntimeError("could not start the CI release workflow (it must already be on main): "
                           + (r.stderr.strip() or r.stdout.strip()))
    return f"CI started on {branch}: it builds and publishes pre-release {tag} (see GitHub Actions)"


def fetch(tag=None, log=print):
    """Download GitHub release assets into releases/<kind>/<version>/ (latest release if tag is None)."""
    import tempfile
    args = ["gh", "release", "download"] + ([tag] if tag else []) + ["--pattern", "*.bin", "--pattern", "*.manifest.json",
                                                                     "--pattern", "*.hex", "--pattern", "*.elf"]
    got = []
    with tempfile.TemporaryDirectory() as tmp:
        subprocess.run(args + ["--dir", tmp], cwd=ROOT, check=True, capture_output=True, text=True)
        for man in Path(tmp).glob("*.manifest.json"):
            d = json.loads(man.read_text())
            kind, ver = d["kind"], d["version"]
            out = ROOT / "releases" / kind / ver[len(PROJECTS[kind]["prefix"]):]
            out.mkdir(parents=True, exist_ok=True)
            for ext in ("bin", "hex", "elf"):
                f = Path(tmp) / f"{ver}.{ext}"
                if f.exists():
                    shutil.copy2(f, out / f"{PROJECTS[kind]['dir']}.{ext}")
            d["bin"] = f"{PROJECTS[kind]['dir']}.bin"
            img = (out / d["bin"]).read_bytes()
            if hashlib.sha256(img).hexdigest() != d["sha256"]:
                raise SystemExit(f"{ver}: downloaded image does not match its manifest sha256")
            d["note"] = (d.get("note") or "") + " [GitHub]"
            (out / "manifest.json").write_text(json.dumps(d, indent=2) + "\n")
            log(f"fetched {ver} -> {out.relative_to(ROOT)}")
            got.append(out)
    return got


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("list")
    p = sub.add_parser("ci-check-bump", help="CI: fail if sources changed without a VERSION bump")
    p.add_argument("base")
    p = sub.add_parser("ci-publish", help="CI: create the GitHub release from releases/")
    p.add_argument("--prerelease", action="store_true")
    p = sub.add_parser("publish", help="commit the version bump, push, and let CI publish the GitHub release")
    p.add_argument("kind", choices=PROJECTS)
    p = sub.add_parser("fetch", help="download a GitHub release into releases/")
    p.add_argument("tag", nargs="?")
    for name in ("bump", "new"):
        p = sub.add_parser(name)
        p.add_argument("kind", choices=PROJECTS)
        g = p.add_mutually_exclusive_group()
        g.add_argument("--part", choices=("patch", "minor", "major"), default="patch")
        g.add_argument("--set", dest="set_to", metavar="X.Y.Z")
        p.add_argument("--note")
    p = sub.add_parser("build")
    p.add_argument("kind", choices=PROJECTS)
    p.add_argument("--note")
    a = ap.parse_args()

    if a.cmd == "ci-check-bump":
        return ci_check_bump(a.base)
    if a.cmd == "ci-publish":
        return ci_publish(a.prerelease)
    if a.cmd == "publish":
        print(publish(a.kind))
        return 0
    if a.cmd == "fetch":
        fetch(a.tag)
        return 0
    if a.cmd == "list":
        for d in releases():
            flag = " (dirty tree)" if d.get("git", {}).get("dirty") else ""
            print(f"{d['kind']:6} {d['version']:10} {d['size']:7} B  crc {d['crc']}  {d['built']}  "
                  f"git {d['git']['commit']}{flag}  {d.get('note', '')}")
        return 0
    if a.cmd in ("bump", "new"):
        bump(a.kind, a.part, a.set_to, a.note)
        if a.cmd == "bump":
            return 0
    if not build(a.kind):
        print("build FAILED")
        return 1
    save(a.kind, a.note)
    return 0


if __name__ == "__main__":
    sys.exit(main())
