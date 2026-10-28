#!/usr/bin/env python3
"""Build PRIV Control 0.5.4: native gallery and explicit export to Photos.

The verified 0.5.3 package supplies the unchanged theme, modules and maintainer
scripts. This script never runs iOS code or accesses device/user data.
"""
from __future__ import annotations

import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import plistlib
import shutil
import subprocess
import sys
import tempfile

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
VERSION = "0.5.4"
BASE = ROOT / "debs/PRIV_OS_0_5_3_Sileo_iphoneos-arm64.deb"
BASE_SHA256 = "9e0498b6671d11ca065a69f226926a4c9f1ec2b40f15a2abe8f4f8e5c51fd557"
BASE_SOURCE_SHA256 = "e6cd6a0c3eed3bf5d0df3d52b1c8f15ba1fa544c7b6b44bc9a52630ddb91c953"
OUTPUT = ROOT / "debs/PRIV_OS_0_5_4_Sileo_iphoneos-arm64.deb"
APP = Path("var/jb/Applications/PRIVControl.app")
THEME = Path("var/jb/Library/Themes/PRIV OS 0.5.theme")
BUILD = ROOT / "build"
EPOCH = 1793188800  # 2026-10-28 12:00:00 UTC.
WALLPAPERS = {
    "PRIV-Wallpaper-Night.png": ("1e5aebaa5f5c82827bf2a7d17432bdb43fd87b66175ec5919ea75d6637777568", (941, 1672)),
    "PRIV-Wallpaper-Classic.png": ("3b69cfb3dcf5dfe551ac3bac83a6b804160684c26b6f9a90999d7b16520fc6e6", (750, 1334)),
}


def digest(path: Path, algorithm: str = "sha256") -> str:
    return hashlib.new(algorithm, path.read_bytes()).hexdigest()


def inventory(root: Path) -> dict[str, str]:
    return {str(p.relative_to(root)): digest(p) for p in root.rglob("*") if p.is_file()}


def source_guards() -> None:
    baseline_path = ROOT / "native/baseline/PRIVControl-0.5.c"
    if digest(baseline_path) != BASE_SOURCE_SHA256:
        raise SystemExit("Historical native baseline changed.")
    baseline = baseline_path.read_text()
    current = (ROOT / "native/PRIVControl.c").read_text()
    for start, end in (("static id vaultFile(void)", "static void scrollTo"),
                       ("static void resign(", "static BOOL finish(")):
        old = baseline[baseline.index(start):baseline.index(end)]
        new = current[current.index(start):current.index(end)]
        if old != new:
            raise SystemExit("Vault/crypto/import/lock source changed: " + start)
    gallery = (ROOT / "native/WallpaperGallery.inc").read_text()
    for name, (sha256, size) in WALLPAPERS.items():
        path = ROOT / "wallpapers" / name
        if digest(path) != sha256 or sha256 not in gallery:
            raise SystemExit("Wallpaper asset or versioned export marker changed: " + name)
        with Image.open(path) as image:
            if image.size != size or image.mode not in ("RGB", "RGBA"):
                raise SystemExit("Invalid wallpaper image: " + name)
            image.verify()


def tool_environment(prefix: Path | None) -> tuple[str, str, dict[str, str]]:
    environment = dict(os.environ, SOURCE_DATE_EPOCH=str(EPOCH))
    if prefix:
        prefix = prefix.resolve()
        binary = prefix / "usr/lib/llvm-18/bin"
        library_paths = [str(prefix / "usr/lib/x86_64-linux-gnu"), str(prefix / "usr/lib/llvm-18/lib")]
        if environment.get("LD_LIBRARY_PATH"):
            library_paths.append(environment["LD_LIBRARY_PATH"])
        environment["LD_LIBRARY_PATH"] = ":".join(library_paths)
        return str(binary / "clang"), str(binary / "ld64.lld"), environment
    return os.environ.get("PRIV_CLANG", "clang"), os.environ.get("PRIV_LD64", "ld64.lld"), environment


def build_native(destination: Path, prefix: Path | None) -> dict:
    clang, linker, environment = tool_environment(prefix)
    obj = destination / "PRIVControl.o"
    executable = destination / "PRIVControl"
    commands = [
        [clang, "-target", "arm64-apple-ios15.0", "-Os", "-Wall", "-Werror=implicit-function-declaration",
         "-fPIC", "-fno-stack-protector", "-c", str(ROOT / "native/PRIVControl.c"), "-o", str(obj)],
        [linker, "-arch", "arm64", "-platform_version", "ios", "15.0", "17.5", "-syslibroot",
         str(ROOT / "native/sysroot"), "-adhoc_codesign", "-fixup_chains", "-pie", "-o", str(executable),
         str(obj), "-lSystem", "-lobjc", "-framework", "UIKit", "-framework", "Security"],
    ]
    for command in commands:
        subprocess.run(command, check=True, env=environment)
    check = subprocess.run([sys.executable, str(ROOT / "scripts/verify_native_binary.py"), str(executable)],
                           check=True, capture_output=True, text=True)
    return {"sha256": digest(executable), "bytes": executable.stat().st_size,
            "verification": json.loads(check.stdout)}


