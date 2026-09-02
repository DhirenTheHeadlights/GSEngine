# GSEngine

<!--
To fill a clip slot below: open any GitHub issue/PR draft, drag-drop the .mp4 into the
comment box, copy the `https://github.com/user-attachments/assets/...` URL it spits out,
and paste it as the video src. Don't actually submit the issue.
-->

GSEngine is a game engine written in modern C++ with C++26 reflection. It began as the foundation for a 3D shooter, but nothing in it is tied to that genre.

**Windows only.** x64 Windows is the only supported platform — there is no Linux or macOS build, and none is planned. Both the Vulkan and DX12 backends are fully supported; Vulkan is the default and falls back to DX12 if it is unavailable. The build expects the MinGW-w64 GCC toolchain that `bootstrap.py` installs; MSVC is not supported.

## Features

**Rendering**

* Bindless Forward+ light culling
* Mesh-shader meshlet pipeline with GPU culling
* Hardware ray tracing — RT shadows and GI probes
* HDR pipeline with AgX tonemapping and bloom
* Physically-based atmosphere and sky, with a procedural starfield
* Volumetric clouds driven by an evolving weather field, casting shadows onto scene geometry
* Procedural terrain — a snow-capped mountain ring generated at load from ridged noise
* Temporal anti-aliasing
* MSDF/MTSDF text rendering
* Hardware-accelerated H.265/AV1 capture

**Physics**

* GPU-driven rigid-body pipeline — broad phase, narrow phase, solve, and transform writeback, all dispatched on-device
* VBD (Vertex Block Descent) solver with graph-colored parallel constraint solving
* Spatial-hash broad phase and SAT narrow-phase collision
* Joints from hinge/ball/universal to activation-driven muscles
* Physics-driven humanoid locomotion
* Physics-aware camera

**Engine**

* C++26 reflection drives ECS registration, Slang shader/type emission, serialization, asset formats, networking, and a hot-reloadable settings UI
* Compile-time dimensional analysis — `length / time` is a `velocity`, `length + time` is a compile error
* Coroutine async on a lock-free work-stealing scheduler
* Async render graph with automatic barrier insertion
* Backend-agnostic RHI with two complete backends — Vulkan and DX12, selectable at startup
* Built entirely from C++ modules

### Atmosphere, volumetric clouds & terrain

<video src="https://github.com/user-attachments/assets/b6eecaf4-c325-48f9-9c29-5455d68b24b6" autoplay muted loop playsinline width="100%"></video>

Everything in the clip above is generated at load and driven by live settings — no baked lighting, no authored heightmap, no skybox.

The sky is a physically-based atmosphere with aerial perspective, Henyey-Greenstein cloud scattering, and a procedural starfield that fades in as the sun drops. The cloud layer is grouped into discrete masses by a spatially varying weather field that evolves over time, and those masses cast real shadows onto the terrain through a sunward-marched shadow map. The mountains are a ridged-noise ring built at load, with snow placed by elevation and slope so it settles on crests and leaves the steep faces bare.

Sun angles, planet radii, Rayleigh/Mie/ozone coefficients, cloud coverage, density, the weather cycle, and every terrain parameter are hot-reloadable.

### HDR, bloom, AgX tonemap

<!-- <video src="PASTE_CLIP_URL_HERE" autoplay muted loop playsinline width="100%"></video> -->

R16G16B16A16 scene target end-to-end; AgX with input matrix → log-EV remap → contrast polynomial → punchy-look saturation → output matrix. Bloom downsamples 7 mips with Karis average, then tent-upsamples back into mip 0.

### VBD physics solver

<video src="https://github.com/user-attachments/assets/b0a74469-7a3e-4bfb-8017-1e4ae1043033" autoplay muted loop playsinline width="100%"></video>

GPU Vertex Block Descent with reflection-emitted shader constants and a custom narrow phase. Joints, contacts, and ragdolls converge in a fraction of the iterations of Gauss-Seidel.

Above: four counter-rotating drums tumbling 13,824 dynamic bodies, solved entirely on the GPU — broad phase, narrow phase, solve, and transform writeback all dispatched on-device, with no per-body readback to the CPU.

### Forward+ light culling

<video src="https://github.com/user-attachments/assets/e42468da-6613-4b96-8df1-b5c5988e29e9" autoplay muted loop playsinline width="100%"></video>

Above: 364 physically scaled point lights drifting through the light hall at night, tile-culled per 16 px tile with ray-traced shadows, probe GI accumulated across frames, and histogram auto exposure metering the lamps.

Bindless point/spot lights tile-culled on the compute queue; renderer dispatches via reflection-driven binding packs (`recording_context::dispatch<Entry>(pc, args, groups)`).

## System Prerequisites

|Tool|Version|
|-|-|
|Git|2.45+|
|CMake|3.28+|
|Python|3.11+|
|Vulkan SDK|1.4+|

GCC and Ninja are not on that list — `bootstrap.py` installs both.

## Quick Start

```
git clone <repo>
cd GSEngine
python bootstrap.py
cmake --preset x64-mingw-gcc-Release
cmake --build --preset x64-mingw-gcc-Release
```

`bootstrap.py` does everything the build needs:

* initializes submodules recursively
* downloads the latest `gcc-trunk-v*` release into `~/.gcc-trunk/<tag>/` and points the `~/.gcc-trunk/current` junction at it
* installs Ninja into `~/.gcc-trunk/ninja/`
* builds the cppreference hover index used by the editor

The CMake presets resolve the compiler and Ninja through `~/.gcc-trunk/current`, so no `MINGW_ROOT` or `PATH` edit is required. Re-run `python bootstrap.py` to pick up a newer toolchain release; `--force` reinstalls, `--tag gcc-trunk-vN` pins a specific one, and `--skip-gcc`/`--skip-ninja`/`--skip-cppref` narrow the run.

A toolchain bump invalidates every previously built module BMI, so reconfigure from a clean build directory after one.

vcpkg's manifest mode (`vcpkg.json`) auto-installs dependencies when CMake configures. First configure rebuilds the dep tree under the MinGW triplet — expect 30-60 minutes.

### Available presets

|Preset|Use|
|-|-|
|`x64-mingw-gcc-Debug`|Day-to-day debug build|
|`x64-mingw-gcc-Release`|Optimized release|
|`x64-mingw-gcc-RelWithDebInfo`|Release with debug info (profiling)|
|`x64-mingw-gcc-Debug-asan`|Debug + AddressSanitizer|

## Dependencies (vcpkg manifest)

Listed in `vcpkg.json` and auto-installed at configure time:

* [GLFW3](https://www.glfw.org/) — windowing/input
* [libpng](http://www.libpng.org/pub/png/libpng.html) — PNG decode
* [libjpeg-turbo](https://libjpeg-turbo.org/) — JPEG decode
* [miniaudio](https://github.com/mackron/miniaudio) — audio
* [shader-slang](https://github.com/shader-slang/slang) — shader compiler
* [concurrentqueue](https://github.com/cameron314/concurrentqueue) — lock-free MPMC queue used by the task scheduler
* [freetype](https://github.com/freetype/freetype) — font rasterization
* [msdfgen](https://github.com/Chlumsky/msdfgen) — multi-channel SDF font generation
* [nsight-aftermath](https://developer.nvidia.com/nsight-aftermath) — GPU crash dumps (NVIDIA)

## Code Style

This project follows the standard library's coding conventions. See [`docs/STYLEGUIDE.md`](docs/STYLEGUIDE.md) for the full guide. The main branch is protected, so all contributions are made through pull requests.

## License

[MIT](LICENSE) — do whatever you want.

