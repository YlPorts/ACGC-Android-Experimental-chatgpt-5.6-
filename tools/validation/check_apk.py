#!/usr/bin/env python3
"""Validate every packaged ELF, dependencies, JNI, alignment and APK signature."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import zipfile

ABIS = {"armeabi-v7a": (1, 40, "arm-linux-androideabi"),
        "arm64-v8a": (2, 183, "aarch64-linux-android")}
REQUIRED = {"libmain.so", "libSDL2.so", "libc++_shared.so"}
MAIN_JNI = {"SDL_main", "Java_com_acgc_port_GameActivity_nativeTakePendingActions",
            "Java_com_acgc_port_GameActivity_nativeGetAndroidSetting"}
SDL_JNI = {"Java_org_libsdl_app_SDLActivity_nativeSetupJNI",
           "Java_org_libsdl_app_SDLActivity_nativeRunMain",
           "Java_org_libsdl_app_SDLControllerManager_nativeSetupJNI",
           "Java_org_libsdl_app_SDLAudioManager_nativeSetupJNI"}


def output(command):
    return subprocess.check_output([str(x) for x in command], text=True, stderr=subprocess.STDOUT)


def elf_header(data, abi, name):
    cls, machine, _ = ABIS[abi]
    assert data[:4] == b"\x7fELF" and data[4] == cls and data[5] == 1, f"{name}: wrong ELF class/encoding for {abi}"
    assert struct.unpack_from("<H", data, 18)[0] == machine, f"{name}: wrong machine for {abi}"
    if cls == 2:
        offset = struct.unpack_from("<Q", data, 32)[0]
        stride, count = struct.unpack_from("<HH", data, 54)
        align_offset, fmt = 48, "<Q"
    else:
        offset = struct.unpack_from("<I", data, 28)[0]
        stride, count = struct.unpack_from("<HH", data, 42)
        align_offset, fmt = 28, "<I"
    aligns = []
    for i in range(count):
        p = offset + i * stride
        if struct.unpack_from("<I", data, p)[0] == 1:
            align = struct.unpack_from(fmt, data, p + align_offset)[0]
            aligns.append(align)
            if cls == 2:
                assert align >= 16384, f"{name}: LOAD alignment {align} cannot support 16 KB pages"
    assert aligns, f"{name}: no load segments"
    return aligns


def symbols(readelf, path):
    defined, needed = set(), set()
    for line in output([readelf, "--dyn-syms", "--wide", path]).splitlines():
        parts = line.split()
        if len(parts) < 8 or not parts[0].endswith(":") or not parts[0][:-1].isdigit():
            continue
        name = parts[7].split("@")[0]
        if parts[6] != "UND":
            defined.add(name)
        elif parts[4] != "WEAK":
            needed.add(name)
    dependencies = re.findall(r"\(NEEDED\).*?\[(.*?)\]", output([readelf, "--dynamic", "--wide", path]))
    return defined, needed, dependencies


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("apk", type=Path)
    parser.add_argument("--abi", action="append", required=True, choices=ABIS)
    parser.add_argument("--ndk", type=Path, required=True)
    parser.add_argument("--build-tools", type=Path, required=True)
    parser.add_argument("--keystore", type=Path)
    parser.add_argument("--match-signature", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    sdk_tools = args.build_tools
    compiler = next((args.ndk / "toolchains/llvm/prebuilt").glob("*"))
    readelf = compiler / "bin/llvm-readelf"
    report = {"apk": args.apk.name, "sha256": hashlib.sha256(args.apk.read_bytes()).hexdigest(),
              "release_status": "candidate", "gameplay_tested": False, "abis": {}}
    with tempfile.TemporaryDirectory(prefix="acgc-apk-") as tmp:
        tmp = Path(tmp)
        with zipfile.ZipFile(args.apk) as apk:
            files = [n for n in apk.namelist() if n.startswith("lib/") and n.endswith(".so")]
            packaged_abis = {n.split("/")[1] for n in files}
            assert packaged_abis == set(args.abi), f"ABI mismatch: {packaged_abis} != {set(args.abi)}"
            assert len(files) == len(set(files)), "duplicate native library entry"
            for entry in files:
                assert len(entry.split("/")) == 3, f"unexpected library path {entry}"
                target = tmp / entry
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(apk.read(entry))
        for abi in sorted(packaged_abis):
            libraries = {p.name: p for p in (tmp / "lib" / abi).glob("*.so")}
            assert REQUIRED <= libraries.keys(), f"{abi}: missing {REQUIRED-libraries.keys()}"
            exports, imports = {}, {}
            descriptions = {}
            dependencies = set()
            for name, path in libraries.items():
                aligns = elf_header(path.read_bytes(), abi, name)
                defined, undefined, needed = symbols(readelf, path)
                exports[name], imports[name] = defined, undefined
                dependencies.update(needed)
                descriptions[name] = {"load_alignment": aligns, "needed": needed}
            assert MAIN_JNI <= exports["libmain.so"], f"{abi}: missing app JNI or SDL_main"
            assert SDL_JNI <= exports["libSDL2.so"], f"{abi}: missing SDL JNI"
            # Resolve imports against all packaged dependencies and the real
            # minSdk=23 NDK platform stubs. Also inspect transitive dependencies.
            available = set().union(*exports.values())
            pending = dependencies - libraries.keys()
            platform = set()
            while pending:
                name = pending.pop()
                if name in platform:
                    continue
                path = compiler / "sysroot/usr/lib" / ABIS[abi][2] / "23" / name
                assert path.is_file(), f"{abi}: dependency {name} is neither packaged nor available on API 23"
                defined, _, needed = symbols(readelf, path)
                available.update(defined)
                platform.add(name)
                pending.update(set(needed) - platform - libraries.keys())
            for name, required in imports.items():
                unresolved = required - available
                assert not unresolved, f"{abi}/{name}: unresolved {sorted(unresolved)}"
            report["abis"][abi] = descriptions
    output([sdk_tools / "zipalign", "-c", "-P", "16", "-v", "4", args.apk])
    certificate = output([sdk_tools / "apksigner", "verify", "--verbose", "--print-certs", args.apk])
    digest = re.search(r"certificate SHA-256 digest: ([0-9a-f]+)", certificate)[1]
    report["certificate_sha256"] = digest
    if args.keystore:
        cert = subprocess.check_output(["keytool", "-exportcert", "-alias", "acgc-public", "-keystore",
                                        str(args.keystore), "-storepass:env", "ACGC_KEYSTORE_PASSWORD"])
        assert hashlib.sha256(cert).hexdigest() == digest, "APK is not signed with the existing stable key"
    if args.match_signature:
        other = output([sdk_tools / "apksigner", "verify", "--print-certs", args.match_signature])
        expected = re.search(r"certificate SHA-256 digest: ([0-9a-f]+)", other)[1]
        assert digest == expected, "signature differs from the supplied previous APK"
    badging = output([sdk_tools / "aapt", "dump", "badging", args.apk])
    assert "package: name='com.acgc.port'" in badging, "application ID changed"
    assert "versionCode='6'" in badging and "sdkVersion:'23'" in badging, "unexpected version/minSdk"
    report["verified"] = True
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
