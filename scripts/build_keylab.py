#!/usr/bin/env python3
"""Build the native PRIV Key app and its rootless Sileo package.

Requires Xcode only for the native compile/signing step. The geometry tests,
Debian package writer and APT index update use the host C compiler / Python
standard library. No device connection or Apple signing identity is needed.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
from email.utils import format_datetime
import gzip
import hashlib
import io
import json
import os
from pathlib import Path
import platform
import plistlib
import shutil
import struct
import subprocess
import sys
import tarfile
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "native/PRIVKey"
VERSION = "0.2.1"
PACKAGE_ID = "fr.privos.keylab"
APP_NAME = "PRIVKey"
APP_PATH = "var/jb/Applications/PRIVKey.app"
DEB_NAME = f"{PACKAGE_ID}_{VERSION}_iphoneos-arm64.deb"
DEFAULT_OUTPUT = ROOT / "build/keylab"
DEFAULT_EPOCH = 1793203200  # 2026-10-28 16:00 UTC
SOURCE_PATHS = ["native/PRIVKey", "scripts/build_keylab.py",
                "scripts/test_key_geometry.c", "scripts/test_key_package_install.py",
                "scripts/smoke_keylab_simulator.py", "scripts/PKReferenceSmoke.m",
                ".github/workflows/build-keylab.yml"]


def run(args: list[str], **kwargs) -> subprocess.CompletedProcess:
    return subprocess.run(args, check=True, **kwargs)


def output(args: list[str], **kwargs) -> str:
    return run(args, capture_output=True, text=True, **kwargs).stdout.strip()


def digest(data: bytes, algorithm: str = "sha256") -> str:
    return hashlib.new(algorithm, data).hexdigest()


def epoch() -> int:
    value = int(os.environ.get("SOURCE_DATE_EPOCH", DEFAULT_EPOCH))
    if value < 0:
        raise ValueError("SOURCE_DATE_EPOCH must be nonnegative")
    return value


def test_geometry() -> None:
    test = ROOT / "scripts/test_key_geometry.c"
    core = SOURCE / "PKGeometry.c"
    if not test.is_file() or not core.is_file():
        raise RuntimeError("The geometry implementation and its tests are required")
    with tempfile.TemporaryDirectory(prefix="privos-key-test-") as folder:
        executable = Path(folder) / "test-key-geometry"
        run([os.environ.get("CC", "cc"), "-std=c99", "-Wall", "-Wextra", "-O2",
             "-I", str(SOURCE), str(test), str(core), "-lm", "-o", str(executable)])
        run([str(executable)])


def write_png(path: Path, size: int) -> None:
    """Generate an original flat app icon without an image dependency."""
    rows = bytearray()
    for y in range(size):
        rows.append(0)
        for x in range(size):
            u, v = x / size, y / size
            ring = 0.10 ** 2 <= (u - .34) ** 2 + (v - .38) ** 2 <= .165 ** 2
            shaft = .405 <= u <= .77 and abs(v - (u - .01)) < .047
            tooth1 = .625 <= u <= .68 and .55 <= v <= .73
            tooth2 = .735 <= u <= .79 and .65 <= v <= .80
            pixel = (88, 239, 191) if ring or shaft or tooth1 or tooth2 else (15, 23, 30)
            rows.extend(pixel)

    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(rows), 9)) + chunk(b"IEND", b"")
    path.write_bytes(png)


def app_info(sdk_version: str, *, simulator: bool = False) -> dict:
    sdk_name = "iphonesimulator" if simulator else "iphoneos"
    return {
        "CFBundleDevelopmentRegion": "fr",
        "CFBundleDisplayName": "PRIV Key",
        "CFBundleExecutable": APP_NAME,
        "CFBundleIdentifier": PACKAGE_ID,
        "CFBundleInfoDictionaryVersion": "6.0",
        "CFBundleName": APP_NAME,
        "CFBundlePackageType": "APPL",
        "CFBundleShortVersionString": VERSION,
        "CFBundleVersion": "4",
        "MinimumOSVersion": "15.0",
        "DTPlatformName": sdk_name,
        "DTSDKName": sdk_name + sdk_version,
        "CFBundleSupportedPlatforms": ["iPhoneSimulator" if simulator else "iPhoneOS"],
        "LSRequiresIPhoneOS": True,
        "UIDeviceFamily": [1, 2],
        "UILaunchScreen": {},
        "UIUserInterfaceStyle": "Dark",
        "UIApplicationSupportsIndirectInputEvents": True,
        "UISupportedInterfaceOrientations": ["UIInterfaceOrientationPortrait",
                                             "UIInterfaceOrientationLandscapeLeft",
                                             "UIInterfaceOrientationLandscapeRight"],
        "UISupportedInterfaceOrientations~ipad": ["UIInterfaceOrientationPortrait",
                                                   "UIInterfaceOrientationPortraitUpsideDown",
                                                   "UIInterfaceOrientationLandscapeLeft",
                                                   "UIInterfaceOrientationLandscapeRight"],
        "NSCameraUsageDescription": "Photographier une clé de test pour relever son contour localement.",
        "NSPhotoLibraryUsageDescription": "Choisir une photo de clé de test pour son relevé, sans envoi réseau.",
        "CFBundleIcons": {"CFBundlePrimaryIcon": {"CFBundleIconFiles": ["AppIcon60x60"]}},
        "CFBundleIcons~ipad": {"CFBundlePrimaryIcon": {"CFBundleIconFiles": ["AppIcon60x60", "AppIcon76x76"]}},
    }


def build_native(destination: Path, *, simulator: bool = False,
                 extra_sources: list[Path] | None = None) -> tuple[Path, dict]:
    if sys.platform != "darwin":
        raise RuntimeError("The iOS app must be compiled on macOS with Xcode. Use the GitHub Actions workflow.")
    if extra_sources and not simulator:
        raise ValueError("Additional test sources are limited to the simulator build")
    sdk_name = "iphonesimulator" if simulator else "iphoneos"
    target_architecture = platform.machine() if simulator else "arm64"
    if target_architecture not in {"arm64", "x86_64"}:
        raise RuntimeError("Unsupported simulator host architecture: " + target_architecture)
    target = target_architecture + "-apple-ios15.0" + ("-simulator" if simulator else "")
    sdk = output(["xcrun", "--sdk", sdk_name, "--show-sdk-path"])
    sdk_version = output(["xcrun", "--sdk", sdk_name, "--show-sdk-version"])
    clang = output(["xcrun", "--sdk", sdk_name, "--find", "clang"])
    app = destination / (APP_NAME + ".app")
    app.mkdir(parents=True, exist_ok=True)
    object_dir = destination / "objects"
    object_dir.mkdir(exist_ok=True)
    sources = sorted([*SOURCE.glob("*.m"), *SOURCE.glob("*.c")])
    sources.extend(Path(path).resolve() for path in (extra_sources or []))
    if not sources or not any(path.suffix == ".m" for path in sources):
        raise RuntimeError("No native Objective-C application sources were found")
    if any(not path.is_file() or path.suffix not in {".m", ".c"} for path in sources):
        raise RuntimeError("A native source file is missing or has an unsupported type")
    if len({path.name for path in sources}) != len(sources):
        raise RuntimeError("Native source filenames must be unique")
    objects = []
    flags = ["-target", target, "-isysroot", sdk,
             "-Os", "-Wall", "-Wextra", "-Werror=implicit-function-declaration",
             "-fvisibility=hidden", "-fstack-protector-strong", "-I", str(SOURCE)]
    for source in sources:
        obj = object_dir / (source.name + ".o")
        extra = ["-fobjc-arc", "-fblocks", "-fobjc-exceptions"] if source.suffix == ".m" else ["-std=c99"]
        run([clang, *flags, *extra, "-c", str(source), "-o", str(obj)])
        objects.append(obj)
    executable = app / APP_NAME
    link = [clang, "-target", target, "-isysroot", sdk,
            "-Wl,-pie", *map(str, objects), "-lobjc"]
    for framework in ("UIKit", "Foundation", "CoreGraphics", "QuartzCore", "PhotosUI", "UniformTypeIdentifiers", "SceneKit"):
        link.extend(["-framework", framework])
    run([*link, "-o", str(executable)])
    executable.chmod(0o755)
    (app / "Info.plist").write_bytes(plistlib.dumps(app_info(sdk_version, simulator=simulator), sort_keys=True))
    for name, size in (("AppIcon60x60@2x.png", 120), ("AppIcon60x60@3x.png", 180),
                       ("AppIcon76x76.png", 76), ("AppIcon76x76@2x.png", 152)):
        write_png(app / name, size)
    run(["codesign", "--force", "--sign", "-", "--timestamp=none", str(app)])
    run(["codesign", "--verify", "--deep", "--strict", "--verbose=2", str(app)])
    architecture = output(["xcrun", "lipo", "-archs", str(executable)])
    if architecture != target_architecture:
        raise RuntimeError("Unexpected native architecture: " + architecture)
    native_data = executable.read_bytes()
    if native_data[:4] != b"\xcf\xfa\xed\xfe":
        raise RuntimeError("The native executable is not a 64-bit Mach-O")
    report = {"native_compilation": "passed", "signature_verification": "passed",
              "signing": "ad hoc, no privileged entitlements", "architecture": architecture,
              "minimum_ios": "15.0", "build_sdk": sdk_version, "platform": sdk_name,
              "executable_sha256": digest(native_data),
              "sources": {str(p.relative_to(ROOT)): digest(p.read_bytes()) for p in sources},
              "device_validation": "Not performed. Installation, UIKit, camera, and physical dimensions require testing on the iPhone."}
    return app, report


def tar_gz(files: dict[str, tuple[bytes, int]]) -> bytes:
    # dpkg creates directories from explicit archive entries. A successful
    # `dpkg-deb --extract` is not sufficient to prove a package can be installed:
    # its tar extractor also creates missing parents, whereas dpkg --unpack
    # requires them in the data archive on a fresh installation.
    directories = {"."}
    for name in files:
        if not name or name.startswith("/") or ".." in Path(name).parts:
            raise ValueError("Unsafe package member: " + name)
        directories.update(parent.as_posix() for parent in Path(name).parents)
    buffer = io.BytesIO()
    with tarfile.open(fileobj=buffer, mode="w", format=tarfile.USTAR_FORMAT) as archive:
        for name in sorted(directories, key=lambda value: (len(Path(value).parts), value)):
            item = tarfile.TarInfo("./" if name == "." else "./" + name + "/")
            item.type = tarfile.DIRTYPE
            item.uid = item.gid = 0
            item.uname = item.gname = "root"
            item.mode = 0o755
            item.mtime = epoch()
            archive.addfile(item)
        for name in sorted(files):
            contents, mode = files[name]
            item = tarfile.TarInfo("./" + name)
            item.uid = item.gid = 0
            item.uname = item.gname = "root"
            item.mode = mode
            item.mtime = epoch()
            item.size = len(contents)
            archive.addfile(item, io.BytesIO(contents))
    return gzip.compress(buffer.getvalue(), compresslevel=9, mtime=epoch())


def ar_archive(members: list[tuple[str, bytes]]) -> bytes:
    archive = bytearray(b"!<arch>\n")
    for name, contents in members:
        if len(name) > 15:
            raise ValueError("AR member name is too long")
        header = (f"{name + '/':<16}{epoch():<12}{0:<6}{0:<6}{'100644':<8}{len(contents):<10}`\n").encode("ascii")
        if len(header) != 60:
            raise ValueError("Invalid AR member header")
        archive.extend(header)
        archive.extend(contents)
        if len(contents) % 2:
            archive.extend(b"\n")
    return bytes(archive)


def control_text(installed_kib: int) -> str:
    return (f"Package: {PACKAGE_ID}\nName: PRIV Key LAB\nVersion: {VERSION}\n"
            "Architecture: iphoneos-arm64\nSection: Applications\nPriority: optional\n"
            "Maintainer: PRIV OS Lab\nDepends: firmware (>= 15.0)\n"
            f"Installed-Size: {installed_kib}\n"
            "Description: Atelier photo natif : references carte et pieces, traits de mesure orientables, loupe et contour manuel. Exports locaux SVG, JSON et STL.\n")


POSTINST = """#!/var/jb/bin/sh
set -eu
case "${1:-}" in
  configure)
    if [ -x /var/jb/usr/bin/uicache ]; then
      /var/jb/usr/bin/uicache -p /var/jb/Applications/PRIVKey.app || true
    fi
    ;;
