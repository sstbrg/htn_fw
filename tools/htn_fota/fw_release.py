#!/usr/bin/env python3
"""Bump, build and archive Anchor/Tag firmware releases.

  fw_release.py list
  fw_release.py bump anchor [--part patch|minor|major | --set 3.2.0] [--note "what changed"]
  fw_release.py build tag                # build + save releases/tag/<version>/
  fw_release.py new anchor --part minor --note "..."   # bump + build + save
  fw_release.py import                  # unpack release zips dropped into releases/

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


def releases(log=None):
    """All saved releases, newest version first (unpacks zips dropped into releases/ first)."""
    try:
        import_zips(log or (lambda *_: None))
    except (RuntimeError, OSError, ValueError, KeyError) as e:
        if log:
            log(f"release zip not imported: {e}")
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


def released_shas(kind, ver):
    """sha256 values recorded for <kind> <ver> in existing GitHub releases (CI only; uses gh)."""
    import tempfile
    import zipfile
    lst = subprocess.run(["gh", "release", "list", "--limit", "200", "--json", "tagName", "--jq", ".[].tagName"],
                         cwd=ROOT, capture_output=True, text=True, check=True).stdout.split()
    shas = set()
    for tag in [t for t in lst if re.search(rf"(^|-){re.escape(ver)}(-|$)", t)]:
        with tempfile.TemporaryDirectory() as tmp:
            subprocess.run(["gh", "release", "download", tag, "--dir", tmp, "--pattern", "*.zip",
                            "--pattern", "*.manifest.json"], cwd=ROOT, capture_output=True)
            for z in Path(tmp).glob("*.zip"):
                with zipfile.ZipFile(z) as zf:
                    for n in zf.namelist():
                        if n.startswith(f"{kind}/") and n.endswith("/manifest.json"):
                            d = json.loads(zf.read(n))
                            if d.get("version") == ver:
                                shas.add(d["sha256"])
            for m in Path(tmp).glob(f"{ver}.manifest.json"):     # releases from before the single zip
                shas.add(json.loads(m.read_text())["sha256"])
    return shas


def ci_check_version():
    """CI, after `build`: an image may keep its version only if it is byte-identical to the
    image already released under that version. A changed image needs a VERSION bump."""
    bad = []
    for kind, p in PROJECTS.items():
        ver = version_string(kind)
        img = (ROOT / p["dir"] / "Debug" / f"{p['dir']}.bin").read_bytes()
        sha = hashlib.sha256(img).hexdigest()
        shas = released_shas(kind, ver)
        if not shas:
            print(f"{kind}: {ver} not released yet: OK")
        elif sha in shas:
            print(f"{kind}: {ver} image identical to the released one: OK")
        else:
            bad.append(f"{kind}: the {ver} image differs from the released {ver}: bump VERSION "
                       f"(tools/htn_fota/fw_release.py bump {kind})")
    for b in bad:
        print(f"::error::{b}")
    return 1 if bad else 0


def release_tag():
    return f"fw-{version_string('anchor')}-{version_string('tag')}"


def ci_publish(prerelease=False):
    """CI: publish the GitHub release for the current versions with a single asset,
    htn-fw-anc<ver>-tag<ver>.zip (anchor/<ver>/ and tag/<ver>/ with .bin/.hex/.elf/manifest).

    If the release already exists, make sure the zip is its only asset."""
    tag = release_tag()
    zpath = release_zip(tag)
    view = subprocess.run(["gh", "release", "view", tag, "--json", "assets", "--jq", ".assets[].name"],
                          cwd=ROOT, capture_output=True, text=True)
    if view.returncode == 0:
        names = view.stdout.split()
        if zpath.name not in names:
            subprocess.run(["gh", "release", "upload", tag, str(zpath)], cwd=ROOT, check=True)
            print(f"release {tag} exists: added {zpath.name}")
        for name in names:
            if name != zpath.name:
                subprocess.run(["gh", "release", "delete-asset", tag, name, "--yes"], cwd=ROOT, check=True)
                print(f"release {tag}: removed loose asset {name}")
        return 0
    notes = [f"Firmware built from {git_info()['commit']} by CI.", "",
             "| board | version | size | CRC (bootloader) | sha256 |", "|---|---|---|---|---|"]
    for kind, p in PROJECTS.items():
        ver = version_string(kind)
        man = json.loads((ROOT / "releases" / kind / ver[len(p["prefix"]):] / "manifest.json").read_text())
        notes.append(f"| {kind} | {ver} | {man['size']} B | {man['crc']} | `{man['sha256'][:16]}...` |")
    notes += ["", f"Download `{zpath.name}` and put it (or its unpacked contents) into the FOTA tools'",
              "`releases/` folder; the wizard lists it after Refresh.",
              "Flash over USB: `tools/htn_fota/htn_fw.py --port <port> flash releases/<kind>/<ver>/<Proj>.bin`.",
              "The .hex carries its own addresses (0x08200000); never write a .bin to 0x08000000 (bootloader)."]
    subprocess.run(["gh", "release", "create", tag, "--title", tag.replace("fw-", "Firmware "),
                    "--notes", "\n".join(notes), *(["--prerelease"] if prerelease else []), str(zpath)],
                   cwd=ROOT, check=True)
    print(f"published {tag} ({zpath.name})")
    return 0


def import_zips(log=print):
    """Unpack release zips dropped into releases/ (htn-fw-*.zip from a GitHub release).

    A zip holds <kind>/<version>/{<Proj>.bin,.hex,.elf,manifest.json}. Each image is
    checked against its manifest sha256; already-imported zips are skipped."""
    import zipfile
    done = []
    for z in sorted((ROOT / "releases").glob("*.zip")):
        with zipfile.ZipFile(z) as zf:
            names = zf.namelist()
            for man in [n for n in names if re.fullmatch(r"(anchor|tag)/[0-9.]+/manifest\.json", n)]:
                d = json.loads(zf.read(man))
                folder = man.rsplit("/", 1)[0]
                out = ROOT / "releases" / folder
                current = out / "manifest.json"
                if current.exists() and json.loads(current.read_text()).get("sha256") == d.get("sha256"):
                    continue
                img = zf.read(f"{folder}/{d['bin']}")
                if hashlib.sha256(img).hexdigest() != d["sha256"]:
                    raise RuntimeError(f"{z.name}: {folder}/{d['bin']} does not match its manifest sha256")
                out.mkdir(parents=True, exist_ok=True)
                for n in names:
                    if n.startswith(folder + "/") and not n.endswith("/") and "/" not in n[len(folder) + 1:]:
                        (out / n[len(folder) + 1:]).write_bytes(zf.read(n))
                log(f"imported {d['version']} from {z.name} -> releases/{folder}")
                done.append(out)
    return done


def release_zip(tag):
    """releases/<tag>.zip with anchor/<ver>/ and tag/<ver>/ of the current versions (for CI)."""
    import zipfile
    zpath = ROOT / "releases" / f"htn-{tag}.zip"
    with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as zf:
        for kind, p in PROJECTS.items():
            ver = version_string(kind)[len(p["prefix"]):]
            d = ROOT / "releases" / kind / ver
            for f in sorted(d.iterdir()):
                if f.suffix in (".bin", ".hex", ".elf") or f.name == "manifest.json":
                    zf.write(f, f"{kind}/{ver}/{f.name}")
    return zpath


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("list")
    sub.add_parser("ci-check-version", help="CI: fail if an image changed but kept a released VERSION")
    p = sub.add_parser("ci-publish", help="CI: create the GitHub release (images + zip) from releases/")
    p.add_argument("--prerelease", action="store_true")
    sub.add_parser("import", help="unpack release zips dropped into releases/")
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

    if a.cmd == "ci-check-version":
        return ci_check_version()
    if a.cmd == "ci-publish":
        return ci_publish(a.prerelease)
    if a.cmd == "import":
        if not import_zips():
            print("no new release zips in releases/")
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
