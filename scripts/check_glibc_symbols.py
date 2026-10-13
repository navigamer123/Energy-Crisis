#!/usr/bin/env python3
import os
import re
import subprocess
import sys

MAX_ALLOWED_GLIBC = (2, 40)

def parse_glibc_versions(elf_path):
    versions = set()
    try:
        output = subprocess.check_output(
            ["readelf", "-V", elf_path],
            stderr=subprocess.DEVNULL,
            universal_newlines=True
        )
        for match in re.finditer(r"GLIBC_([0-9]+)\.([0-9]+)", output):
            major = int(match.group(1))
            minor = int(match.group(2))
            versions.add((major, minor))
    except Exception as e:
        print(f"Warning: could not inspect {elf_path}: {e}", file=sys.stderr)
    return versions

def is_elf(path):
    if not os.path.isfile(path) or os.path.islink(path):
        return False
    try:
        with open(path, "rb") as f:
            magic = f.read(4)
            return magic == b"\x7fELF"
    except Exception:
        return False

def main():
    target_dir = sys.argv[1] if len(sys.argv) > 1 else "."
    print(f"==> Inspecting GLIBC version requirements in: {target_dir}")

    elf_files = []
    for root, _, files in os.walk(target_dir):
        for file in files:
            p = os.path.join(root, file)
            if is_elf(p):
                elf_files.append(p)

    if not elf_files:
        print("ERROR: No ELF files found to check!", file=sys.stderr)
        sys.exit(1)

    max_overall = (0, 0)
    violations = []
    summary = []

    for elf in sorted(elf_files):
        rel = os.path.relpath(elf, target_dir)
        versions = parse_glibc_versions(elf)
        if not versions:
            summary.append((rel, "None"))
            continue
        max_v = max(versions)
        if max_v > max_overall:
            max_overall = max_v
        v_str = f"GLIBC_{max_v[0]}.{max_v[1]}"
        summary.append((rel, v_str))
        if max_v > MAX_ALLOWED_GLIBC:
            violations.append((rel, v_str))

    print(f"\nDiscovered {len(elf_files)} ELF binaries/libraries:")
    for rel, v_str in summary:
        print(f"  - {rel:<40} max required: {v_str}")

    print(f"\nHighest required GLIBC version: GLIBC_{max_overall[0]}.{max_overall[1]}")
    print(f"Target system allowance:       GLIBC_{MAX_ALLOWED_GLIBC[0]}.{MAX_ALLOWED_GLIBC[1]}")

    if violations:
        print("\n[FAIL] Found ELF objects exceeding target GLIBC version:", file=sys.stderr)
        for rel, v_str in violations:
            print(f"  * {rel}: {v_str} > GLIBC_{MAX_ALLOWED_GLIBC[0]}.{MAX_ALLOWED_GLIBC[1]}", file=sys.stderr)
        sys.exit(1)

    print("\n[PASS] All ELF objects satisfy glibc <= 2.40 compatibility requirement!")
    sys.exit(0)

if __name__ == "__main__":
    main()