def write_indexes(control: Path) -> None:
    fields = dict(line.split(": ", 1) for line in control.read_text().splitlines())
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
        "Version: 0.5.4\nArchitectures: iphoneos-arm64\nComponents: main\n"
        "Description: PRIV OS Lab 0.5.4 rootless experimental\n"
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
        raise SystemExit("Unexpected base package; refusing to carry its executables forward.")
    source_guards()
    BUILD.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="release-0.5.4-", dir=BUILD) as temporary:
        work = Path(temporary)
        native = build_native(work, args.toolchain)
        stage = work / "package"
        subprocess.run(["dpkg-deb", "--raw-extract", str(BASE), str(stage)], check=True)
        original = inventory(stage)
        original_modes = {str(p.relative_to(stage)): p.stat().st_mode & 0o7777
                          for p in stage.rglob("*") if p.is_file()}
        executable = stage / APP / "PRIVControl"
        shutil.copyfile(work / "PRIVControl", executable)
        executable.chmod(0o755)
        info_path = stage / APP / "Info.plist"
        info = plistlib.loads(info_path.read_bytes())
        old_info = dict(info)
        info.update({
            "CFBundleShortVersionString": VERSION, "CFBundleVersion": "9",
            "NSPhotoLibraryAddUsageDescription": "Pour enregistrer le fond d'écran PRIV choisi dans Photos, sans lire tes autres photos.",
        })
        info_path.write_bytes(plistlib.dumps(info, fmt=plistlib.FMT_XML, sort_keys=False))
        allowed_keys = {"CFBundleShortVersionString", "CFBundleVersion", "NSPhotoLibraryAddUsageDescription"}
        if any(info.get(key) != value for key, value in old_info.items() if key not in allowed_keys):
            raise SystemExit("Unexpected Info.plist change.")
        (stage / APP / "Wallpapers").mkdir()
        for name in WALLPAPERS:
            shutil.copyfile(ROOT / "wallpapers" / name, stage / APP / "Wallpapers" / name)
        shutil.copyfile(ROOT / "FONDS.md", stage / "var/jb/usr/share/privos/INSTALLATION-FONDS.txt")
        control = stage / "DEBIAN/control"
        control.write_text(
            "Package: fr.privos.control\n"
            "Name: PRIV OS LAB 0.5.4 (Fonds dans Photos)\n"
            "Version: 0.5.4\nArchitecture: iphoneos-arm64\nSection: Applications\nPriority: optional\n"
            "Maintainer: PRIV OS Lab\n"
            "Depends: firmware (>= 15.0), firmware (<< 18.0), com.opa334.ccsupport\n"
            "Description: Galerie native Fonds d'ecran dans PRIV Control : apercus PRIV Nuit et Classique, enregistrement explicite dans Photos. Theme LumenBoard de 53 applications conserve, modules et format du coffre conserves.\n"
        )
        after = inventory(stage)
        changed = sorted(name for name, value in original.items() if after.get(name) != value)
        allowed_changes = {"DEBIAN/control", str(APP / "PRIVControl"), str(APP / "Info.plist")}
        if set(changed) != allowed_changes:
            raise SystemExit("Unexpected existing-file changes: " + repr(changed))
        added = sorted(set(after) - set(original))
        expected_added = {str(APP / "Wallpapers" / name) for name in WALLPAPERS}
        expected_added.add("var/jb/usr/share/privos/INSTALLATION-FONDS.txt")
        if set(added) != expected_added:
            raise SystemExit("Unexpected new files: " + repr(added))
        for name, mode in original_modes.items():
            if (stage / name).stat().st_mode & 0o7777 != mode:
                raise SystemExit("Existing file mode changed: " + name)
        for path in stage.rglob("*"):
            os.utime(path, (EPOCH, EPOCH), follow_symlinks=False)
        os.utime(stage, (EPOCH, EPOCH))
        subprocess.run(["dpkg-deb", "--root-owner-group", "-Zxz", "-z6", "--build", str(stage), str(OUTPUT)],
                       check=True, env=dict(os.environ, SOURCE_DATE_EPOCH=str(EPOCH)))
        write_indexes(control)
        report = {
            "version": VERSION, "package": str(OUTPUT.relative_to(ROOT)), "sha256": digest(OUTPUT),
            "bytes": OUTPUT.stat().st_size, "native": native,
            "original_files_changed": changed, "files_added": added,
            "theme_bundle_ids": len(list((stage / THEME / "IconBundles").glob("*.png"))),
            "theme_and_control_center_files_unchanged": True,
            "maintainer_scripts_unchanged": True,
            "vault_crypto_import_and_lock_source_unchanged": True,
            "device_test": "Not performed. Native Photos authorization and rendering must be checked on the target iPhone.",
        }
        (BUILD / "validation-0.5.4.json").write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
