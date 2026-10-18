# Energy Crisis — Agent Guidelines & Project Rules

## CRITICAL RULE: Arcade Release Build Method
Whenever asked to build, package, test, or update the release for the **DHO Arcade (DevHub One / NixOS)** cabinet:

1. **ALWAYS BUILD INSIDE THE DOCKER CONTAINER**:
   - The DHO Arcade runs **NixOS with glibc 2.40** and **nix-ld 2.0.3**.
   - The host development machine runs a rolling release (CachyOS/Arch) with glibc >= 2.43.
   - **NEVER** build the arcade executable on the host machine.
   - **NEVER** copy host shared libraries from `/usr/lib/` or `/usr/lib64/` into `lib/` or the release archive.
   - Doing so will introduce `GLIBC_2.43` symbol mismatches and break the arcade deployment!

2. **Standard Arcade Build Commands**:
   To produce the portable arcade release, always run:
   ```bash
   make arcade-package
   ```
   or directly:
   ```bash
   ./scripts/build-linux-release.sh
   ```

3. **Docker Environment**:
   - Uses pinned Debian 12 Bookworm (`debian:12-slim@sha256:...`) which provides **glibc 2.36** (fully compatible with NixOS glibc 2.40).
   - Builds SFML 3.1.0 from source inside the container.
   - Packages `Energy_Crisis.x86_64`, `launch.sh`, metadata, and bundled libraries into `dist/Energy_Crisis/` and `Energy_Crisis.zip`.

4. **Forbidden Bundled Libraries**:
   - Never bundle internal glibc components (`libc.so.6`, `ld-linux`, `libpthread`, `libm`, etc.).
   - Never bundle GPU drivers (`libGL`, `libGLX`, `libEGL`, `libdrm`).
   - NixOS and `nix-ld` provide these at runtime. Bundling `libc.so.6` crashes `nix-ld` with `__pointer_chk_guard` undefined symbol.

5. **Verification**:
   - Always verify the generated binaries using:
     ```bash
     python3 scripts/check_glibc_symbols.py dist/Energy_Crisis
     ```
   - All symbols must satisfy `<= GLIBC_2.40`.

---

## Skills Available in Project
- Use skill `.agents/skills/build-arcade-release/SKILL.md` to run and validate the complete arcade build workflow.
