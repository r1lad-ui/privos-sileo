#!/usr/bin/env python3
"""Regression test for the rootless Debian installer, using disposable roots.

Run on Linux as root (for example, sudo python3 scripts/test_key_package_install.py).
--package can also inspect and unpack the real CI-built iOS package. Only dpkg's
unpack phase runs: there is no preinst in this package, postinst is not called,
and neither a test fixture executable nor an iOS executable is ever executed.
"""
from __future__ import annotations

import argparse
import io
import os
from pathlib import Path, PurePosixPath
import plistlib
import shutil
import stat
import subprocess
import tarfile
import tempfile

import build_keylab as builder


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def archive_members(package: Path, option: str) -> dict[str, tuple[tarfile.TarInfo, bytes]]:
    result = subprocess.run(["dpkg-deb", option, str(package)], check=True, capture_output=True)
    members = {}
    with tarfile.open(fileobj=io.BytesIO(result.stdout), mode="r:") as archive:
        for item in archive.getmembers():
            path = PurePosixPath(item.name)
            require(not path.is_absolute() and ".." not in path.parts,
                    "Unsafe archive path: " + item.name)
            name = str(path)
            require(name not in members, "Duplicate archive member: " + name)
            require(item.isdir() or item.isfile(), "Unexpected archive member type: " + name)
            contents = archive.extractfile(item).read() if item.isfile() else b""
            members[name] = item, contents
    return members


def validate_package(package: Path) -> dict[str, tuple[tarfile.TarInfo, bytes]]:
    control = archive_members(package, "--ctrl-tarfile")
    require(set(control) <= {".", "control", "postinst", "prerm"},
            "Unexpected control members; this regression must not execute preinst scripts")
    require({"control", "postinst", "prerm"} <= set(control), "Missing control members")
    for name in ("postinst", "prerm"):
        item, contents = control[name]
        require(item.isfile() and item.mode & 0o7777 == 0o755,
                name + " must be a regular executable script with mode 0755")
        require(contents.startswith(b"#!/var/jb/bin/sh\n"),
                name + " must use the rootless shell interpreter")
        require(b"\r" not in contents and b"\x00" not in contents and contents.endswith(b"\n"),
                name + " must contain LF-terminated shell text")
    data = archive_members(package, "--fsys-tarfile")
    files = {name: value for name, value in data.items() if value[0].isfile()}
    directories = {name for name, value in data.items() if value[0].isdir()}
    expected_files = {builder.APP_PATH + "/" + builder.APP_NAME,
                      builder.APP_PATH + "/Info.plist",
                      builder.APP_PATH + "/_CodeSignature/CodeResources"}
    require(expected_files <= set(files), "App executable, plist, or nested signature resource is missing")
    required_directories = set()
    for name, (item, contents) in files.items():
        require(name.startswith(builder.APP_PATH + "/"), "File outside PRIV Key: " + name)
        require(item.uid == 0 and item.gid == 0, "Unexpected archive file ownership: " + name)
        expected_mode = 0o755 if name == builder.APP_PATH + "/" + builder.APP_NAME else 0o644
        require(item.mode & 0o7777 == expected_mode, "Unexpected archive file mode: " + name)
        required_directories.update(str(parent) for parent in PurePosixPath(name).parents
                                    if str(parent) != ".")
    require(required_directories <= directories,
            "Missing explicit tar directory entries: " + ", ".join(sorted(required_directories - directories)))
    require(directories <= required_directories | {"."}, "Unexpected directories outside app ancestry")
    for name in directories:
        item = data[name][0]
        require(item.uid == 0 and item.gid == 0 and item.mode & 0o7777 == 0o755,
                "Directory must be root-owned with mode 0755: " + name)
    return files


def unpack_case(package: Path, files: dict, work: Path, use_symlink: bool) -> None:
    label = "rootless-symlink" if use_symlink else "fresh-directories"
    root = work / label
    root.mkdir()
    (root / "var").mkdir()
    if use_symlink:
        bootstrap = root / "private/preboot/fixture/procursus"
        (bootstrap / "Applications").mkdir(parents=True)
        # Absolute, like a device bootstrap link, but wholly inside this temporary
        # root: Linux resolves symlinks without applying dpkg's --root prefix.
        (root / "var/jb").symlink_to(bootstrap, target_is_directory=True)
    else:
        (root / "var/jb/Applications").mkdir(parents=True)
    admin = root / "var/lib/dpkg"
    admin.mkdir(parents=True)
    require(not (root / builder.APP_PATH).exists(), "Fixture app directory must start absent")
    command = ["dpkg", "--root=" + str(root), "--admindir=" + str(admin),
               "--log=" + str(root / "dpkg.log"), "--force-architecture", "--force-depends",
               "--no-triggers", "--unpack", str(package)]
    result = subprocess.run(command, capture_output=True, text=True, timeout=60)
    require(result.returncode == 0,
            label + " unpack failed:\n" + result.stdout + result.stderr)
    for name, (item, contents) in files.items():
        installed = root / name
        require(installed.resolve().is_relative_to(root.resolve()), "Installed path escaped fixture root")
        require(installed.is_file() and installed.read_bytes() == contents,
                "Unpacked file mismatch: " + name)
        require(stat.S_IMODE(installed.stat().st_mode) == item.mode & 0o7777,
                "Unpacked permission mismatch: " + name)
    if use_symlink:
        require((root / "var/jb").is_symlink(), "dpkg replaced the existing bootstrap symlink")
    require("Status: install ok unpacked" in (admin / "status").read_text(),
            "The package did not stop successfully in unpacked state")
    print(label + ": dpkg unpack passed; app and nested signature resources match")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", type=Path, help="Inspect and unpack an existing built .deb")
    args = parser.parse_args()
    require(os.geteuid() == 0, "This isolated dpkg regression requires root; run it with sudo")
    require(bool(shutil.which("dpkg") and shutil.which("dpkg-deb")), "Linux dpkg and dpkg-deb are required")
    with tempfile.TemporaryDirectory(prefix="keylab-install-regression-") as temporary:
        work = Path(temporary).resolve()
        if args.package:
            package = args.package.resolve(strict=True)
        else:
            app = work / "fixture/PRIVKey.app"
            signature = app / "_CodeSignature"
            signature.mkdir(parents=True)
            (app / builder.APP_NAME).write_bytes(b"Inert packaging fixture. Never execute.\n")
            (app / "Info.plist").write_bytes(plistlib.dumps(builder.app_info("17.5")))
            (signature / "CodeResources").write_bytes(b"Nested packaging fixture.\n")
            package = builder.build_package(app, work)
        files = validate_package(package)
        unpack_case(package, files, work, use_symlink=False)
        unpack_case(package, files, work, use_symlink=True)
    print("Rootless maintainer scripts, archive directories, and real dpkg extraction verified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
