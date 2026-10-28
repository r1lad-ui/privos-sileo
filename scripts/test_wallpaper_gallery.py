#!/usr/bin/env python3
"""Exercise the production wallpaper module with a host-only UIKit boundary mock.

Run: python3 scripts/test_wallpaper_gallery.py
Needs a POSIX host C compiler (CC, or cc). Does not need an iOS SDK.
This verifies application guards, callbacks and ownership bookkeeping, not real
iOS TCC prompts, UIKit ABI, Photos persistence or visual rendering.
"""

import hashlib
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile


def main() -> None:
    root = Path(__file__).resolve().parent.parent
    native = root / "native" / "WallpaperGallery.inc"
    raw = native.read_bytes()
    code = re.sub(r"/\*.*?\*/|//[^\n]*", "", raw.decode(), flags=re.S)
    forbidden = re.search(
        r"\b(g_vault|vaultFile|decryptFile|saveVault|handlePicker|"
        r"imagePickerController|PHAsset|PHAssetCollection|fetchAssets|"
        r"fetchAssetCollections|DCIM)\b|Photos\.sqlite|vault\.pv4",
        code,
    )
    if forbidden:
        raise SystemExit(f"Unexpected private-data or Photos-read dependency: {forbidden.group()}")

    compiler = shlex.split(os.environ.get("CC", "cc"))
    with tempfile.TemporaryDirectory(prefix="priv-wallpaper-tests-") as scratch:
        binary = Path(scratch) / "test_wallpaper_gallery"
        subprocess.run(
            [*compiler, "-std=c11", "-O0", "-g", "-Wall", "-Wextra", "-Werror",
             str(root / "scripts" / "test_wallpaper_gallery.c"),
             "-ldl", "-o", str(binary)],
            check=True,
        )
        subprocess.run([str(binary)], check=True)
    print("Read-boundary check passed; production SHA256=" + hashlib.sha256(raw).hexdigest())


if __name__ == "__main__":
    main()
