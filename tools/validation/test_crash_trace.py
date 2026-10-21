#!/usr/bin/env python3
"""Test the production Android tombstone parser without Android framework stubs."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix="acgc-tombstone-") as directory:
    subprocess.run(["javac", "-encoding", "UTF-8", "-d", directory,
                    str(ROOT / "android/app/src/main/java/com/acgc/port/TombstoneTrace.java"),
                    str(ROOT / "tests/java/com/acgc/port/TombstoneTraceTest.java")], check=True)
    subprocess.run(["java", "-cp", directory, "com.acgc.port.TombstoneTraceTest"], check=True)
