#!/usr/bin/env python3
"""Read-only verification of a PRIV Control .deb against its approved base.

Only control metadata, the native executable and its Info.plist may change.
--allow-added declares exact additional regular files; all declared additions
are required. Ancestor directories for those files may be added. No package
or maintainer script is installed or executed; dpkg-deb only reads archives.

Example:
  python3 scripts/verify_release.py --base debs/previous.deb \
    --package debs/current.deb --version 0.5.5 \
    --allow-added var/jb/usr/share/privos/SECURITE.txt
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import plistlib
import subprocess
import sys
import tarfile
import tempfile

from PIL import Image

from verify_native_binary import VerificationError, require, verify_native_binary


APP = "var/jb/Applications/PRIVControl.app"
THEME = "var/jb/Library/Themes/PRIV OS 0.5.theme/"
CONTROL_CENTER = "var/jb/Library/ControlCenter/"
ALLOWED_CHANGED = {"DEBIAN/control", f"{APP}/PRIVControl", f"{APP}/Info.plist"}
CONTROL_METADATA = {"Version", "Name", "Description", "Installed-Size"}
INFO_VERSION_KEYS = {"CFBundleShortVersionString", "CFBundleVersion"}
PHOTO_KEYS = {"NSPhotoLibraryUsageDescription", "NSPhotoLibraryAddUsageDescription"}
MAINTAINER_NAMES = {"preinst", "postinst", "prerm", "postrm", "config"}
# Reviewed scripts only register/unregister this app with uicache, then exit.
# Pinning them prevents an arbitrary base from silently approving global
# preferences, device-lock changes, launch daemons or forced restarts.
MAINTAINER_SHA256 = {
    "DEBIAN/postinst": "b54c745d97fbf0f08685955787acba564601bbdca5650460f3296983ca4eca03",
    "DEBIAN/postrm": "e6d2ed8cde2e4e6f35ccd86a4760ec2dba8ccde6fd5605c3c47479e241dfbdd5",
}


@dataclass(frozen=True)
class Entry:
    kind: str
    mode: int
    uid: int
    gid: int
    data: bytes = b""
    link: str = ""

    def metadata(self) -> tuple:
        return self.kind, self.mode, self.uid, self.gid, self.link


def normalized_name(name: str) -> str:
    path = PurePosixPath(name)
    require(not path.is_absolute() and ".." not in path.parts,
            f"Unsafe archive path: {name}")
    return str(path)


def archive_entries(package: Path) -> dict[str, Entry]:
    """Read control/data tar streams without extracting executable files."""
    result: dict[str, Entry] = {}
    for option, prefix in (("--ctrl-tarfile", "DEBIAN/"), ("--fsys-tarfile", "")):
        process = subprocess.run(["dpkg-deb", option, str(package)], check=True,
                                 capture_output=True)
        with tarfile.open(fileobj=io.BytesIO(process.stdout), mode="r:") as archive:
            for member in archive:
                relative = normalized_name(member.name)
                if relative == ".":
                    continue
                name = prefix + relative
                require(name not in result, f"Duplicate archive entry: {name}")
                if member.isdir():
                    kind, contents = "directory", b""
                elif member.isreg():
                    kind = "file"
                    stream = archive.extractfile(member)
                    require(stream is not None, f"Unreadable archive file: {name}")
                    contents = stream.read()
                    require(len(contents) == member.size, f"Truncated archive file: {name}")
                elif member.issym():
                    kind, contents = "symlink", b""
                else:
                    raise VerificationError(f"Unsupported special file/hardlink: {name}")
                result[name] = Entry(kind, member.mode, member.uid, member.gid,
                                     contents, member.linkname if kind == "symlink" else "")
    return result


def regular_file(entries: dict[str, Entry], name: str) -> bytes:
    require(name in entries and entries[name].kind == "file", f"Missing regular file: {name}")
    return entries[name].data


def control_fields(contents: bytes) -> dict[str, str]:
    fields: dict[str, str] = {}
    current: str | None = None
    for line in contents.decode("utf-8").splitlines():
        if not line:
            continue
        if line[0].isspace():
            require(current is not None, "Invalid Debian control continuation")
            fields[current] += "\n" + line
            continue
        require(":" in line, "Invalid Debian control field")
        key, value = line.split(":", 1)
        require(key and key not in fields, f"Duplicate/empty Debian control field: {key}")
        fields[key] = value.strip()
        current = key
    return fields


def verify_inventory(before: dict[str, Entry], after: dict[str, Entry],
                     allowed_added: set[str]) -> dict:
    missing = sorted(set(before) - set(after))
    require(not missing, f"Existing entries removed: {missing}")
    changed: list[str] = []
    for name, old in before.items():
        new = after[name]
        require(old.metadata() == new.metadata(), f"Existing type/mode/owner/link changed: {name}")
        if old.data != new.data:
            changed.append(name)
    require(set(changed) <= ALLOWED_CHANGED,
            f"Unexpected existing-file changes: {sorted(set(changed) - ALLOWED_CHANGED)}")
    added = sorted(set(after) - set(before))
    added_files = {name for name in added if after[name].kind != "directory"}
    require(added_files == allowed_added,
            f"New files differ from --allow-added: found={sorted(added_files)}, expected={sorted(allowed_added)}")
    ancestors = {str(parent) for name in allowed_added for parent in PurePosixPath(name).parents}
    for name in added:
        entry = after[name]
        require(entry.uid == 0 and entry.gid == 0, f"New entry is not root-owned: {name}")
        if entry.kind == "directory":
            require(name in ancestors and entry.mode == 0o755,
                    f"Unexpected new directory or directory mode: {name}")
        else:
            require(entry.kind == "file" and entry.mode == 0o644,
                    f"New entry must be a non-executable regular file: {name}")
    # No additions are authorized implicitly. The release caller must list
    # every permitted resource. No runtime behavior is inferred from C.
    return {"existing_files_changed": sorted(changed),
            "files_added": sorted(added_files),
            "directories_added": [name for name in added if after[name].kind == "directory"]}


def verify_control(before: dict[str, Entry], after: dict[str, Entry], version: str) -> tuple[dict, dict]:
    old = control_fields(regular_file(before, "DEBIAN/control"))
    new = control_fields(regular_file(after, "DEBIAN/control"))
    require(new.get("Package") == old.get("Package") == "fr.privos.control", "Unexpected package identity")
    require(new.get("Architecture") == old.get("Architecture") == "iphoneos-arm64", "Unexpected package architecture")
    require(new.get("Version") == version, "Package Version differs from --version")
    for key in set(old) | set(new):
        if key not in CONTROL_METADATA:
            require(old.get(key) == new.get(key), f"Unexpected Debian control change: {key}")
    require(bool(new.get("Maintainer")), "Missing package maintainer")
    return old, new


def verify_info(before: dict[str, Entry], after: dict[str, Entry], version: str) -> dict:
    path = f"{APP}/Info.plist"
    old = plistlib.loads(regular_file(before, path))
    new = plistlib.loads(regular_file(after, path))
    require(isinstance(old, dict) and isinstance(new, dict), "Invalid application Info.plist")
    require(new.get("CFBundleShortVersionString") == version, "App version differs from --version")
    require(new.get("CFBundleIdentifier") == "fr.privos.control", "Application identity changed")
    require(new.get("CFBundleExecutable") == "PRIVControl", "Application executable changed")
    require(new.get("MinimumOSVersion") == "15.0", "Application minimum iOS changed")
    require(bool(new.get("CFBundleVersion")), "Missing application build version")
    for key in PHOTO_KEYS:
        require(isinstance(new.get(key), str) and bool(new[key].strip()), f"Missing Photos usage description: {key}")
        if key in old:
            require(old[key] == new[key], f"Existing Photos usage description changed: {key}")
    changed = sorted(key for key in set(old) | set(new) if old.get(key) != new.get(key))
    # Supports verifying the 0.5.4 transition that first adds the add-only key.
    optional_additions = {"NSPhotoLibraryAddUsageDescription"} - set(old)
    require(set(changed) <= INFO_VERSION_KEYS | optional_additions,
            f"Unexpected Info.plist keys changed: {sorted(set(changed) - INFO_VERSION_KEYS - optional_additions)}")
    return {"keys_changed": changed, "photos_usage_descriptions_preserved": True}


def verify_theme(before: dict[str, Entry], after: dict[str, Entry]) -> dict:
    files = [name for name, value in after.items() if name.startswith(THEME) and value.kind == "file"]
    icons = [name for name in files if name.startswith(THEME + "IconBundles/") and name.endswith(".png")]
    require(len(icons) == 53, f"Expected 53 theme bundle IDs, found {len(icons)}")
    for name in files:
        require(name in before and before[name] == after[name], f"Theme resource changed: {name}")
    for name in icons:
        with Image.open(io.BytesIO(after[name].data)) as image:
            # PRIVControl's original icon is 180px; the other 52 are 512px.
            size = (180, 180) if name == THEME + "IconBundles/fr.privos.control-large.png" else (512, 512)
            require(image.format == "PNG" and image.size == size and image.mode in ("RGB", "RGBA"),
                    f"Invalid icon format: {name}")
            image.load()
            require(any(low != high for low, high in image.convert("RGB").getextrema()), f"Empty/solid-color icon: {name}")
            if image.mode == "RGBA":
                require(image.getchannel("A").getextrema()[1] > 0, f"Fully transparent icon: {name}")
    return {"bundle_ids": len(icons), "files_preserved": len(files), "unchanged": True}


def verify_release(base: Path | str, package: Path | str, version: str,
                   allowed_added: set[str] | None = None) -> dict:
    base, package = Path(base), Path(package)
    additions = {normalized_name(name) for name in (allowed_added or set())}
    require(not any(name == "." or name.startswith("DEBIAN/") for name in additions),
            "--allow-added cannot authorize installer/control entries")
    before, after = archive_entries(base), archive_entries(package)
    inventory = verify_inventory(before, after, additions)
    old_control, new_control = verify_control(before, after, version)
    info = verify_info(before, after, version)
    theme = verify_theme(before, after)
    cc_files = [name for name, entry in before.items() if name.startswith(CONTROL_CENTER) and entry.kind == "file"]
    require(bool(cc_files), "Base package has no Control Center modules")
    for name in cc_files:
        require(before[name] == after[name], f"Control Center resource changed: {name}")
    scripts = [f"DEBIAN/{name}" for name in sorted(MAINTAINER_NAMES) if f"DEBIAN/{name}" in before]
    require(set(scripts) == set(MAINTAINER_SHA256), "Unexpected base maintainer script set")
    for name in scripts:
        require(before[name] == after[name], f"Maintainer script changed: {name}")
        require(hashlib.sha256(regular_file(after, name)).hexdigest() == MAINTAINER_SHA256[name],
                f"Maintainer script differs from reviewed uicache-only script: {name}")
    native_data = regular_file(after, f"{APP}/PRIVControl")
    with tempfile.TemporaryDirectory(prefix="priv-release-verification-") as directory:
        path = Path(directory) / "PRIVControl"
        path.write_bytes(native_data)
        native = verify_native_binary(path)
    native["path"] = f"{APP}/PRIVControl"
    return {
        "verified": True, "version": version,
        "base": {"path": str(base.resolve()), "version": old_control["Version"],
                 "sha256": hashlib.sha256(base.read_bytes()).hexdigest()},
        "package": {"path": str(package.resolve()), "version": new_control["Version"],
                    "bytes": package.stat().st_size, "sha256": hashlib.sha256(package.read_bytes()).hexdigest()},
        **inventory, "info_plist": info, "theme": theme,
        "control_center_files_preserved": len(cc_files), "maintainer_scripts_preserved": scripts,
        "maintainer_scripts_match_reviewed_uicache_only_hashes": True,
        "all_other_existing_entries_preserved": True, "native": native,
        "limitations": "Static archive comparison only. No installer or iOS code executed; runtime security behavior and device UI require separate review.",
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", required=True, type=Path)
    parser.add_argument("--package", required=True, type=Path)
    parser.add_argument("--version", required=True)
    parser.add_argument("--allow-added", action="append", default=[], metavar="PACKAGE_PATH")
    args = parser.parse_args()
    try:
        report = verify_release(args.base, args.package, args.version, set(args.allow_added))
    except (OSError, ValueError, tarfile.TarError, subprocess.CalledProcessError) as exc:
        print(json.dumps({"verified": False, "error": str(exc)}, indent=2), file=sys.stderr)
        return 1
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
