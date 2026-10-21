#!/usr/bin/env python3
"""Verify external signing without changing a previously validated CI payload."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import zipfile


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def payload(apk):
    names = apk.namelist()
    assert len(names) == len(set(names)), "duplicate ZIP entries"
    return {n for n in names if not re.fullmatch(
        r"META-INF/(?:MANIFEST\.MF|[^/]+\.(?:SF|RSA|DSA|EC))", n, re.IGNORECASE)}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("apk", type=Path)
    p.add_argument("--unsigned", type=Path, required=True)
    p.add_argument("--build-report", type=Path, required=True)
    p.add_argument("--build-tools", type=Path, required=True)
    p.add_argument("--certificate-sha256", required=True)
    p.add_argument("--abi", action="append", required=True)
    p.add_argument("--build-run-id", required=True)
    p.add_argument("--output", type=Path, required=True)
    a = p.parse_args()
    prior = json.loads(a.build_report.read_text())
    assert prior["verified"] and prior["release_status"] == "unsigned-build"
    assert prior["signature_verified"] is False
    assert prior["sha256"] == digest(a.unsigned), "unsigned input differs from the validated build"
    assert set(prior["abis"]) == set(a.abi), "wrong architecture product"
    assert prior["jni_methods_verified"] >= 54, "incomplete compiled Java JNI validation"
    with zipfile.ZipFile(a.unsigned) as original, zipfile.ZipFile(a.apk) as signed:
        files = payload(original)
        assert files == payload(signed), "signing added or removed application payload files"
        for name in sorted(files):
            assert original.read(name) == signed.read(name), f"signing changed application payload: {name}"
    tools = a.build_tools
    subprocess.run([str(tools / "zipalign"), "-c", "-P", "16", "4", str(a.apk)], check=True)
    verification = subprocess.check_output(
        [str(tools / "apksigner"), "verify", "--verbose", "--print-certs", str(a.apk)], text=True)
    certificates = re.findall(r"certificate SHA-256 digest: ([0-9a-f]+)", verification)
    assert certificates == [a.certificate_sha256.lower()], "unexpected signing certificate or multiple signers"
    report = dict(prior, apk=a.apk.name, sha256=digest(a.apk), release_status="candidate",
                  certificate_sha256=certificates[0], signature_verified=True,
                  ci_build_run_id=a.build_run_id, unsigned_sha256=prior["sha256"],
                  payload_unchanged=True, gameplay_tested=False)
    a.output.parent.mkdir(parents=True, exist_ok=True)
    a.output.write_text(json.dumps(report, indent=2) + "\n")
    print(f"{a.apk.name}: signature verified; all {len(files)} application files match CI build {a.build_run_id}")


if __name__ == "__main__":
    main()