esac
exit 0
"""
PRERM = """#!/var/jb/bin/sh
set -eu
case "${1:-}" in
  remove|deconfigure)
    if [ -x /var/jb/usr/bin/uicache ]; then
      /var/jb/usr/bin/uicache -u /var/jb/Applications/PRIVKey.app || true
    fi
    ;;
esac
exit 0
"""


def build_package(app: Path, destination: Path) -> Path:
    data_files = {}
    installed_size = 0
    for file in sorted(app.rglob("*")):
        if file.is_symlink():
            raise ValueError("Unexpected symlink inside app bundle")
        if file.is_file():
            contents = file.read_bytes()
            installed_size += len(contents)
            mode = 0o755 if file.name == APP_NAME else 0o644
            data_files[APP_PATH + "/" + file.relative_to(app).as_posix()] = contents, mode
    if APP_PATH + "/" + APP_NAME not in data_files or APP_PATH + "/Info.plist" not in data_files:
        raise ValueError("Incomplete app bundle")
    control_files = {"control": (control_text((installed_size + 1023) // 1024).encode(), 0o644),
                     "postinst": (POSTINST.encode(), 0o755), "prerm": (PRERM.encode(), 0o755)}
    package = destination / DEB_NAME
    package.write_bytes(ar_archive([("debian-binary", b"2.0\n"),
                                    ("control.tar.gz", tar_gz(control_files)),
                                    ("data.tar.gz", tar_gz(data_files))]))
    return package


def read_deb(package: Path) -> tuple[str, list[str]]:
    data = package.read_bytes()
    if not data.startswith(b"!<arch>\n"):
        raise ValueError("Not an AR package")
    cursor, members = 8, {}
    while cursor < len(data):
        header = data[cursor:cursor + 60]
        if len(header) != 60 or header[-2:] != b"`\n":
            raise ValueError("Invalid AR header")
        name = header[:16].decode("ascii").strip().rstrip("/")
        length = int(header[48:58])
        cursor += 60
        if length < 0 or cursor + length > len(data) or name in members:
            raise ValueError("Invalid AR member")
        members[name] = data[cursor:cursor + length]
        cursor += length + length % 2
    if set(members) != {"debian-binary", "control.tar.gz", "data.tar.gz"} or members["debian-binary"] != b"2.0\n":
        raise ValueError("Unexpected Debian package structure")
    with tarfile.open(fileobj=io.BytesIO(members["control.tar.gz"]), mode="r:gz") as archive:
        control_file = archive.extractfile("./control")
        if control_file is None:
            raise ValueError("Missing Debian control")
        control = control_file.read().decode("utf-8")
    with tarfile.open(fileobj=io.BytesIO(members["data.tar.gz"]), mode="r:gz") as archive:
        paths = []
        directories = set()
        shared_parents = {parent.as_posix() for parent in Path(APP_PATH).parents}
        for item in archive.getmembers():
            name = item.name.removeprefix("./").rstrip("/") or "."
            if name.startswith("/") or ".." in Path(name).parts:
                raise ValueError("Unsafe package data member: " + name)
            if item.isdir():
                if name not in shared_parents and name != APP_PATH and not name.startswith(APP_PATH + "/"):
                    raise ValueError("Unexpected package directory: " + name)
                if item.mode != 0o755:
                    raise ValueError("Unexpected package directory permissions: " + name)
                directories.add(name)
            elif item.isfile():
                required = {parent.as_posix() for parent in Path(name).parents}
                if not required.issubset(directories):
                    raise ValueError("Missing directory entries before file: " + name)
                paths.append(name)
            else:
                raise ValueError("Unexpected package data member type: " + name)
    return control, paths


def fields(block: str) -> dict[str, str]:
    result = {}
    for line in block.splitlines():
        if line and not line[0].isspace() and ":" in line:
            key, value = line.split(":", 1)
            result[key] = value.lstrip()
    return result


def update_indexes(package: Path, root: Path = ROOT) -> dict:
    control, paths = read_deb(package)
    info = fields(control)
    if info.get("Package") != PACKAGE_ID or info.get("Version") != VERSION or info.get("Architecture") != "iphoneos-arm64":
        raise ValueError("The supplied package does not match PRIV Key's release identity")
    if not paths or any(not path.startswith(APP_PATH + "/") for path in paths):
        raise ValueError("Package contains files outside its app bundle")
    deb_directory = root / "debs"
    deb_directory.mkdir(exist_ok=True)
    target = deb_directory / DEB_NAME
    if package.resolve() != target.resolve():
        shutil.copyfile(package, target)
    package_bytes = target.read_bytes()
    entry = control.rstrip() + "\n"
    entry += f"Filename: debs/{DEB_NAME}\nSize: {len(package_bytes)}\n"
    for algorithm, field in (("md5", "MD5sum"), ("sha1", "SHA1"), ("sha256", "SHA256")):
        entry += f"{field}: {digest(package_bytes, algorithm)}\n"
    packages_path = root / "Packages"
    previous = packages_path.read_text() if packages_path.exists() else ""
    retained = [block.strip() for block in previous.split("\n\n")
                if block.strip() and fields(block).get("Package") != PACKAGE_ID]
    packages_bytes = ("\n\n".join([*retained, entry.rstrip()]) + "\n\n").encode()
    packages_path.write_bytes(packages_bytes)
    (root / "Packages.gz").write_bytes(gzip.compress(packages_bytes, compresslevel=9, mtime=0))
    release_path = root / "Release"
    old_release = release_path.read_text() if release_path.exists() else ""
    metadata = fields(old_release)
    for checksum in ("MD5Sum", "SHA1", "SHA256", "SHA512"):
        metadata.pop(checksum, None)
    metadata.setdefault("Origin", "PRIV OS Lab")
    metadata.setdefault("Label", "PRIV OS Lab")
    metadata.setdefault("Suite", "stable")
    metadata.setdefault("Codename", "privos")
    metadata.setdefault("Architectures", "iphoneos-arm64")
    metadata.setdefault("Components", "main")
    metadata["Date"] = format_datetime(datetime.now(timezone.utc))
    release = "".join(f"{key}: {value}\n" for key, value in metadata.items())
    for algorithm, header in (("md5", "MD5Sum"), ("sha256", "SHA256")):
        release += header + ":\n"
        for name in ("Packages", "Packages.gz"):
            body = (root / name).read_bytes()
            release += f" {digest(body, algorithm)} {len(body)} {name}\n"
    release_path.write_text(release)
    if gzip.decompress((root / "Packages.gz").read_bytes()) != packages_bytes:
        raise RuntimeError("Compressed package index mismatch")
    return {"package": target.relative_to(root).as_posix(), "sha256": digest(package_bytes),
            "bytes": len(package_bytes), "other_package_entries_retained": len(retained)}


def publish_git(package: Path, built_commit: str) -> None:
    """Publish in an isolated CI checkout; reject changed sources and force pushes."""
    if os.environ.get("GITHUB_ACTIONS") != "true" or os.environ.get("GITHUB_REF") != "refs/heads/main":
        raise RuntimeError("Automatic publication is limited to main in GitHub Actions")
    if len(built_commit) != 40 or any(c not in "0123456789abcdef" for c in built_commit):
        raise ValueError("A full build commit SHA is required")
    package = package.resolve()
    with tempfile.TemporaryDirectory(prefix="keylab-publish-") as temporary:
        preserved = Path(temporary) / DEB_NAME
        shutil.copyfile(package, preserved)
        for attempt in range(3):
            run(["git", "fetch", "origin", "main"], cwd=ROOT)
            check = subprocess.run(["git", "diff", "--quiet", built_commit, "origin/main", "--", *SOURCE_PATHS], cwd=ROOT)
            if check.returncode != 0:
                raise RuntimeError("App/build sources changed after this build; a new build must publish them")
            # This is a disposable Actions checkout. Unrelated current changes are
            # retained from origin/main; generated indexes are regenerated below.
            run(["git", "checkout", "-B", "keylab-publish", "origin/main"], cwd=ROOT)
            report = update_indexes(preserved)
            run(["git", "add", "--", report["package"], "Packages", "Packages.gz", "Release"], cwd=ROOT)
            unchanged = subprocess.run(["git", "diff", "--cached", "--quiet"], cwd=ROOT)
            if unchanged.returncode == 0:
                print("The verified package is already published")
                return
            run(["git", "-c", "user.name=github-actions[bot]",
                 "-c", "user.email=41898282+github-actions[bot]@users.noreply.github.com",
                 "commit", "-m", f"Publish PRIV Key {VERSION}"], cwd=ROOT)
            pushed = subprocess.run(["git", "push", "origin", "HEAD:main"], cwd=ROOT)
            if pushed.returncode == 0:
                print(json.dumps({"publication": "pushed", **report}, indent=2))
                return
            if attempt == 2:
                raise RuntimeError("Publication push failed; the built package remains in the workflow artifact")


def self_test() -> None:
    with tempfile.TemporaryDirectory(prefix="keylab-package-test-") as temporary:
        root = Path(temporary)
        app = root / "PRIVKey.app"
        app.mkdir()
        (app / APP_NAME).write_bytes(b"package-structure-fixture-only\n")
        (app / "Info.plist").write_bytes(plistlib.dumps(app_info("17.5")))
        package = build_package(app, root)
        control, paths = read_deb(package)
        assert fields(control)["Package"] == PACKAGE_ID
        assert paths == sorted([APP_PATH + "/" + APP_NAME, APP_PATH + "/Info.plist"])
        old_entry = "Package: keep.me\nVersion: 7.8.9\nDescription: Existing package\n continuation\n"
        (root / "Packages").write_text(old_entry)
        (root / "Release").write_text("Origin: Existing owner\nLabel: Existing label\nSuite: stable\nMD5Sum:\n 000 1 Packages\n")
        first = update_indexes(package, root)
        assert first["other_package_entries_retained"] == 1
        assert old_entry.rstrip() in (root / "Packages").read_text()
        assert "Origin: Existing owner\n" in (root / "Release").read_text()
        update_indexes(package, root)
        assert (root / "Packages").read_text().count("Package: " + PACKAGE_ID + "\n") == 1
        assert gzip.decompress((root / "Packages.gz").read_bytes()) == (root / "Packages").read_bytes()
        image = root / "icon.png"
        write_png(image, 120)
        assert image.read_bytes().startswith(b"\x89PNG\r\n\x1a\n")
        if shutil.which("dpkg-deb"):
            run(["dpkg-deb", "--info", str(package)], stdout=subprocess.DEVNULL)
    print("Package structure, retained indexes, repeated publication, and icon checks passed")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--test-only", action="store_true")
    group.add_argument("--self-test", action="store_true")
    group.add_argument("--index-only", action="store_true")
    group.add_argument("--publish-git", action="store_true")
    parser.add_argument("--package", type=Path)
    parser.add_argument("--built-commit")
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    if args.self_test:
        self_test()
    elif args.test_only:
        test_geometry()
        self_test()
    elif args.index_only or args.publish_git:
        if not args.package:
            parser.error("--package is required for index/publication operations")
        if args.publish_git:
            if not args.built_commit:
                parser.error("--built-commit is required for publication")
            publish_git(args.package, args.built_commit)
        else:
            print(json.dumps(update_indexes(args.package), indent=2))
    else:
        test_geometry()
        self_test()
        destination = args.output_dir.resolve()
        destination.mkdir(parents=True, exist_ok=True)
        app, report = build_native(destination)
        package = build_package(app, destination)
        _, package_paths = read_deb(package)
        report.update({"version": VERSION, "package": package.name,
                       "package_bytes": package.stat().st_size,
                       "package_sha256": digest(package.read_bytes()),
                       "package_members": package_paths, "geometry_tests": "passed",
                       "package_structure_tests": "passed"})
        (destination / "validation.json").write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as exc:
        print("Build failed: " + str(exc), file=sys.stderr)
        raise SystemExit(1)
