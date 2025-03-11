## GSEngine

GSEngine is a custom game engine built from the ground up with the goal of providing a simple, yet powerful, framework for game development. The engine is built with C++ 20 modules, and is designed to be easily extendable and modifiable. The engine is currently in development, and is not yet feature complete. The engine was originally for a 3D shooter game, but the infrastructure is designed to be easily adapted to other types of games.

## Features
- Built with C++ 20 module - requires a compiler and build system that supports this feature
- Requires minimal dependencies with plans of eliminating most in the future
- Custom 3D math library, with support for robust matrix, quaternion, and vector operations. Vectors can optionally be templated for a specific unit-type (SI units are default)
- Physics engine with broad and narrow phase collision detection, supporting axis-aligned and oriented bounding boxes
- Custom entity-component system with support for entity and component queuing
- All entities are controlled by integrating hooks into the engine's registry separating functionality between user-defined modules.
- Rendering pipeline, currently utilizing OpenGL, with support for custom shaders and lighting
- Basic shaders and models are included in the project
- Custom model loading from both .obj and .mtl files. Models are stored and re-used when possible
- Scene based engine with support for multiple scenes, completely extendable through inheritance
- Editor utilizing ImGui for runtime adjustments and hot reloading of game code

## Prerequisites (Follow online instructions for installation, ensuring all are added to system PATH variable)
- CMake v3.26 or higher
- Git 2.45 or higher
- Python 3.11.9 or higher
- Compiler with C++ 20 modules support

## Getting Started

To get started, clone the repository.
Run the `build.bat` file to automatically clone and build vcpkg. The required dependencies will be downloaded and compiled.

### Code Structure

This project follows the standard library's code style. The main branch is protected, and all changes must be made through pull requests.

### Prerequisites

Install ReSharper C++ for Visual Studio for style enforcement & many helpful tools

Integrating team style guide into your ReSharper C++:

1. Go to `Extensions` -> `ReSharper` -> `Manage Options`
2. Highlight `Solution "" Personal`
3. Hit the drop down `Copy to` and select `style_guide`
4. Select all of the options and hit `OK`
