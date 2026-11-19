# GSEngine

<!--
To fill a clip slot below: open an issue on a PUBLIC repo, drag-drop the .mp4 into the
comment box, SUBMIT the comment, then copy the `https://github.com/user-attachments/assets/...`
URL out of the posted comment and paste it as the video src. You can close the issue
straight away; the asset stays live. An attachment in a comment you never submit is
scoped to your own account, so the clip 404s for logged-out visitors while still playing
for you. Keep clips under 10 MB and encode H.264 (yuv420p, +faststart) for playback
everywhere; raw engine AV1 output is both too large and unsupported on Safari.
-->

GSEngine is a game engine, and the editor it is built in, written from scratch in C++26. Reflection, modules and coroutines are the foundation rather than an add-on, and most of the simulation runs on the GPU.

[<video src="" autoplay muted loop playsinline width="100%"></video>](https://github.com/user-attachments/assets/ea9750f8-6a56-477f-a37a-fd9d70db80dd)

Nothing in this clip is authored. There is no baked lighting, no heightmap and no skybox. The sky is a physically based atmosphere, the clouds are a raymarched volume grouped into masses by a weather field that evolves over time, and those masses shadow the terrain below them. The mountains are built from ridged noise at load, with snow placed by elevation and slope. Every parameter behind it can be changed while the engine runs.

## What it is built on

### Reflection does the wiring

A type is described once, in C++, and the engine derives the rest from it. This is a complete, networked physics component:

```cpp
struct motor_component {
    [[= networked]] vec3<velocity> velocity_drive_target;
    [[= networked]] bool horizontal_only = true;
    [[= networked]] bool requires_ground_contact = true;
    [[= networked]] force max_force = newtons(1000.f);
    [[= networked]] float compliance = 0.5f;
};
```

There is no registration call and no schema file beside it. The same approach covers the rest of the engine. Systems, their schedule and the data they share are declared with annotations. Structs shared with shaders are emitted as Slang from their C++ definition, so the two sides cannot drift. A settings struct whose fields carry a description is a settings panel and a section of the config file, and any field can be overridden from the command line. Tests and scenarios are plain functions found by sweeping a namespace.

### Units are types

The component above stores a `vec3<velocity>` and a `force`, not floats. Dimensional analysis happens at compile time, across the whole engine:

```cpp
const velocity v = meters(10.f) / seconds(2.f);
const area a = meters(3.f) * meters(4.f);
const auto wrong = meters(3.f) + seconds(1.f);  // does not compile
```

Quantities stay typed through the physics solver and are only reduced to raw floats at the GPU boundary.

### One renderer, three backends

Render passes are written once, against a render graph that tracks resource use and inserts barriers, and a device interface with no API types in it. The backend is chosen at startup.

|Backend|State|
|-|-|
|Vulkan|The default|
|DirectX 12|At parity with Vulkan, and the automatic fallback when Vulkan is unavailable|
|WebGPU|The newest, through wgpu-native. Not at parity: passes that need features WebGPU lacks are switched off|

Vulkan and DX12 are held together by measurement. A parity gate replays physics scenarios on both and requires the GPU solver to produce the same world-state hash on every run, and to stay within tolerance of the CPU solver.

## Physics on the GPU

[<video src="" autoplay muted loop playsinline width="100%"></video>](https://github.com/user-attachments/assets/4ce56270-eaab-4d76-990e-6f52bfb4a710)

Four counter-rotating drums tumbling 13,824 dynamic bodies. Broad phase, narrow phase, solve and transform writeback all run on the device, with no per-body readback to the CPU.

The solver is Vertex Block Descent, with a CPU implementation of the same solve kept alongside as the reference. It has grown past rigid bodies. Joints are driven by Hill-type muscles with compliant tendons, a humanoid walks under a locomotion policy that is trained on the GPU inside the engine, and grass is simulated as deformable blades that push back on whatever moves through them.

## Lighting

[<video src="" autoplay muted loop playsinline width="100%"></video>](https://github.com/user-attachments/assets/2a7f8db8-5f31-4b46-a73b-e875408d9608)

364 physically scaled point lights drifting through a hall at night. Lights are culled per 16 px tile on the compute queue, shadows are ray traced, global illumination comes from probes accumulated across frames, and auto exposure meters the scene from a histogram.

## The editor

The editor is an application of the engine. It is drawn by the same renderer and the same GUI that a game uses, and it is what `bootstrap.py` builds.

It is a code editor first. Highlighting is semantic, driven by a GCC plugin that reads the same compile the build does, with completion, hover documentation, diagnostics and symbol search on top. Around that sit a build runner, a terminal, source control, and the game itself running in a docked viewport. Profiler, allocation and system-graph panels read the running game directly.

It also hosts AI agents as a first-class panel. A chat works through scoping, applying and review phases, builds and launches the game through the editor, and reads logs, traces and panels through the same tools a person would.

<!-- <video src="PASTE_CLIP_URL_HERE" autoplay muted loop playsinline width="100%"></video> -->

## Testing and shipping

Behaviour is checked by scenarios. A scenario is a short script that settles a world, drives it through a fixed sequence on a fixed-step clock, and exits with frame-time percentiles and a hash of the world state. The same scenario is a benchmark, a regression test and a determinism check. Unit tests and the headless scenarios run in CI on every push. [`docs/TESTING.md`](docs/TESTING.md) records what is verified and what is not.

A game built on the engine ships through tools in this repository. A packer stages the game image, an installer installs it, and a launcher updates it against a release feed, verifying a SHA-256 before it extracts anything. The release workflow drives all three.

## Status

GSEngine runs on x64 Windows and builds with one compiler, GCC trunk, because that is where C++26 reflection is implemented. MSVC and Clang are not supported.

A browser build is in progress and experimental. The engine compiles to wasm32 with a patched GCC and runs headless scenarios there. [`docs/wasm_pipeline_plan.md`](docs/wasm_pipeline_plan.md) records where it stands.

## Building

|Tool|Version|
|-|-|
|Git|2.45+|
|CMake|4.2+|
|Python|3.11+|
|Vulkan SDK|1.4+|

```
git clone <repo>
cd GSEngine
python bootstrap.py
```

`bootstrap.py` initializes submodules, installs the pinned GCC trunk toolchain and Ninja under `~/.gcc-trunk`, then configures and builds the editor into `.generated/out/build/x64-mingw-gcc-Release/Editor/Editor.exe`. Nothing needs to be added to `PATH`. The first configure builds the vcpkg dependencies listed in `vcpkg.json` and takes 30 to 60 minutes.

|Preset|Use|
|-|-|
|`x64-mingw-gcc-Debug`|Day-to-day debug build|
|`x64-mingw-gcc-Release`|Optimized release, the default|
|`x64-mingw-gcc-RelWithDebInfo`|Release with debug info and contracts enabled|
|`x64-mingw-gcc-Debug-asan`|Debug with AddressSanitizer|
|`wasm32-wasi-gcc-Release`|The experimental browser build|

[`docs/TOOLCHAIN_SETUP.md`](docs/TOOLCHAIN_SETUP.md) covers the bootstrap options, the toolchain pin, and building the toolchain yourself.

## Contributing

The code follows the standard library's conventions, set out in [`docs/STYLEGUIDE.md`](docs/STYLEGUIDE.md). The main branch is protected, so changes arrive through pull requests.

## License

[MIT](LICENSE). Do whatever you want.
