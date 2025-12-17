# GSEngine

GSEngine is a modular game engine written in modern C++20. It aims to provide a lightweight core that can be extended to fit a wide variety of projects. Originally created for a 3D shooter, the architecture is flexible enough to support many genres. Peruse the [wiki](https://github.com/DhirenTheHeadlights/GSEngine/wiki) to get started with GSEngine.

## Prerequisites

Install the following tools and ensure they are on your system `PATH`:

- CMake 3.26 or higher
- Git 2.45 or higher
- Python 3.11.9 or higher
- A compiler with C++20 module support
- Vulkan SDK

The `setup.py` script can bootstrap third‑party packages through vcpkg on Windows; vcpkg will look for:

- [Vulkan SDK](https://vulkan.lunarg.com/sdk/home)
- [GLFW3](https://www.glfw.org/)
- [STB](https://github.com/nothings/stb)
- [miniaudio](https://github.com/mackron/miniaudio)
- [msdfgen](https://github.com/Chlumsky/msdfgen)
- [freetype](https://github.com/freetype/freetype)
- [toml++](https://marzer.github.io/tomlplusplus/)
- [tbb](https://github.com/uxlfoundation/oneTBB)

## Quick Start

1. Clone this repository.
2. Configure with CMake to generate build files.
3. Build the solution in your IDE or via the generated build scripts.
4. Launch the editor to explore the sample scenes or integrate your own modules.

## Code Style

This project follows the standard library's coding conventions. The main branch is protected, so all contributions are made through pull requests.

For Visual Studio users, ReSharper C++ can apply the repository style automatically:

1. Go to `Extensions` → `ReSharper` → `Manage Options`.
2. Highlight `Solution "" Personal`.
3. Use the drop‑down `Copy to` and select `style_guide`.
4. Select all options and press `OK`.
