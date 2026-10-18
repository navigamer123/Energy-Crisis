# Rule: Build Portable Arcade Releases Inside Docker

## Context
The project targets DevHub One (DHO) Arcade devices running **NixOS** with **glibc 2.40** and **nix-ld 2.0.3**.
The host machine is running a rolling Linux distribution (CachyOS/Arch) with glibc 2.43+.

## Mandatory Procedure
- **DO NOT** compile the arcade release directly on the host machine.
- **DO NOT** copy host shared libraries from `/usr/lib` into the release.
- **ALWAYS** build the arcade package using the Debian 12 Docker container:
  ```bash
  make arcade-package
  ```
  or:
  ```bash
  ./scripts/build-linux-release.sh
  ```
- **DO NOT** bundle internal glibc libraries (`libc.so.6`, `ld-linux`, `libpthread`, `libm`, etc.) or GPU display drivers (`libGL`, `libGLX`, `libEGL`, `libdrm`).
- **ALWAYS** verify the output with `python3 scripts/check_glibc_symbols.py dist/Energy_Crisis`.
