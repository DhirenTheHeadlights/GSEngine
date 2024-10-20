## 3-D Shooter Project

This is a dual development project for a simulation engine and a 3-D shooter game as a demo. The game will feature multiplayer and single player modes.

## Getting Started

To get started, clone the respository. There are no dependencies required to run the project.
Run the `build.bat` file to build the project. This will generate a `build` folder with the project files.

### Code Structure

The files in this project are divided into the following categories:

- `Engine` - Contains all the engine specific code
- `Game` - Contains all the game logic

Engine statically links all required third party libraries. Game contains all the game logic and is also statically linked to the engine.

The `Build` folder will contain the Visual Studio solution file. Open this file to start working on the project using VS, or, if you prefer, open and run in `Folder View` mode to run using just CMake & Ninja.

When you end up adding a file to the project, VS will prompt you to update the `CMakeLists.txt` file. DO NOT UPDATE THIS FILE. Select `Cancel` when prompted.

To steamline developing new objects, you will need to add a `Code Snippet` to Visual Studio. To do this, follow these steps:

1. Open Visual Studio
2. Go to `Tools` -> `Code Snippets Manager`
3. Click `Import`
4. Navigate to the `resources/Code Snippets` folder in the repository
5. Select the file and click `Open`
6. Click `Finish`

To use the code snippet, type `gameobject` and press `Tab` twice. This will generate a new GameObject class for you to use.
Visual studio will automatically highlight the class name for you to change. All you need to do is type the new class name and press `Enter`.

When nesting namespaces, do not indent the nested namespace.

### Prerequisites

Install ReSharper C++ for Visual Studio for style enforcement & many helpful tools

Integrating team style guide into your ReSharper C++:

1. Go to `Extensions` -> `ReSharper` -> `Manage Options`
2. Highlight `Solution "Goon Squad" Team Shared`
3. Hit the drop down `Copy to` and select `This computer` or `Solution "Goon Squad" Team Shared`
4. Select all of the options and hit `OK`