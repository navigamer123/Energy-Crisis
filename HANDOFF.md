# Energy Crisis — Arcade Release Handoff

## 1. Executive Summary

This handoff documents the portable Linux release system developed for **Energy Crisis** to run on **DHO Arcade (DevHub One)** cabinets powered by **NixOS**.

The build pipeline resolves all previous runtime issues (notably `GLIBC_2.43 not found` and `__pointer_chk_guard undefined symbol`) by isolating the compilation and packaging inside a reproducible **Debian 12 Bookworm** Docker container.

---

## 2. Target vs. Host Environment

| Parameter | Build Host (CachyOS / Arch) | Build Container (Docker) | Target Arcade Cabinet (NixOS) |
| :--- | :--- | :--- | :--- |
| **OS** | CachyOS (rolling) | Debian 12 (Bookworm slim) | NixOS |
| **glibc** | 2.44 / 2.43 | **2.36** | **2.40** |
| **Linker** | `/lib64/ld-linux-x86-64.so.2` | `/lib64/ld-linux-x86-64.so.2` | `nix-ld` 2.0.3 (`/lib64/ld-linux-x86-64.so.2`) |
| **Compatibility** | Incompatible (too new) | **Compatible with Target** | Target runtime environment |

---

## 3. Key Technical Decisions & Fixes

1. **Clean Container Isolation:**
   - Previous builds compiled on the host leaked dynamic symbol bindings up to `GLIBC_2.43` (e.g. `libm.so.6`, `libglib-2.0.so.0`).
   - Binaries built in Debian 12 only require up to `GLIBC_2.36`. Since the arcade runs glibc 2.40, all symbols are satisfied natively.

2. **System glibc Exclusion:**
   - The previous release bundled host glibc 2.44's `libc.so.6` and dynamic loader. NixOS's `nix-ld` loader crashed due to private glibc symbol mismatches (`__pointer_chk_guard`).
   - Glibc libraries (`libc.so.6`, `ld-linux`, `libpthread`, `libm`, etc.) and Mesa GPU drivers (`libGL`, `libGLX`, `libEGL`, `libdrm`) are now strictly excluded from `lib/`. NixOS provides them at runtime.

3. **Self-Contained Application Dependencies:**
   - SFML 3.1.0 is compiled from source in the container.
   - Non-system dependencies (HarfBuzz, FreeType, Vorbis, FLAC, Ogg, SQLite 3, X11 client libraries) are bundled in `lib/`.
   - Executable RPATH is configured to `$ORIGIN/lib`; bundled libraries have RPATH `$ORIGIN`.

4. **Multi-Stage Automated Verification:**
   - [scripts/check_glibc_symbols.py](file:///home/navi/Documents/Projects/Hacaton/Energy%20Crisis/scripts/check_glibc_symbols.py) runs inside Docker and on the host post-extraction, checking every ELF file against `GLIBC_2.40`.

---

## 4. How to Build & Package

Run from the workspace root:

```bash
make arcade-package
```

Or execute the build script directly:

```bash
./scripts/build-linux-release.sh
```

### Generated Files
* **Release Archive:** [Energy_Crisis.zip](file:///home/navi/Documents/Projects/Hacaton/Energy%20Crisis/Energy_Crisis.zip) (13 MB, ready for deployment)
* **Extracted Files:** [dist/Energy_Crisis/](file:///home/navi/Documents/Projects/Hacaton/Energy%20Crisis/dist/Energy_Crisis/)
* **Platform Uploader ZIP:** [dist/Energy_Crisis-0.1.2-platform.zip](file:///home/navi/Documents/Projects/Hacaton/Energy%20Crisis/dist/Energy_Crisis-0.1.2-platform.zip)

---

## 5. Deployment Instructions for the Arcade

1. Transfer `Energy_Crisis.zip` to the arcade cabinet:
   ```bash
   scp Energy_Crisis.zip arcade@<cabinet-ip>:/home/arcade/ArcadeData/games/
   ```
2. Unpack into `/home/arcade/ArcadeData/games/Energy_Crisis/`:
   ```bash
   ssh arcade@<cabinet-ip>
   cd /home/arcade/ArcadeData/games/
   mkdir -p Energy_Crisis
   unzip -o Energy_Crisis.zip -d Energy_Crisis/
   chmod +x Energy_Crisis/launch.sh Energy_Crisis/Energy_Crisis.x86_64
   ```
3. Test execute:
   ```bash
   cd /home/arcade/ArcadeData/games/Energy_Crisis
   ./launch.sh
   ```

---

## 6. Project Customizations Configured

- **Agent Rule:** [AGENTS.md](file:///home/navi/Documents/Projects/Hacaton/Energy%20Crisis/AGENTS.md) and [.agents/rules/arcade_build.md](file:///home/navi/Documents/Projects/Hacaton/Energy%20Crisis/.agents/rules/arcade_build.md) instruct any future AI agent in any session to build the arcade release inside Docker.
- **Custom Skill:** [.agents/skills/build-arcade-release/SKILL.md](file:///home/navi/Documents/Projects/Hacaton/Energy%20Crisis/.agents/skills/build-arcade-release/SKILL.md) provides automated runbooks and verification commands for fast building.
- **Documentation:** [docs/ARCADE_BUILD.md](file:///home/navi/Documents/Projects/Hacaton/Energy%20Crisis/docs/ARCADE_BUILD.md) and [README.md](file:///home/navi/Documents/Projects/Hacaton/Energy%20Crisis/README.md).
