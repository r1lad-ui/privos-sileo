#!/usr/bin/env python3
"""Build 0.5.5: honest local security diagnostics and a private app preview.

Only the native executable, bundle versions and package metadata are replaced.
The wallpaper module, theme, Control Center modules and install scripts remain
unchanged. The package does not change iOS passcodes or device preferences.
"""
from __future__ import annotations

import argparse
import gzip
import json
import os
from pathlib import Path
import plistlib
import shutil
import subprocess
import sys
import tempfile

from build_wallpaper_release import (
    APP, BUILD, EPOCH, ROOT, build_native, digest, inventory, source_guards,
)

VERSION = "0.5.5"
BASE = ROOT / "debs/PRIV_OS_0_5_4_Sileo_iphoneos-arm64.deb"
BASE_SHA256 = "2e8f29fbf697a03203bb675afd36db26ac09cb367358ad435c3dde79eee88737"
OUTPUT = ROOT / "debs/PRIV_OS_0_5_5_Sileo_iphoneos-arm64.deb"
GALLERY_SHA256 = "0771e803f73aec734d3aae0a28e71b80d03a624b8d3997f6f30b0e37819d504b"
GUIDE = "var/jb/usr/share/privos/SECURITE.txt"


def write_indexes(control: Path) -> None:
    fields = dict(line.split(": ", 1) for line in control.read_text().splitlines())
    lines = [f"{key}: {fields[key]}" for key in ("Package", "Version", "Architecture", "Maintainer", "Depends")]
    lines += [f"Filename: {OUTPUT.relative_to(ROOT)}", f"Size: {OUTPUT.stat().st_size}",
              f"MD5sum: {digest(OUTPUT, 'md5')}", f"SHA1: {digest(OUTPUT, 'sha1')}", f"SHA256: {digest(OUTPUT)}"]
    lines += [f"{key}: {fields[key]}" for key in ("Section", "Priority", "Name", "Description")]
    package_bytes = ("\n".join(lines) + "\n").encode()
    (ROOT / "Packages").write_bytes(package_bytes)
    (ROOT / "Packages.gz").write_bytes(gzip.compress(package_bytes, mtime=0))
    release = (
        "Origin: PRIV OS Lab\nLabel: PRIV OS Lab\nSuite: stable\nCodename: privos\n"
        "Version: 0.5.5\nArchitectures: iphoneos-arm64\nComponents: main\n"
        "Description: PRIV OS Lab 0.5.5 rootless experimental\n"
        "Date: Wed, 28 Oct 2026 12:00:00 +0000\n"
    )
    for algorithm, header in (("md5", "MD5Sum"), ("sha256", "SHA256")):
        release += header + ":\n"
        for name in ("Packages", "Packages.gz"):
            path = ROOT / name
            release += f" {digest(path, algorithm)} {path.stat().st_size} {name}\n"
    (ROOT / "Release").write_text(release)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--toolchain", type=Path, help="Extracted LLVM 18 Ubuntu root; otherwise use PRIV_CLANG/PRIV_LD64 or PATH.")
    args = parser.parse_args()
    if digest(BASE) != BASE_SHA256:
        raise SystemExit("Unexpected 0.5.4 base package hash.")
    source_guards()
    if digest(ROOT / "native/WallpaperGallery.inc") != GALLERY_SHA256:
        raise SystemExit("The working 0.5.4 wallpaper module changed.")
    current = (ROOT / "native/PRIVControl.c").read_text()
    if not current.startswith("/* PRIV Control 0.5.5") or '#include "SecurityCenter.inc"' not in current:
        raise SystemExit("Expected native 0.5.5 source and security module.")
    BUILD.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="release-0.5.5-", dir=BUILD) as temporary:
        work = Path(temporary)
        native = build_native(work, args.toolchain)
        stage = work / "package"
        subprocess.run(["dpkg-deb", "--raw-extract", str(BASE), str(stage)], check=True)
        original = inventory(stage)
        modes = {str(path.relative_to(stage)): path.stat().st_mode & 0o7777
                 for path in stage.rglob("*") if path.is_file()}
        executable = stage / APP / "PRIVControl"
        shutil.copyfile(work / "PRIVControl", executable)
        executable.chmod(0o755)
        info_path = stage / APP / "Info.plist"
        info = plistlib.loads(info_path.read_bytes())
        info["CFBundleShortVersionString"] = VERSION
        info["CFBundleVersion"] = "10"
        info_path.write_bytes(plistlib.dumps(info, fmt=plistlib.FMT_XML, sort_keys=False))
        shutil.copyfile(ROOT / "SECURITE.md", stage / GUIDE)
        control = stage / "DEBIAN/control"
        control.write_text(
            "Package: fr.privos.control\n"
            "Name: PRIV OS LAB 0.5.5 (Securite et fonds)\n"
            "Version: 0.5.5\nArchitecture: iphoneos-arm64\nSection: Applications\nPriority: optional\n"
            "Maintainer: PRIV OS Lab\n"
            "Depends: firmware (>= 15.0), firmware (<< 18.0), com.opa334.ccsupport\n"
            "Description: Ecran Securite : disponibilite du code et de Touch ID declaree par iOS, guide de verrouillage, contenu PRIV masque hors activite. Aucun changement automatique des reglages iOS. Fonds Photos et theme LumenBoard conserves.\n"
        )
        after = inventory(stage)
        changed = sorted(name for name, value in original.items() if after.get(name) != value)
        expected = {"DEBIAN/control", str(APP / "PRIVControl"), str(APP / "Info.plist")}
        if set(changed) != expected or set(after) - set(original) != {GUIDE}:
            raise SystemExit("Unexpected package changes: " + repr(changed))
        for name, mode in modes.items():
            if (stage / name).stat().st_mode & 0o7777 != mode:
                raise SystemExit("Existing file permission changed: " + name)
        for path in stage.rglob("*"):
            os.utime(path, (EPOCH, EPOCH), follow_symlinks=False)
        os.utime(stage, (EPOCH, EPOCH))
        subprocess.run(["dpkg-deb", "--root-owner-group", "-Zxz", "-z6", "--build", str(stage), str(OUTPUT)],
                       check=True, env=dict(os.environ, SOURCE_DATE_EPOCH=str(EPOCH)))
        verify = subprocess.run([
            sys.executable, str(ROOT / "scripts/verify_release.py"),
            "--base", str(BASE), "--package", str(OUTPUT), "--version", VERSION, "--allow-added", GUIDE,
        ], check=True, capture_output=True, text=True)
        package_verification = json.loads(verify.stdout)
        write_indexes(control)
        report = {
            "version": VERSION, "package": str(OUTPUT.relative_to(ROOT)),
            "bytes": OUTPUT.stat().st_size, "sha256": digest(OUTPUT), "native": native,
            "package_verification": package_verification,
            "vault_crypto_import_and_original_resign_unchanged": True,
            "wallpaper_module_unchanged": True,
            "device_test": "Not performed. iOS diagnostic, real app-switcher snapshot and Photos need confirmation on the phone.",
        }
        (BUILD / "validation-0.5.5.json").write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
