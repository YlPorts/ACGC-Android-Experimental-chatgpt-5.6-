#!/usr/bin/env python3
"""Run isolated engine tests. Cross runners execute tests, not Android gameplay."""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]


def run(command, **kwargs):
    subprocess.run([str(x) for x in command], cwd=ROOT, check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default=os.environ.get("CC", "cc"))
    parser.add_argument("--cxx", default=os.environ.get("CXX", "c++"))
    parser.add_argument("--runner", default="", help="e.g. qemu-aarch64")
    parser.add_argument("--static", action="store_true")
    parser.add_argument("--sanitize", action="store_true")
    parser.add_argument("--output", type=Path, default=ROOT / "build/native-tests")
    args = parser.parse_args()
    if args.sanitize:
        os.environ["ASAN_OPTIONS"] = "detect_leaks=1:halt_on_error=1"
        os.environ["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"
    if args.static and args.sanitize:
        parser.error("static cross tests and host sanitizers are separate runs")
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    cc, cxx, runner = shlex.split(args.cc), shlex.split(args.cxx), shlex.split(args.runner)
    macros = subprocess.check_output(cc + ["-dM", "-E", "-x", "c", "/dev/null"], text=True)
    bits = 8 * int(re.search(r"#define __SIZEOF_POINTER__ (\d+)", macros)[1])
    flags = ["-DTARGET_PC", "-D_LANGUAGE_C", "-DF3DEX_GBI_2", "-DVERSION=0",
             "-Iinclude", "-Ipc/include", "-Ipc/lib/fixnes", "-Ipc/lib/glad/include", "-Iandroid/SDL/include", "-fno-strict-aliasing", "-fwrapv", "-g"]
    if bits == 64:
        flags.append("-DPC_NATIVE_64")
    if args.sanitize:
        flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    if args.static:
        flags.append("-static")
    run(cc + ["-std=gnu11"] + flags + ["-c", "pc/src/pc_address.c", "-o", out / "address.o"])
    original = (ROOT / "src/static/jaudio_NES/game/game64_dummy.c_inc").read_text()
    for name in ("NIN_BANK", "NIN_WAVE"):
        body = re.search(r"u8 " + name + r"\[\][^=]*=\s*\{([^}]+)\};", original, re.S)[1]
        (out / (name + ".bin")).write_bytes(bytes(int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]+)", body)))
    audio = "src/static/jaudio_NES/internal/"
    cases = {
        "address": (["pc/src/pc_address.c"], ["-pthread"], []),
        "gbi": (["pc/src/pc_gbi_runtime.c", "pc/src/pc_address.c"], [], []),
        "aram": (["pc/src/pc_aram.c"], [], []),
        "os_contract": (["pc/src/pc_os.c"], ["-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections"], []),
        "dvd_audio": ([audio + "dvdthread.c", audio + "sample.c", "pc/src/pc_aram.c", "pc/src/pc_address.c"],
                      ["-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections"], []),
        "dvd_backend": ([audio + "dvdthread.c", audio + "sample.c", "pc/src/pc_aram.c", "pc/src/pc_address.c",
                         "pc/src/pc_dvd.c", "pc/src/pc_disc.c"],
                        ["-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections"], []),
        "scene": ([], [], []),
        "language": (["pc/src/pc_language.c", "src/static/jaudio_NES/game/audioheaders.c",
                      "src/data/item/item_name.c"], [], []),
        "audio_bank": (["pc/src/pc_audio_bank.c"], [], []),
        "bx": (["pc/src/pc_bx.c"], [], [out / "NIN_BANK.bin", out / "NIN_WAVE.bin"]),
        "guest_memory": (["pc/src/pc_guest_memory.c"], [], []),
        "audio_commands": ([audio + name + ".c" for name in ("cmdstack", "rspsim", "sample", "rate")]
                           + ["pc/src/pc_address.c"], ["-lm"], []),
        "audio_queue": ([audio + name + ".c" for name in ("os", "sub_sys", "audiowork")],
                        ["-pthread", "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections"], []),
        "nes": (["pc/src/pc_nes_fixnes.c"] + [str(p.relative_to(ROOT)) for p in sorted((ROOT / "pc/lib/fixnes").glob("*.c"))]
                + [str(p.relative_to(ROOT)) for p in sorted((ROOT / "pc/lib/fixnes/mapper").glob("*.c"))],
                ["-lm", "-Wl,--gc-sections", "-ffunction-sections", "-fdata-sections", "-O2"], []),
        "save": (["pc/src/pc_save_bswap.c"], [], ["--create-fixture", out / "fixture.save"]),
    }
    results = []
    for name, (sources, libs, arguments) in cases.items():
        started = time.monotonic()
        executable = out / (name + "_test")
        compile_flags = [x for x in libs if x.startswith("-f") or x in ("-O2", "-pthread")]
        run(cc + ["-std=gnu11"] + flags + compile_flags + [f"tests/native/{name}_test.c"] + sources
            + ["-o", executable] + libs)
        run(runner + [executable] + arguments)
        if name == "save":
            run(runner + [executable, out / "fixture.save", out / "roundtrip.save"])
            assert (out / "fixture.save").read_bytes() == (out / "roundtrip.save").read_bytes()
        elapsed = time.monotonic() - started
        results.append({"test": name, "passed": True, "seconds": round(elapsed, 3)})
        print(f"PASS {bits}-bit {name} ({elapsed:.2f}s)", flush=True)
    executable = out / "pointer32_test"
    run(cxx + ["-std=c++17"] + flags + ["tests/native/pointer32_test.cpp", out / "address.o", "-o", executable])
    run(runner + [executable])
    results.append({"test": "pointer32", "passed": True})
    executable = out / "heap_test"
    kernel = "src/static/JSystem/JKernel/"
    sources = [kernel + n + ".cpp" for n in ("JKRExpHeap", "JKRHeap", "JKRDisposer")]
    sources += ["src/static/JSystem/JSupport/JSUList.cpp"]
    run(cxx + ["-std=c++17"] + flags + ["-ffunction-sections", "-fdata-sections",
        "tests/native/heap_test.cpp"] + sources + ["-Wl,--gc-sections", "-o", executable])
    run(runner + [executable])
    results.append({"test": "heap", "passed": True})
    print(f"PASS {bits}-bit heap allocation/resize/free stress", flush=True)
    report = {"bits": bits, "compiler": args.cc, "runner": args.runner or "host",
              "sanitizers": args.sanitize, "gameplay_tested": False, "tests": results}
    (out / "results.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"PASS {bits}-bit suite; {len(results)} isolated tests passed", flush=True)


if __name__ == "__main__":
    main()
