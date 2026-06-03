# GSEngine

<!--
HERO CLIP — drop the best 5–10 seconds of footage (clouds + atmosphere usually wins).
To get a hosted URL: open any GitHub issue/PR draft, drag-drop the .mp4 into the comment
box, copy the `https://github.com/user-attachments/assets/...` URL it spits out, paste
below. Don't actually submit the issue.
-->
<!-- <video src="PASTE_HERO_CLIP_URL_HERE" autoplay muted loop playsinline width="100%"></video> -->

GSEngine is a game engine written in modern C++ with C++26 reflection. It began as the foundation for a 3D shooter, but nothing in it is tied to that genre. The [wiki](https://github.com/DhirenTheHeadlights/GSEngine/wiki) is the best place to start.

Targets Windows (Vulkan).

## Features

**Rendering**
- Bindless Forward+ light culling
- Mesh-shader meshlet pipeline with GPU culling
- Hardware ray tracing — RT shadows and GI probes
- HDR pipeline with AgX tonemapping and bloom
- Physically-based atmosphere and sky
- Volumetric clouds
- Temporal anti-aliasing
- MSDF/MTSDF text rendering
- Hardware-accelerated H.265/AV1 capture

**Physics**
- GPU-driven rigid-body pipeline — broad phase, narrow phase, solve, and transform writeback, all dispatched on-device
- VBD (Vertex Block Descent) solver with graph-colored parallel constraint solving
- Spatial-hash broad phase and SAT narrow-phase collision
- Joints from hinge/ball/universal to activation-driven muscles
- Physics-driven humanoid locomotion
- Physics-aware camera

**Engine**
- C++26 reflection drives ECS registration, Slang shader/type emission, serialization, asset formats, networking, and a hot-reloadable settings UI
- Compile-time dimensional analysis — `length / time` is a `velocity`, `length + time` is a compile error
- Coroutine async on a lock-free work-stealing scheduler
- Async render graph with automatic barrier insertion
- Backend-agnostic RHI (Vulkan today, DX12 in progress)
- Built entirely from C++ modules
- Custom in-tree clang-tidy checks

### Atmosphere & volumetric clouds

<!-- <video src="PASTE_CLIP_URL_HERE" autoplay muted loop playsinline width="100%"></video> -->

Dawn-to-dusk sun arc with aerial perspective and Henyey-Greenstein cloud scattering. Sun angles, planet radii, Rayleigh/Mie/ozone coefficients, and cloud coverage/density are all live-tweakable.

### HDR, bloom, AgX tonemap

<!-- <video src="PASTE_CLIP_URL_HERE" autoplay muted loop playsinline width="100%"></video> -->

R16G16B16A16 scene target end-to-end; AgX with input matrix → log-EV remap → contrast polynomial → punchy-look saturation → output matrix. Bloom downsamples 7 mips with Karis average, then tent-upsamples back into mip 0.

### Physics-driven locomotion

<!-- <video src="PASTE_CLIP_URL_HERE" autoplay muted loop playsinline width="100%"></video> -->

No skeletal animation playback — the character's pose comes out of a balance controller driving a pelvis `motor_component` from the support polygon and capture point. Shove it or stand it on a slope and it keeps its balance.

### VBD physics solver

<!-- <video src="PASTE_CLIP_URL_HERE" autoplay muted loop playsinline width="100%"></video> -->

GPU Vertex Block Descent with reflection-emitted shader constants and a custom narrow phase. Joints, contacts, and ragdolls converge in a fraction of the iterations of Gauss-Seidel.

### Forward+ light culling

<!-- <video src="PASTE_CLIP_URL_HERE" autoplay muted loop playsinline width="100%"></video> -->

Bindless point/spot lights tile-culled on the compute queue; renderer dispatches via reflection-driven binding packs (`recording_context::dispatch<Entry>(pc, args, groups)`).

## System Prerequisites

| Tool                       | Version   |
|----------------------------|-----------|
| Git                        | 2.45+     |
| CMake                      | 3.28+     |
| Ninja                      | 1.11+     |
| MinGW-w64 GCC              | 16.1+ (UCRT)  |
| Vulkan SDK                 | 1.4+      |

Two ways to get the toolchain:

**WinLibs prebuilt (fast, but currently stuck on 16.1.0 which has a known module-loading bug for this codebase — see [GCC PR 122785](https://www.mail-archive.com/gcc-bugs@gcc.gnu.org/msg885994.html)):**
Grab the UCRT + POSIX threads, GCC 16.1.0+ archive from [WinLibs](https://winlibs.com/). Unzip somewhere stable and point `MINGW_ROOT` at it:
```powershell
$env:MINGW_ROOT = "C:\mingw64"
```

**Trunk build (slow but has the fix):**
```powershell
python scripts/build_gcc_trunk.py --persist
```
Downloads MSYS2 into `.msys2/`, clones GCC trunk, builds, installs to `~/.gcc-trunk/<sha>/`, and `setx`'s `MINGW_ROOT`. First run takes 2-4 hours and ~10 GB of disk; subsequent rebuilds reuse `.msys2/` and the GCC source clone. Pin a specific commit with `--sha <hash>`.

Persist `MINGW_ROOT` so CMake picks it up across shells:
```powershell
[Environment]::SetEnvironmentVariable("MINGW_ROOT", "C:\path\to\gcc", "User")
```

## Quick Start

```
git clone <repo>
git submodule update --init --recursive
cmake --preset x64-mingw-gcc-Release
cmake --build --preset x64-mingw-gcc-Release
```

vcpkg's manifest mode (`vcpkg.json`) auto-installs dependencies when CMake configures. First configure rebuilds the dep tree under the MinGW triplet — expect 30-60 minutes.

### Available presets

| Preset                          | Use                                       |
|---------------------------------|-------------------------------------------|
| `x64-mingw-gcc-Debug`           | Day-to-day debug build                    |
| `x64-mingw-gcc-Release`         | Optimized release                         |
| `x64-mingw-gcc-RelWithDebInfo`  | Release with debug info (profiling)       |
| `x64-mingw-gcc-Debug-asan`      | Debug + AddressSanitizer                  |

## Dependencies (vcpkg manifest)

Listed in `vcpkg.json` and auto-installed at configure time:

- [GLFW3](https://www.glfw.org/) — windowing/input
- [libpng](http://www.libpng.org/pub/png/libpng.html) — PNG decode
- [libjpeg-turbo](https://libjpeg-turbo.org/) — JPEG decode
- [miniaudio](https://github.com/mackron/miniaudio) — audio
- [shader-slang](https://github.com/shader-slang/slang) — shader compiler
- [concurrentqueue](https://github.com/cameron314/concurrentqueue) — lock-free MPMC queue used by the task scheduler
- [freetype](https://github.com/freetype/freetype) — font rasterization
- [msdfgen](https://github.com/Chlumsky/msdfgen) — multi-channel SDF font generation
- [nsight-aftermath](https://developer.nvidia.com/nsight-aftermath) — GPU crash dumps (NVIDIA)

## In-tree clang-tidy Checks

`scripts/gse_tidy_checks/` ships custom checks — no-anonymous-namespace, no-get-prefix, redundant-namespace-qualifier stripping, concept-in-template-param, etc. — built against clang's libtooling and stitched into `clang-tidy`.

## Code Style

This project follows the standard library's coding conventions. See [`docs/STYLEGUIDE.md`](docs/STYLEGUIDE.md) for the full guide. The main branch is protected, so all contributions are made through pull requests.

## License

[MIT](LICENSE) — do whatever you want.
