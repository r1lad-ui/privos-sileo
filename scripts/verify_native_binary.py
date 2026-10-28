#!/usr/bin/env python3
"""Verify a PRIV Control Mach-O without executing it.

Checks the release architecture/platform, linked libraries, chained-fixup
metadata and every SHA-256 page of its ad-hoc code signature. This is static
validation, not an iPhone execution test or a security audit of the app.

Usage: python3 scripts/verify_native_binary.py path/to/PRIVControl
Import verify_native_binary(path) to obtain the same JSON-serializable report.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys


EXPECTED_LIBRARIES = {
    "/usr/lib/libSystem.B.dylib",
    "/usr/lib/libobjc.A.dylib",
    "/System/Library/Frameworks/UIKit.framework/UIKit",
    "/System/Library/Frameworks/Security.framework/Security",
}
MINIMUM_IOS = 15 << 16
SDK_VERSION = (17 << 16) | (5 << 8)


class VerificationError(ValueError):
    """The file does not meet the native PRIV Control release contract."""


def require(condition: bool, message: str) -> None:
    if not condition:
        raise VerificationError(message)


def region(data: bytes, offset: int, length: int, label: str) -> bytes:
    require(offset >= 0 and length >= 0 and offset + length <= len(data),
            f"Out-of-bounds {label}")
    return data[offset:offset + length]


def unpack(data: bytes, offset: int, fmt: str, label: str) -> tuple:
    return struct.unpack(fmt, region(data, offset, struct.calcsize(fmt), label))


def cstring(data: bytes, offset: int, end: int, label: str) -> str:
    raw = region(data, offset, end - offset, label)
    require(b"\0" in raw, f"Unterminated {label}")
    try:
        return raw.split(b"\0", 1)[0].decode("utf-8")
    except UnicodeDecodeError as exc:
        raise VerificationError(f"Invalid UTF-8 in {label}") from exc


def version_string(version: int) -> str:
    return f"{version >> 16}.{(version >> 8) & 255}.{version & 255}"


def verify_signature(data: bytes, offset: int, size: int) -> dict:
    blob = region(data, offset, size, "code signature")
    magic, length, count = unpack(blob, 0, ">III", "signature header")
    require(magic == 0xFADE0CC0, "Missing embedded-signature superblob")
    require(12 + count * 8 <= length <= len(blob), "Invalid signature length")
    blob = blob[:length]
    slots: dict[int, bytes] = {}
    for index in range(count):
        kind, relative = unpack(blob, 12 + index * 8, ">II", "signature slot")
        require(kind not in slots, f"Duplicate signature slot {kind}")
        _, slot_length = unpack(blob, relative, ">II", "signature subblob")
        require(relative >= 12 + count * 8 and slot_length >= 8,
                "Invalid signature subblob location")
        slots[kind] = region(blob, relative, slot_length, "signature subblob")
    # This app needs no privileged entitlements or certificate signature.
    require(set(slots) == {0}, "Unexpected signature/entitlement slots")
    directory = slots[0]
    fields = unpack(directory, 0, ">9I4BI", "CodeDirectory")
    (magic, length, version, flags, hashes, identifier, special_slots,
     code_slots, code_limit, hash_size, hash_type, platform, page_shift,
     _) = fields
    require(magic == 0xFADE0C02, "Invalid CodeDirectory magic")
    require(length == len(directory), "Invalid CodeDirectory length")
    require(flags & 0x2 != 0, "Code signature is not ad hoc")
    require(hash_type == 2 and hash_size == 32, "Expected SHA-256 signature")
    require(special_slots == 0, "Unexpected CodeDirectory special slots")
    require(0 < page_shift <= 20, "Invalid signature page size")
    require(code_limit == offset, "Signature does not cover all preceding bytes")
    page_size = 1 << page_shift
    require(code_slots == (code_limit + page_size - 1) // page_size,
            "Signature page count does not cover the executable")
    region(directory, hashes, code_slots * hash_size, "signature page hashes")
    require(hashes >= 44, "Signature hash table overlaps its header")
    for index in range(code_slots):
        page = data[index * page_size:min((index + 1) * page_size, code_limit)]
        actual = hashlib.sha256(page).digest()
        expected = directory[hashes + index * 32:hashes + (index + 1) * 32]
        require(actual == expected, f"Code signature mismatch on page {index}")
    name = cstring(directory, identifier, hashes, "signature identifier")
    require(name == "PRIVControl", f"Unexpected signature identifier: {name}")
    return {
        "ad_hoc": True, "flags": hex(flags), "identifier": name,
        "hash_type": "sha256", "page_size": page_size,
        "verified_pages": code_slots, "entitlements_present": False,
        "code_directory_version": hex(version), "platform_byte": platform,
    }


def verify_fixups(data: bytes, offset: int, size: int) -> dict:
    blob = region(data, offset, size, "chained fixups")
    (version, starts, imports, symbols, count, import_format,
     symbols_format) = unpack(blob, 0, "<7I", "chained-fixups header")
    require(version == 0, "Unsupported chained-fixups version")
    require(import_format in (1, 2, 3), "Invalid chained import format")
    require(symbols_format == 0, "Expected uncompressed chained symbol names")
    require(28 <= starts < len(blob), "Invalid chained-fixups starts offset")
    require(28 <= imports <= len(blob), "Invalid chained-fixups imports offset")
    require(28 <= symbols < len(blob), "Invalid chained-fixups symbols offset")
    region(blob, imports, count * {1: 4, 2: 8, 3: 16}[import_format],
           "chained-fixups imports")
    (segments,) = unpack(blob, starts, "<I", "chained-fixups segment count")
    region(blob, starts + 4, segments * 4, "chained-fixups segment offsets")
    return {"present": True, "version": version, "imports_count": count,
            "imports_format": import_format, "segments": segments}


def verify_native_binary(path: Path | str) -> dict:
    path = Path(path)
    data = path.read_bytes()
    (magic, cpu, subtype, file_type, commands, command_bytes, flags,
     _) = unpack(data, 0, "<8I", "Mach-O header")
    require(magic == 0xFEEDFACF, "Expected a little-endian 64-bit Mach-O")
    require(cpu == 0x0100000C and subtype == 0, "Expected plain arm64")
    require(file_type == 2, "Expected an executable Mach-O")
    require(flags & 0x200000 != 0, "Expected a PIE executable")
    require(commands > 0, "Missing load commands")
    region(data, 32, command_bytes, "load commands")
    end = 32 + command_bytes
    offset = 32
    libraries: list[str] = []
    build = signature = fixups = symtab = None
    command_types: list[str] = []
    for _ in range(commands):
        command, size = unpack(data, offset, "<II", "load-command header")
        require(size >= 8 and size % 8 == 0 and offset + size <= end,
                "Invalid Mach-O load-command size")
        payload = region(data, offset, size, "load command")
        command_types.append(hex(command))
        if command in (0xC, 0x80000018, 0x8000001F, 0x80000023):
            (name_offset,) = unpack(payload, 8, "<I", "dylib name offset")
            require(name_offset >= 24, "Invalid dylib name offset")
            libraries.append(cstring(payload, name_offset, size, "dylib name"))
        elif command == 0x32:
            require(build is None, "Duplicate LC_BUILD_VERSION")
            platform, minimum, sdk, tool_count = unpack(payload, 8, "<4I", "build version")
            region(payload, 24, tool_count * 8, "build tool versions")
            require(platform == 2, "Expected iOS platform")
            require(minimum == MINIMUM_IOS, "Expected minimum iOS 15.0")
            require(sdk == SDK_VERSION, "Expected SDK version 17.5")
            build = {"platform": "ios", "minimum": version_string(minimum),
                     "sdk": version_string(sdk)}
        elif command == 0x80000034:
            require(fixups is None, "Duplicate chained-fixups command")
            location, length = unpack(payload, 8, "<II", "chained-fixups command")
            fixups = verify_fixups(data, location, length)
        elif command == 0x1D:
            require(signature is None, "Duplicate code-signature command")
            location, length = unpack(payload, 8, "<II", "code-signature command")
            signature = verify_signature(data, location, length)
        elif command == 0x2:
            require(symtab is None, "Duplicate symbol-table command")
            symtab = unpack(payload, 8, "<4I", "symbol-table command")
        offset += size
    require(offset == end, "Load-command count/size mismatch")
    require(build is not None, "Missing LC_BUILD_VERSION")
    require(fixups is not None, "Missing chained fixups")
    require(signature is not None, "Missing ad-hoc signature")
    require(len(libraries) == len(EXPECTED_LIBRARIES)
            and set(libraries) == EXPECTED_LIBRARIES,
            f"Unexpected linked libraries: {libraries}")
    require(symtab is not None, "Missing symbol table")
    symbol_offset, symbol_count, strings_offset, strings_size = symtab
    region(data, symbol_offset, symbol_count * 16, "symbol table")
    strings = region(data, strings_offset, strings_size, "symbol strings")
    imports: list[str] = []
    for index in range(symbol_count):
        string_index, kind, _, _, _ = unpack(data, symbol_offset + index * 16,
                                            "<IBBHQ", "symbol entry")
        if kind == 1:  # N_UNDF | N_EXT
            imports.append(cstring(strings, string_index, len(strings), "import name"))
    return {
        "verified": True, "path": str(path.resolve()), "bytes": len(data),
        "sha256": hashlib.sha256(data).hexdigest(), "architecture": "arm64",
        "pie": True, "build": build, "linked_libraries": libraries,
        "imports": sorted(imports), "chained_fixups": fixups,
        "signature": signature, "load_command_types": command_types,
        "device_test": "Not performed; static binary verification only.",
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    args = parser.parse_args()
    try:
        report = verify_native_binary(args.binary)
    except (OSError, VerificationError) as exc:
        print(json.dumps({"verified": False, "error": str(exc)}, indent=2), file=sys.stderr)
        return 1
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
