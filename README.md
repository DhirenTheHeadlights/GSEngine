# GSEngine

GSEngine is a modular game engine written in modern C++20. It aims to provide a lightweight core that can be extended to fit a wide variety of projects. Originally created for a 3D shooter, the architecture is flexible enough to support many genres. Peruse the [wiki](https://github.com/DhirenTheHeadlights/GSEngine/wiki) to get started with GSEngine.

## Prerequisites

Install the following tools and ensure they are on your system `PATH`:

- CMake 3.26 or higher
- Git 2.45 or higher
- Python 3.11.9 or higher
- A compiler with C++20 module support
- Vulkan SDK

The `setup.py` script bootstraps third‑party packages through vcpkg on Windows. It installs:

- [Vulkan SDK](https://vulkan.lunarg.com/sdk/home)
- [GLFW3](https://www.glfw.org/)
- [STB](https://github.com/nothings/stb)
- [miniaudio](https://github.com/mackron/miniaudio)
- [msdfgen](https://github.com/Chlumsky/msdfgen)
- [freetype](https://github.com/freetype/freetype)
- [toml++](https://marzer.github.io/tomlplusplus/)
- [tbb](https://github.com/uxlfoundation/oneTBB)

## Quick Start

```
git clone <repo>
python bootstrap.py --persist
cmake --preset x64-Release
cmake --build --preset x64-Release
```

`bootstrap.py` runs `setup.py` (vcpkg + dependencies) and then downloads the prebuilt `clang-p2996` toolchain. Skip a step with `--skip-deps` or `--skip-clang`. Pass `--skip-clang` if you only need the MSVC presets.

## clang-p2996 Toolchain

MSVC's module implementation is unreliable on this codebase, so we build with a prebuilt fork of Clang that also includes preview support for [P2996 reflection](https://github.com/bloomberg/clang-p2996).

### As a teammate — just install

```
python scripts/install_clang_p2996.py --persist
```

This downloads the latest release zip to `%LOCALAPPDATA%\clang-p2996\<tag>` and sets `CLANG_P2996_ROOT`. Then use the `x64-clang-p2996-Debug` / `x64-clang-p2996-Release` CMake presets — they're hidden unless `CLANG_P2996_ROOT` is set, so they only appear once the install step has run.

Pin a specific tag with `--tag clang-p2996-v1` and verify the artifact with `--sha256 <hash>` (both are printed in the GitHub release notes).

### As a maintainer — cut a new release

The `build-clang-p2996` GitHub Actions workflow does the full build + release upload. Trigger it manually from the Actions tab with:

- `tag` — e.g. `clang-p2996-v2`
- `sha` — clang-p2996 commit SHA to pin (leave blank for branch tip, but **pinning is strongly recommended**)

The workflow produces a Windows x64 zip, computes its SHA256, and creates a draft release. Publish it after a sanity check, then bump `DEFAULT_TAG` in `scripts/install_clang_p2996.py` so `bootstrap.py` picks up the new release by default.

To build locally instead of on CI (e.g. on your dev box — ~5x faster than a free GH runner), run from an **x64 Native Tools Command Prompt**:

```
python scripts/build_clang_p2996.py --sha <commit>
```

Expect 30–90 minutes and ~30 GB of disk.

## Code Style

This project follows the standard library's coding conventions. The main branch is protected, so all contributions are made through pull requests.

For Visual Studio users, ReSharper C++ can apply the repository style automatically:

1. Go to `Extensions` → `ReSharper` → `Manage Options`.
2. Highlight `Solution "" Personal`.
3. Use the drop‑down `Copy to` and select `style_guide`.
4. Select all options and press `OK`.
