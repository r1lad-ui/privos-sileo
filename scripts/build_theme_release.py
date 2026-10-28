#!/usr/bin/env python3
"""Build the resource-only 0.5.3 update from the verified 0.5.2 package.

The native application, its signed resources and both Control Center modules
are carried forward byte for byte. No iOS command or maintainer script is run.
"""
from __future__ import annotations

import gzip
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
VERSION = "0.5.3"
BASE = ROOT / "debs/PRIV_OS_0_5_2_Sileo_iphoneos-arm64.deb"
BASE_SHA256 = "2f6168d842cff85e579cd0a83b4094ce1bb1aedc31b1d38916fb67406d740aae"
OUTPUT = ROOT / "debs/PRIV_OS_0_5_3_Sileo_iphoneos-arm64.deb"
THEME = Path("var/jb/Library/Themes/PRIV OS 0.5.theme")
BUILD = ROOT / "build"
EPOCH = 1793188800  # 2026-10-28 12:00:00 UTC; reproducible release timestamp.


def digest(path: Path, algorithm: str = "sha256") -> str:
    return hashlib.new(algorithm, path.read_bytes()).hexdigest()


def inventory(root: Path) -> dict[str, str]:
    return {str(p.relative_to(root)): digest(p) for p in root.rglob("*") if p.is_file()}


def main() -> None:
    if digest(BASE) != BASE_SHA256:
        raise SystemExit("Unexpected base package hash; refusing to reuse executable files.")
    apps = json.loads((ROOT / "assets/icons/apps.json").read_text())
    names: set[str] = set()
    bundle_ids: set[str] = set()
    for app in apps:
        png = ROOT / "assets/icons/png" / (app["slug"] + ".png")
        with Image.open(png) as image:
            if image.size != (512, 512) or image.mode not in ("RGB", "RGBA"):
                raise SystemExit(f"Invalid icon: {png}")
            image.verify()
        for bundle in app["bundle_ids"]:
            if "/" in bundle or bundle in bundle_ids:
                raise SystemExit(f"Duplicate or unsafe bundle ID: {bundle}")
            bundle_ids.add(bundle)
        for name in app["names"]:
            if "/" in name or name in names:
                raise SystemExit(f"Duplicate or unsafe icon name: {name}")
            names.add(name)

    BUILD.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="release-0.5.3-", dir=BUILD) as temporary:
        stage = Path(temporary) / "package"
        subprocess.run(["dpkg-deb", "--raw-extract", str(BASE), str(stage)], check=True)
        original = inventory(stage)
        for app in apps:
            png = ROOT / "assets/icons/png" / (app["slug"] + ".png")
            for bundle in app["bundle_ids"]:
                shutil.copyfile(png, stage / THEME / "IconBundles" / (bundle + "-large.png"))
            for name in app["names"]:
                shutil.copyfile(png, stage / THEME / "Icons" / (name + ".png"))

        wallpapers = stage / "var/jb/usr/share/privos/wallpapers"
        wallpapers.mkdir(parents=True, exist_ok=True)
        for name in ("PRIV-Wallpaper-Night.png", "PRIV-Wallpaper-Classic.png"):
            shutil.copyfile(ROOT / "wallpapers" / name, wallpapers / name)
        shutil.copyfile(ROOT / "ICONES.md", stage / "var/jb/usr/share/privos/INSTALLATION-THEME.txt")

        control = stage / "DEBIAN/control"
        control.write_text(
            "Package: fr.privos.control\n"
            "Name: PRIV OS LAB 0.5.3 (icones et fonds)\n"
            "Version: 0.5.3\n"
            "Architecture: iphoneos-arm64\n"
            "Section: Applications\n"
            "Priority: optional\n"
            "Maintainer: PRIV OS Lab\n"
            "Depends: firmware (>= 15.0), firmware (<< 18.0), com.opa334.ccsupport\n"
            "Description: Extension du theme PRIV OS pour LumenBoard : messageries, Proton, finance, apps Apple et utilitaires. Fonds nuit et classique. Calendrier sans date figee. Application et modules conserves.\n"
        )

        after = inventory(stage)
        changed = sorted(name for name, value in original.items() if after.get(name) != value)
        allowed_changes = {
            "DEBIAN/control",
            str(THEME / "IconBundles/com.apple.mobilecal-large.png"),
            str(THEME / "Icons/Calendrier.png"),
        }
        if set(changed) - allowed_changes:
            raise SystemExit("Unexpected existing-file changes: " + repr(changed))
        for tree in ("var/jb/Applications", "var/jb/Library/ControlCenter"):
            if any(after.get(name) != value for name, value in original.items() if name.startswith(tree + "/")):
                raise SystemExit("Native app or Control Center files were changed.")
        # Repack resources only; retain file permissions and root ownership.
        for path in stage.rglob("*"):
            os.utime(path, (EPOCH, EPOCH), follow_symlinks=False)
        os.utime(stage, (EPOCH, EPOCH))
        environment = dict(os.environ, SOURCE_DATE_EPOCH=str(EPOCH))
        subprocess.run(["dpkg-deb", "--root-owner-group", "-Zxz", "-z6", "--build", str(stage), str(OUTPUT)],
                       check=True, env=environment)

        # Sileo index: keep one current version, preserving older .deb downloads.
        fields = {}
        for line in control.read_text().splitlines():
            key, value = line.split(": ", 1)
            fields[key] = value
        package_lines = [f"{key}: {fields[key]}" for key in ("Package", "Version", "Architecture", "Maintainer", "Depends")]
        package_lines += [
            f"Filename: {OUTPUT.relative_to(ROOT)}", f"Size: {OUTPUT.stat().st_size}",
            f"MD5sum: {digest(OUTPUT, 'md5')}", f"SHA1: {digest(OUTPUT, 'sha1')}", f"SHA256: {digest(OUTPUT)}",
        ]
        package_lines += [f"{key}: {fields[key]}" for key in ("Section", "Priority", "Name", "Description")]
        package_bytes = ("\n".join(package_lines) + "\n").encode()
        (ROOT / "Packages").write_bytes(package_bytes)
        (ROOT / "Packages.gz").write_bytes(gzip.compress(package_bytes, mtime=0))
        release = (
            "Origin: PRIV OS Lab\nLabel: PRIV OS Lab\nSuite: stable\nCodename: privos\n"
            "Version: 0.5.3\nArchitectures: iphoneos-arm64\nComponents: main\n"
            "Description: PRIV OS Lab 0.5.3 rootless experimental\n"
            "Date: Wed, 28 Oct 2026 12:00:00 +0000\n"
        )
        for algorithm, header in (("md5", "MD5Sum"), ("sha256", "SHA256")):
            release += header + ":\n"
            for name in ("Packages", "Packages.gz"):
                p = ROOT / name
                release += f" {digest(p, algorithm)} {p.stat().st_size} {name}\n"
        (ROOT / "Release").write_text(release)
        report = {
            "version": VERSION, "package": str(OUTPUT.relative_to(ROOT)), "sha256": digest(OUTPUT),
            "bytes": OUTPUT.stat().st_size, "extension_designs": len(apps),
            "bundle_ids_total": len(list((stage / THEME / "IconBundles").glob("*.png"))),
            "original_files_changed": changed,
            "native_application_and_modules_unchanged": True,
            "maintainer_scripts_unchanged": all(after[f"DEBIAN/{p}"] == original[f"DEBIAN/{p}"] for p in ("postinst", "postrm")),
            "device_test": "Not performed. Reapply in LumenBoard and verify on the iPhone.",
        }
        (BUILD / "validation-0.5.3.json").write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
