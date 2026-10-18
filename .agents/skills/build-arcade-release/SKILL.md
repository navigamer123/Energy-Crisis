---
name: build-arcade-release
description: >-
  Builds, packages, and validates the portable Linux release of Energy Crisis for DHO Arcade
  running NixOS (glibc 2.40). Use whenever asked to build the arcade release, update the
  arcade package, fix glibc compatibility issues, or produce Energy_Crisis.zip.
---

# Build Arcade Release for DHO Arcade (NixOS)

Use this skill to quickly and reliably build, package, and validate the portable Linux release of Energy Crisis for DHO Arcade cabinets running NixOS.

## Background & Constraints

* **Target Device:** NixOS, glibc 2.40, nix-ld 2.0.3, ELF interpreter `/lib64/ld-linux-x86-64.so.2`.
* **Host Hazard:** The host system (CachyOS) runs glibc >= 2.43. Compiling or copying libraries from the host produces `GLIBC_2.43 not found` errors on the arcade.
* **Solution:** The build is executed strictly inside a pinned Debian 12 Docker container (glibc 2.36), which produces binaries 100% compatible with the arcade's glibc 2.40.
* **System Library Exclusion:** Glibc internals (`libc.so.6`, `ld-linux`, `libpthread`, `libm`, etc.) and graphics drivers (`libGL`, `libGLX`, `libEGL`, `libdrm`) MUST NOT be bundled in `lib/`.

---

## Fast Build Procedure

### Step 1: Run the Automated Build

From the workspace root directory, run:

```bash
make arcade-package
```

*(Alternatively: `./scripts/build-linux-release.sh`)*

This command automatically:
1. Builds the pinned Debian 12 Docker container `energy-crisis-arcade:debian12`.
2. Compiles SFML 3.1.0 and `Energy_Crisis` with `-DARCADE_MODE=1` inside Docker.
3. Collects non-system shared libraries and sets RPATH to `$ORIGIN/lib` via `patchelf`.
4. Runs container-side symbol validation.
5. Extracts artifacts to `dist/Energy_Crisis/` and builds `Energy_Crisis.zip`.
6. Runs host-side symbol validation.

---

## Step 2: Validate the Output

After the build completes, verify the output meets the compatibility criteria:

```bash
python3 scripts/check_glibc_symbols.py dist/Energy_Crisis
```

Expected output:
- `[PASS] All binaries verified. Maximum glibc version across all files: GLIBC_2.36 (Target: <= GLIBC_2.40)`
- No `libc.so.6` or `ld-linux` present in `dist/Energy_Crisis/lib/`.

Verify the main executable interpreter and RPATH:

```bash
readelf -l dist/Energy_Crisis/Energy_Crisis.x86_64 | grep interpreter
# Expected: /lib64/ld-linux-x86-64.so.2

objdump -p dist/Energy_Crisis/Energy_Crisis.x86_64 | grep -E 'RPATH|RUNPATH'
# Expected: RPATH                $ORIGIN/lib
```

---

## Step 3: Run Engine Tests (Optional Verification)

To verify the game engine logic remains intact:

```bash
make test
```

All 16 engine tests should pass.

---

## Output Artifacts

* **Release ZIP:** `Energy_Crisis.zip` (and `dist/Energy_Crisis-0.1.2.zip`)
* **Extracted Folder:** `dist/Energy_Crisis/`
* **Transitional Platform ZIP:** `dist/Energy_Crisis-0.1.2-platform.zip`

---

## Troubleshooting

- **Docker daemon not running:** Start Docker via `sudo systemctl start docker`.
- **Permission denied on docker:** Ensure user has access to Docker socket or run with proper user permissions.
- **Cache clean rebuild needed:** To force a complete rebuild of the container and SFML, run:
  ```bash
  docker build --no-cache -f docker/Dockerfile -t energy-crisis-arcade:debian12 .
  ./scripts/build-linux-release.sh
  ```
