#!/usr/bin/env python3
"""Archive unstripped engine/SDL binaries only when Build IDs match the APK."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument("apk", type=Path)
p.add_argument("--ndk", type=Path, required=True)
p.add_argument("--output", type=Path, required=True)
a = p.parse_args()
readelf = a.ndk / "toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf"


def build_id(path):
    text = subprocess.check_output([str(readelf), "--notes", str(path)], text=True)
    values = re.findall(r"Build ID: ([0-9a-f]+)", text)
    assert len(values) == 1, f"missing/ambiguous Build ID: {path}"
    return values[0]


report = {"apk": a.apk.name, "apk_sha256": hashlib.sha256(a.apk.read_bytes()).hexdigest(), "libraries": []}
with zipfile.ZipFile(a.apk) as apk, tempfile.TemporaryDirectory(prefix="acgc-symbols-") as directory:
    for name in sorted(apk.namelist()):
        if not name.startswith("lib/") or not name.endswith(("/libmain.so", "/libSDL2.so")):
            continue
        _, abi, library = name.split("/")
        packaged = Path(directory) / library
        packaged.write_bytes(apk.read(name))
        expected = build_id(packaged)
        # Gradle's CMake hash is independent of architecture; old configurations
        # may coexist. Select by the actual APK's Build ID, never just a path.
        candidates = (ROOT / "android/app/build/intermediates/cxx").glob(f"*/*/obj/{abi}/{library}")
        matching = [path for path in candidates if build_id(path) == expected]
        assert matching, f"no matching unstripped binary for {name} ({expected})"
        source = matching[0]
        sections = subprocess.check_output([str(readelf), "--sections", str(source)], text=True)
        assert ".debug_info" in sections, f"DWARF debug information missing from {source}"
        target = a.output / abi / library
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        report["libraries"].append({"abi": abi, "library": library, "build_id": expected,
                                    "sha256": hashlib.sha256(target.read_bytes()).hexdigest(), "dwarf": True})
assert report["libraries"], "no engine/SDL libraries in the APK"
(a.output / "native-symbols.json").write_text(json.dumps(report, indent=2) + "\n")
print(f"PASS {len(report['libraries'])} unstripped native libraries match APK Build IDs")
