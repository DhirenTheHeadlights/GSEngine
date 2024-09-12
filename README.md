## 3-D Shooter Project

This 3-D shooter will be built from the ground up with multiplayer in mind

## Getting Started

To get started, clone the respository. There are no dependencies required to run the project.

### Style Guide

The files in this project are divided into the following categories:

- `src` - Contains all the source code for the project
- `resources` - Contains all the resources for the project
- `include` - Contains all the header files for the project

These are the major categories of files in the `src` & `include` directory:

- `GameLayer` - Contains all the game logic
- `Platform` - Contains all the platform specific code (e.g. OS, Windowing, Input, etx.)
- `Engine` - Contains all the engine specific code

To steamline developing new objects, you will need to add a `Code Snippet` to Visual Studio. To do this, follow these steps:

1. Open Visual Studio
2. Go to `Tools` -> `Code Snippets Manager`
3. Click `Import`
4. Navigate to the `resources/Code Snippets` folder in the repository
5. Select the file and click `Open`
6. Click `Finish`

To use the code snippet, type `gameobject` and press `Tab` twice. This will generate a new GameObject class for you to use.
Visual studio will automatically highlight the class name for you to change. All you need to do is type the new class name and press `Enter`.

These are two big things to avoid when writing code for this project:

- Avoid inheritance when possible
- Avoid using pointers when possible

### Prerequisites

Install ReSharper C++ for Visual Studio for style enforcement & many helpful tools