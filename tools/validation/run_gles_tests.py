#!/usr/bin/env python3
"""Render production Android GLES paths using Mesa EGL, without a game image."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "build/tests/gles")
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    flags = ["-O1", "-g", "-DTARGET_PC", "-DTARGET_ANDROID", "-DPC_NATIVE_64", "-D_LANGUAGE_C",
             "-DF3DEX_GBI_2", "-DVERSION=0", "-fno-strict-aliasing", "-fwrapv", "-ffunction-sections",
             "-fdata-sections", "-Iinclude", "-Ipc/include"]
    flags += shlex.split(subprocess.check_output(["pkg-config", "--cflags", "sdl2", "egl", "glesv2"], text=True))
    objects = []

    def compile_source(source, cxx=False):
        obj = out / (Path(source).stem + ".o")
        # GNU C++ enables glibc's unrelated fsqrt(double) extension. The game
        # declares fsqrt(float); Android/bionic does not expose that extension.
        extra = ["-U_GNU_SOURCE", "-D_POSIX_C_SOURCE=200809L", "-fpermissive", "-Wno-narrowing"] if cxx else []
        subprocess.run(["c++" if cxx else "cc", "-std=gnu++17" if cxx else "-std=gnu11",
                        *flags, *extra, "-c", source, "-o", obj], cwd=ROOT, check=True)
        objects.append(obj)

    for name in ("pc_gx", "pc_gx_texture", "pc_gx_tev", "pc_profiler", "pc_address", "pc_settings",
                 "pc_texture_pack", "pc_mtx", "pc_gbi_runtime", "pc_os", "pc_misc", "pc_diagnostics"):
        compile_source(f"pc/src/{name}.c")
    compile_source("src/static/libforest/emu64/emu64.c", True)
    compile_source("tests/graphics/gles_render_test.c")
    executable = out / "gles_render_test"
    libraries = shlex.split(subprocess.check_output(["pkg-config", "--libs", "sdl2", "egl", "glesv2"], text=True))
    subprocess.run(["c++", *objects, "-Wl,--gc-sections", *libraries, "-lm", "-o", executable], check=True)
    shaders = out / "shaders"
    shaders.mkdir(exist_ok=True)
    for name in ("default.vert", "default.frag"):
        (shaders / name).write_bytes((ROOT / "pc/shaders" / name).read_bytes())
    env = dict(os.environ, LIBGL_ALWAYS_SOFTWARE="1", MESA_SHADER_CACHE_DIR=str(out / "mesa-cache"))
    for mode, arguments in (("specialized", []), ("uber", ["--uber"])):
        result = subprocess.run([executable, *arguments], cwd=out, env=env, text=True, capture_output=True, timeout=120)
        (out / (mode + ".log")).write_text(result.stdout + result.stderr)
        print(result.stdout, end="")
        if result.returncode:
            print(result.stderr, end="")
            raise SystemExit(result.returncode)
    (out / "results.json").write_text(json.dumps({"passed": True, "api": "OpenGL ES 3", "renderer": "Mesa software",
        "production_paths": ["GX", "TEV shaders", "Emu64"], "shader_modes": ["specialized", "uber"],
        "gameplay_tested": False, "physical_mali_tested": False}, indent=2) + "\n")


if __name__ == "__main__":
    main()
