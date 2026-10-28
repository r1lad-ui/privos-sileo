#!/usr/bin/env python3
"""Run native reference/document/UI checks on a disposable iPhone SE simulator.

This builds a separate simulator application with an additional test source.
Neither that source nor this simulator executable enters the published .deb.
The runner creates and deletes only its own simulator; it never uses an iPhone
or an existing developer simulator. Xcode and an installed iOS runtime are required.
"""
from __future__ import annotations

import json
from pathlib import Path
import shutil
import subprocess
import sys
import time
import uuid

import build_keylab as builder


OUTPUT = builder.ROOT / "build/keylab-simulator"
DEVICE_TYPE = "com.apple.CoreSimulator.SimDeviceType.iPhone-SE-3rd-generation"


def command(arguments: list[str], *, timeout: int = 60,
            check: bool = True) -> subprocess.CompletedProcess:
    try:
        result = subprocess.run(arguments, text=True, capture_output=True, timeout=timeout)
    except subprocess.TimeoutExpired as exc:
        # Python may return bytes on timeout even with text=True. Retain bounded
        # boot/install progress so a failed CI run identifies the stalled stage.
        captured = []
        for part in (exc.stdout, exc.stderr):
            captured.append(part.decode("utf-8", errors="replace") if isinstance(part, bytes) else (part or ""))
        tail = "\n".join(captured)[-3000:]
        raise RuntimeError(f"Command timed out after {timeout}s: " + " ".join(arguments) +
                           "\nCaptured output (last 3000 characters):\n" + (tail or "<no output>")) from exc
    if check and result.returncode:
        raise RuntimeError("Command failed: " + " ".join(arguments) + "\n" + result.stdout + result.stderr)
    return result


def simctl(*arguments: str, timeout: int = 60,
           check: bool = True) -> subprocess.CompletedProcess:
    return command(["xcrun", "simctl", *arguments], timeout=timeout, check=check)


def runtime_version(runtime: dict) -> tuple[int, ...]:
    try:
        return tuple(int(part) for part in runtime.get("version", "0").split("."))
    except ValueError:
        return (0,)


def create_device(preferred_sdk_version: str | None = None) -> tuple[str, dict]:
    inventory = json.loads(simctl("list", "--json").stdout)
    if not any(item.get("identifier") == DEVICE_TYPE for item in inventory.get("devicetypes", [])):
        raise RuntimeError("Xcode has no iPhone SE (3rd generation) device type; the 375 × 667 layout was not tested")
    runtimes = [item for item in inventory.get("runtimes", [])
                if item.get("isAvailable") is True and
                item.get("identifier", "").startswith("com.apple.CoreSimulator.SimRuntime.iOS-")]
    if not runtimes:
        raise RuntimeError("No available iOS simulator runtime is installed; native UI verification was not performed")
    preferred_version = runtime_version({"version": preferred_sdk_version or "0"})
    preferred_series = (preferred_version + (0, 0))[:2]
    def preference(runtime: dict) -> tuple[bool, tuple[int, ...]]:
        version = runtime_version(runtime)
        series = (version + (0, 0))[:2]
        return bool(preferred_sdk_version) and series == preferred_series, version
    errors = []
    # Prefer an installed runtime from the build SDK's major/minor series.
    # Otherwise retain the newest-first fallback; no runtime is downloaded.
    # All candidates use a fresh simulator, including fallback attempts.
    for runtime in sorted(runtimes, key=preference, reverse=True):
        name = "PRIV-Key-Verification-" + str(uuid.uuid4())[:8]
        result = simctl("create", name, DEVICE_TYPE, runtime["identifier"], check=False)
        if result.returncode:
            errors.append(runtime.get("name", runtime["identifier"]) + ": " + result.stderr.strip())
            continue
        identifier = result.stdout.strip()
        try:
            uuid.UUID(identifier)
        except ValueError as exc:
            raise RuntimeError("simctl returned an invalid created device identifier: " + identifier) from exc
        return identifier, runtime
    raise RuntimeError("No installed iOS runtime could create an iPhone SE simulator:\n" + "\n".join(errors))


