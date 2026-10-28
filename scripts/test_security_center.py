#!/usr/bin/env python3
"""Exercise production security diagnostics and application lifecycle on a host.

Run: python3 scripts/test_security_center.py
Needs an x86_64 Linux host and a C compiler (CC, or cc), without an iOS SDK.
The harness includes the entire native app, including SecurityCenter.inc and
WallpaperGallery.inc. Objective-C messages are mocked at the runtime boundary;
the production logic and selectors are not copied or rewritten. A small x86_64
ABI bridge routes CGRect returns separately from scalar message returns.

These checks establish application logic and ownership bookkeeping, not real
Touch ID availability, Secure Enclave behavior, TCC prompts, UIKit rendering,
device snapshots or jailbreak integrity. ARM64 build/signature checks are
separate release gates.
"""

import hashlib
import os
from pathlib import Path
import platform
import re
import shlex
import subprocess
import tempfile


def without_comments(code: str) -> str:
    return re.sub(r"/\*.*?\*/|//[^\n]*", "", code, flags=re.S)


def main() -> None:
    root = Path(__file__).resolve().parent.parent
    module = root / "native" / "SecurityCenter.inc"
    raw = module.read_bytes()
    code = without_comments(raw.decode())
    forbidden = re.search(
        r"\b(evaluatePolicy|evaluateAccessControl|NSUserDefaults|NSFileManager|"
        r"SecItemAdd|SecItemUpdate|SecItemDelete|CFPreferencesSetValue|"
        r"CFPreferencesSetAppValue|MCProfileConnection|"
        r"g_vault|g_pass|g_unlocked|vaultFile|decryptFile|encryptFile|saveVault)\b|"
        r"writeToFile:|writeToURL:",
        code,
    )
    if forbidden:
        raise SystemExit("Unexpected authentication, persistence or vault dependency: " + forbidden.group())

    app = (root / "native" / "PRIVControl.c").read_text()
    baseline = (root / "native" / "baseline" / "PRIVControl-0.5.c").read_text()
    # Preserve the existing sensitive-data cleanup itself; the new lifecycle
    # wrappers are tested behaviorally by calling their actual implementations.
    resign = r"static void resign\([^\n]+"
    old = re.search(resign, baseline)
    new = re.search(resign, app)
    if not old or not new or old.group() != new.group():
        raise SystemExit("Original resign cleanup changed; review the vault lifecycle before releasing")

    for selector, handler in (
        ("applicationWillResignActive:", "securityResign"),
        ("applicationDidEnterBackground:", "securityEnteredBackground"),
        ("applicationDidBecomeActive:", "securityBecameActive"),
    ):
        expected = f'class_addMethod(del,S("{selector}"),(void*){handler},"v@:@")'
        if expected not in app:
            raise SystemExit("Missing tested lifecycle registration: " + selector)

    if platform.machine().lower() not in {"x86_64", "amd64"}:
        raise SystemExit("The host mock ABI bridge currently requires x86_64")
    compiler = shlex.split(os.environ.get("CC", "cc"))
    with tempfile.TemporaryDirectory(prefix="priv-security-tests-") as scratch:
        binary = Path(scratch) / "test_security_center"
        subprocess.run(
            [*compiler, "-std=c11", "-O0", "-g", "-Wall", "-Wextra", "-Werror",
             str(root / "scripts" / "test_security_center.c"), "-o", str(binary)],
            check=True,
        )
        subprocess.run([str(binary)], check=True)
    print("Security boundary and lifecycle registration checks passed")
    print("SecurityCenter.inc SHA256=" + hashlib.sha256(raw).hexdigest())
    print("PRIVControl.c SHA256=" + hashlib.sha256(app.encode()).hexdigest())


if __name__ == "__main__":
    main()
