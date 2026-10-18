# Building Energy Crisis for DHO Arcade (NixOS)

This guide documents how to build and package portable, self-contained releases of **Energy Crisis** for **DHO Arcade (DevHub One)** cabinets running **NixOS**.

---

## 🎯 Target System Specifications

* **Operating System:** NixOS (Linux x86_64)
* **glibc version:** **2.40**
* **Dynamic Linker / nix-ld:** **2.0.3**
* **System ELF Interpreter:** `/lib64/ld-linux-x86-64.so.2` (managed via `nix-ld`)
* **`NIX_LD` Path:** `/run/current-system/sw/share/nix-ld/lib/ld.so`
* **`NIX_LD_LIBRARY_PATH`:** `/run/current-system/sw/share/nix-ld/lib`
* **Target Installation Path:** `/home/arcade/ArcadeData/games/Energy_Crisis/`

---

## ⚠️ Critical Architecture Constraints

### 1. Always Build Inside Docker (Debian 12 Bookworm)
* Modern development hosts (such as CachyOS or Arch Linux) run rolling-release glibc versions (**2.43+**).
* Building the game or bundling shared libraries directly on a rolling-release host injects `GLIBC_2.43` symbol requirements that the arcade's glibc 2.40 cannot satisfy.
* **Debian 12 Bookworm uses glibc 2.36**. All binaries compiled in Debian 12 run seamlessly on the arcade's glibc 2.40.

### 2. Never Bundle Internal glibc or GPU Driver Libraries
* **Do NOT bundle:** `libc.so.6`, `ld-linux-x86-64.so.2`, `libm.so.6`, `libpthread.so.0`, `libdl.so.2`, `librt.so.1`, or `linux-vdso.so.1`.
* **Do NOT bundle GPU drivers:** `libGL.so.1`, `libGLX.so.0`, `libOpenGL.so.0`, `libEGL.so.1`, `libdrm.so.2`.
* *Why:* Bundling `libc.so.6` crashes `nix-ld` with `undefined symbol: __pointer_chk_guard, version GLIBC_PRIVATE`. Graphics drivers must match the hardware on the physical cabinet. NixOS and `nix-ld` provide these at runtime.

### 3. Bundled Application Libraries
The release bundles its own application runtime dependencies inside `lib/` with `RPATH=$ORIGIN`:
* SFML 3.1.0 (`libsfml-graphics`, `libsfml-window`, `libsfml-audio`, `libsfml-system`)
* SQLite 3 (`libsqlite3.so.0`)
* Audio / font engines (`libfreetype`, `libharfbuzz`, `libvorbis`, `libFLAC`, `libogg`)
* Client X11 libraries (`libX11`, `libxcb`, `libXrandr`, `libXcursor`, `libudev`, etc.)

---

## 🚀 How to Build

### Prerequisites
* **Docker** installed and running on the build host (no local C++ compiler or SFML installation is needed on the host).

### Command

Run either of the following equivalent commands from the project root:

```bash
# Recommended via Makefile:
make arcade-package

# Or directly execute the release script:
./scripts/build-linux-release.sh
```

### What the Build Script Does
1. Builds a clean, pinned Docker image (`debian:12-slim@sha256:7c7b2c9...`).
2. Compiles SFML 3.1.0 from source using Ninja and GCC 12 inside the container.
3. Compiles `Energy_Crisis` with `-DARCADE_MODE=1` inside the container.
4. Gathers all non-system runtime libraries into `lib/` and sets `RPATH` via `patchelf`.
5. Validates inside the container that no symbol requirement exceeds `GLIBC_2.40`.
6. Extracts the release files into `dist/Energy_Crisis/`.
7. Performs host-side symbol validation using `scripts/check_glibc_symbols.py`.
8. Generates all distribution archives:
   - `Energy_Crisis.zip`
   - `Energy_Crysis.zip` (legacy alias)
   - `dist/Energy_Crisis-0.1.2.zip`
   - `dist/Energy_Crisis-0.1.2-platform.zip`

---

## 📦 Output Package Structure

The resulting `Energy_Crisis.zip` contains:

```text
Energy_Crisis/
├── Energy_Crisis.x86_64     # Main game executable (glibc <= 2.36, RPATH=$ORIGIN/lib)
├── Energy_Crisis            # Compatibility symlink
├── launch.sh                # Clean POSIX launch script (sets LD_LIBRARY_PATH and execs)
├── metadata.json            # Arcade metadata (title, controls, execution entry)
├── capability_manifest.json # Arcade capabilities manifest
├── game_release.json        # Release metadata
├── thumbnail.png            # Arcade menu artwork
├── NOTICE.txt               # Build and compatibility notice
├── assets/                  # Game fonts, graphics, audio, localization (bg/en)
└── lib/                     # 32 bundled SFML and runtime shared libraries
```

---

## 🔍 Validation Checklist

To independently verify any release directory or ZIP before deploying:

```bash
# 1. Run the GLIBC symbol verification tool:
python3 scripts/check_glibc_symbols.py dist/Energy_Crisis

# 2. Verify ELF interpreter of the executable:
readelf -l dist/Energy_Crisis/Energy_Crisis.x86_64 | grep interpreter
# Expected: /lib64/ld-linux-x86-64.so.2

# 3. Verify RPATH:
objdump -p dist/Energy_Crisis/Energy_Crisis.x86_64 | grep -E 'RPATH|RUNPATH'
# Expected: RPATH                $ORIGIN/lib

# 4. Verify no internal libc or Mesa libraries were bundled:
ls dist/Energy_Crisis/lib/libc.so* dist/Energy_Crisis/lib/libGL.so* 2>/dev/null || echo "PASS: No forbidden system libraries bundled."
```

---

## 🕹️ Deploying to the Arcade Device

1. Copy `Energy_Crisis.zip` to the arcade cabinet:
   ```bash
   scp Energy_Crisis.zip arcade@<cabinet-ip>:/home/arcade/ArcadeData/games/
   ```
2. Unpack into the target game directory:
   ```bash
   ssh arcade@<cabinet-ip>
   cd /home/arcade/ArcadeData/games/
   mkdir -p Energy_Crisis
   unzip -o Energy_Crisis.zip -d Energy_Crisis/
   chmod +x Energy_Crisis/launch.sh Energy_Crisis/Energy_Crisis.x86_64
   ```
3. Test launch directly via arcade shell:
   ```bash
   cd /home/arcade/ArcadeData/games/Energy_Crisis
   ./launch.sh
   ```