def main() -> int:
    if sys.platform != "darwin" or not shutil.which("xcrun"):
        raise RuntimeError("Simulator verification requires macOS with Xcode")
    verification = OUTPUT / "verification"
    verification.mkdir(parents=True, exist_ok=True)
    # Only generated verification files from a previous invocation are cleared.
    for path in verification.iterdir():
        if path.is_file() and path.suffix in {".json", ".png", ".log"}:
            path.unlink()
    report_path = verification / "runner.json"
    report = {"passed": False, "version": builder.VERSION,
              "device_type": DEVICE_TYPE, "expected_logical_screen": [375, 667],
              "physical_device_test": "Not performed"}
    identifier = None
    container = None
    try:
        print("Compiling the separate simulator app and native smoke tests", flush=True)
        app, compilation = builder.build_native(OUTPUT, simulator=True,
                                                extra_sources=[builder.ROOT / "scripts/PKReferenceSmoke.m"])
        (verification / "compilation.json").write_text(json.dumps(compilation, indent=2) + "\n")
        report["preferred_build_sdk"] = compilation["build_sdk"]
        identifier, runtime = create_device(compilation["build_sdk"])
        report["runtime"] = runtime.get("name", runtime["identifier"])
        report["runtime_identifier"] = runtime["identifier"]
        print("Booting isolated iPhone SE simulator with " + report["runtime"], flush=True)
        simctl("boot", identifier, timeout=60)
        simctl("bootstatus", identifier, "-b", timeout=300)
        simctl("install", identifier, str(app), timeout=120)
        raw_container = simctl("get_app_container", identifier, builder.PACKAGE_ID, "data").stdout.strip()
        container = Path(raw_container).resolve(strict=True)
        if not container.is_dir() or not container.is_absolute():
            raise RuntimeError("Invalid simulator data container")
        print("Running native document and reference/guide UI checks", flush=True)
        simctl("launch", "--terminate-running-process",
               "--stdout=" + str(verification / "app-stdout.log"),
               "--stderr=" + str(verification / "app-stderr.log"),
               identifier, builder.PACKAGE_ID, "--keylab-reference-smoke", timeout=60)
        result_path = container / "Documents/keylab-smoke/report.json"
        deadline = time.monotonic() + 60
        native_report = None
        while time.monotonic() < deadline:
            if result_path.is_file():
                try:
                    native_report = json.loads(result_path.read_text())
                    break
                except (OSError, json.JSONDecodeError):
                    # The app may be finishing its atomic report write.
                    pass
            time.sleep(1)
        if native_report is None:
            raise RuntimeError("The simulator app did not write its native smoke report within 60 seconds")
        if native_report.get("passed") is not True:
            raise RuntimeError("Native smoke checks failed: " + json.dumps(native_report, ensure_ascii=False))
        report["passed"] = True
        report["native_report"] = "report.json"
        print("iPhone SE native smoke checks passed", flush=True)
        return 0
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as exc:
        report["failure"] = str(exc)
        raise
    finally:
        if container is not None:
            generated = container / "Documents/keylab-smoke"
            if generated.is_dir():
                for path in generated.iterdir():
                    if path.is_file() and path.suffix in {".json", ".png"}:
                        shutil.copyfile(path, verification / path.name)
        report_path.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n")
        if identifier is not None:
            print("Removing the isolated simulator created for this run", flush=True)
            for arguments in (("shutdown", identifier), ("delete", identifier)):
                try:
                    cleanup = simctl(*arguments, timeout=30, check=False)
                    if cleanup.returncode:
                        print("Simulator cleanup: " + cleanup.stderr.strip(), file=sys.stderr)
                except (OSError, subprocess.SubprocessError) as exc:
                    print("Simulator cleanup: " + str(exc), file=sys.stderr)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as exc:
        print("Simulator verification failed: " + str(exc), file=sys.stderr)
        raise SystemExit(1)
