# GSEngine

GSEngine is a modular game engine written in modern C++20. It aims to provide a lightweight core that can be extended to fit a wide variety of projects. Originally created for a 3D shooter, the architecture is flexible enough to support many genres.

## Core Highlights

- **C++20 modules** for a clean and scalable codebase
- **Minimal dependencies** managed through vcpkg
- **Custom math library** with strongly typed vectors, matrices and quaternions
- **Physics engine** with broad and narrow phase collision detection for AABBs and OBBs
- **Hook‑based ECS** for queuing entities and components in a simple API
- **Rendering pipeline** targeting OpenGL with early Vulkan integration and support for custom shaders
- **2D and 3D rendering** including sprite, text and model helpers
- **Editor** built on ImGui that supports hot reloading of game code
- **Model and asset loading** from OBJ/MTL with automatic caching
- **Networking and audio** modules to lay the groundwork for multiplayer and sound
- **Example game scenes** demonstrating how to build on top of the engine

## Prerequisites

Install the following tools and ensure they are on your `PATH`:

- CMake 3.26 or higher
- Git 2.45 or higher
- Python 3.11.9 or higher
- A compiler with C++20 module support

The `setup.py` script can bootstrap third‑party packages through vcpkg on Windows.

## Quick Start

1. Clone this repository.
2. Run `build.bat` or configure with CMake to generate a `build` folder.
3. Build the solution in your IDE or via the generated build scripts.
4. Launch the editor to explore the sample scenes or integrate your own modules.

## Code Style

This project follows the standard library's coding conventions. The main branch is protected, so all contributions are made through pull requests.

For Visual Studio users, ReSharper C++ can apply the repository style automatically:

1. Go to `Extensions` → `ReSharper` → `Manage Options`.
2. Highlight `Solution "" Personal`.
3. Use the drop‑down `Copy to` and select `style_guide`.
4. Select all options and press `OK`.
