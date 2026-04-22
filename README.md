# GSEngine

GSEngine is a modular game engine written in modern C++ with C++26 reflection. Originally created for a 3D shooter, the architecture is flexible enough to support many genres. Peruse the [wiki](https://github.com/DhirenTheHeadlights/GSEngine/wiki) to get started with GSEngine.

Targets Windows only (clang + libc++ + Vulkan).

## System Prerequisites

The bootstrap script handles the toolchain (clang-p2996, libc++, vcpkg deps) but you need these on your system first:

| Tool                       | Version  | winget command                          |
|----------------------------|----------|-----------------------------------------|
| Python                     | 3.11+    | `winget install Python.Python.3.13`     |
| Git                        | 2.45+    | `winget install Git.Git`                |
| CMake                      | 4.0+     | `winget install Kitware.CMake`          |
| Ninja                      | 1.11+    | `winget install Ninja-build.Ninja`      |
| Visual Studio Build Tools  | 2022/2026| `winget install Microsoft.VisualStudio.2022.BuildTools` (with **C++ workload + Windows 11 SDK**) |
| Vulkan SDK                 | 1.4+     | <https://vulkan.lunarg.com/sdk/home>    |

Visual Studio Build Tools are required even though we use clang — they provide the Windows CRT (`vcruntime`/`ucrt`), the Windows SDK headers/libs, `link.exe`/`mt.exe`, and the `vcvars64.bat` environment that clang needs to find them.

## Quick Start

```
git clone <repo>
python bootstrap.py --persist
cmake --preset x64-clang-p2996-libcxx-Release
cmake --build --preset x64-clang-p2996-libcxx-Release
```

`bootstrap.py` runs:
1. `git submodule update --init --recursive` (fetches vcpkg)
2. `scripts/install_clang_p2996.py` (downloads prebuilt clang-p2996 to `%LOCALAPPDATA%\clang-p2996\<tag>` and sets `CLANG_P2996_ROOT`)
3. `scripts/build_libcxx_p2996.py` (clones the libc++ source and builds it against MSVC ABI, installing into the same clang dir)

vcpkg's manifest mode (`vcpkg.json`) auto-installs dependencies when CMake configures.

Skip flags: `--skip-submodules`, `--skip-clang`, `--skip-libcxx`. Re-running is idempotent — clang and libc++ are skipped if already installed.

Run all of this from an **x64 Native Tools Command Prompt** (or **Developer PowerShell for VS**) so vcvars is loaded.

## Dependencies (vcpkg manifest)

Listed in `vcpkg.json` and auto-installed at configure time:

- [GLFW3](https://www.glfw.org/) — windowing/input
- [STB](https://github.com/nothings/stb) — image loading
- [miniaudio](https://github.com/mackron/miniaudio) — audio
- [shader-slang](https://github.com/shader-slang/slang) — shader compiler
- [concurrentqueue](https://github.com/cameron314/concurrentqueue) — lock-free MPMC queue used by the task scheduler
- [toml++](https://marzer.github.io/tomlplusplus/) — config files
- [freetype](https://github.com/freetype/freetype) — font rasterization
- [msdfgen](https://github.com/Chlumsky/msdfgen) — multi-channel SDF font generation

## clang-p2996 Toolchain

This project is built with a fork of Clang that includes preview support for [P2996 reflection](https://github.com/bloomberg/clang-p2996). `bootstrap.py` downloads the prebuilt artifact; `scripts/install_clang_p2996.py` is the same script run standalone.

Pin a specific tag with `--clang-tag clang-p2996-v1`.

To build the clang toolchain locally run from an **x64 Native Tools Command Prompt**:

```
python scripts/build_clang_p2996.py --sha <commit>
```

## Code Style

This project follows the standard library's coding conventions. The main branch is protected, so all contributions are made through pull requests.
