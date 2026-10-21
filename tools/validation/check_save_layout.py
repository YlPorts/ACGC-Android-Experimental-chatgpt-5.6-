#!/usr/bin/env python3
"""Compile ABI probes using the real NDK; compare persistent layouts to V5.

This checks binary layout, not an Android runtime or a real game save.
"""
import argparse
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
GOLDEN = ROOT / 'tests/native/save_layout_v5.json'

def probe(root, ndk, target, expressions, native64):
    llvm = ndk / 'toolchains/llvm/prebuilt/linux-x86_64/bin'
    source = ('#include "m_common_data.h"\n#include <stddef.h>\n#include <stdint.h>\n'
              '__attribute__((used,section(".acgc_abi"))) const uint64_t values[] = {\n'
              + ',\n'.join(expressions) + '\n};\n')
    with tempfile.TemporaryDirectory(prefix='acgc-layout-') as temporary:
        work = Path(temporary)
        (work / 'probe.c').write_text(source)
        command = [str(llvm / (target + '-clang')), '-std=gnu11', '-DTARGET_PC',
                   '-DVERSION=0', '-DF3DEX_GBI_2', '-D_LANGUAGE_C',
                   '-I' + str(root / 'include'), '-I' + str(root / 'pc/include'),
                   '-c', str(work / 'probe.c'), '-o', str(work / 'probe.o')]
        if native64:
            command.append('-DPC_NATIVE_64')
        subprocess.run(command, check=True, capture_output=True, text=True)
        subprocess.run([str(llvm / 'llvm-objcopy'), '--dump-section',
                        '.acgc_abi=' + str(work / 'probe.bin'), str(work / 'probe.o')], check=True)
        return dict(zip(expressions, struct.unpack('<' + 'Q' * len(expressions),
                                                    (work / 'probe.bin').read_bytes())))

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ndk', type=Path, default=os.environ.get('ANDROID_NDK_HOME'))
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if args.ndk is None:
        parser.error('--ndk or ANDROID_NDK_HOME is required')
    golden = json.loads(GOLDEN.read_text())
    expected = golden['layout']
    reports = {}
    for abi, target in [('armeabi-v7a', 'armv7a-linux-androideabi23'),
                        ('arm64-v8a', 'aarch64-linux-android23')]:
        actual = probe(ROOT, args.ndk, target, list(expected), abi == 'arm64-v8a')
        different = {key: {'expected': expected[key], 'actual': actual[key]}
                     for key in expected if expected[key] != actual[key]}
        if different:
            raise SystemExit(abi + ': incompatible persistent layout: ' + json.dumps(different))
        reports[abi] = actual
        print(abi + ': ' + str(len(actual)) + ' persistent sizes/offsets match V5')
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(reports, indent=2) + '\n')

if __name__ == '__main__':
    main()
