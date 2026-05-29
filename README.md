# GSEngine

<!--
HERO CLIP — drop the best 5–10 seconds of footage (clouds + atmosphere usually wins).
To get a hosted URL: open any GitHub issue/PR draft, drag-drop the .mp4 into the comment
box, copy the `https://github.com/user-attachments/assets/...` URL it spits out, paste
below. Don't actually submit the issue.
-->
<!-- <video src="PASTE_HERO_CLIP_URL_HERE" autoplay muted loop playsinline width="100%"></video> -->

GSEngine is a game engine written in modern C++ with C++26 reflection. It began as the foundation for a 3D shooter, but nothing in it is tied to that genre. The [wiki](https://github.com/DhirenTheHeadlights/GSEngine/wiki) is the best place to start.

Targets Windows only (clang + libc++ + Vulkan).

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

The bootstrap script handles the toolchain (clang-p2996, libc++, compiler-rt, vcpkg deps) but you need these on your system first:

| Tool                       | Version   |
|----------------------------|-----------|
| Python                     | 3.11+     |
| Git                        | 2.45+     |
| CMake                      | 4.0+      |
| Ninja                      | 1.11+     |
| Visual Studio Build Tools  | 2022/2026 (with **C++ workload + Windows 11 SDK**) |
| Vulkan SDK                 | 1.4+      |

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
2. `scripts/install_clang_p2996.py` — downloads the prebuilt clang-p2996 release to `~/.clang-p2996/<tag>` and sets `CLANG_P2996_ROOT`. Resolves the latest GitHub release automatically; pin with `--clang-tag clang-p2996-vN`.
3. `scripts/build_libcxx_p2996.py` — clones the libc++ source and builds it against the MSVC ABI, installing into the same clang dir.
4. `scripts/build_compiler_rt_p2996.py` — builds compiler-rt (ASAN runtime) into the same clang dir.

vcpkg's manifest mode (`vcpkg.json`) auto-installs dependencies when CMake configures.

Skip flags: `--skip-submodules`, `--skip-clang`, `--skip-libcxx`, `--skip-compiler-rt`. Force rebuilds with `--force-libcxx` / `--force-compiler-rt`. Re-running is idempotent — installed steps detect their artifacts and skip.

Run all of this from an **x64 Native Tools Command Prompt** (or **Developer PowerShell for VS**) so vcvars is loaded.

### Available presets

| Preset                                          | Use                                            |
|-------------------------------------------------|------------------------------------------------|
| `x64-clang-p2996-libcxx-Debug`                  | Day-to-day debug build                         |
| `x64-clang-p2996-libcxx-Release`                | Optimized release                              |
| `x64-clang-p2996-libcxx-RelWithDebInfo`         | Release with debug info (profiling)            |
| `x64-clang-p2996-libcxx-Debug-asan`             | Debug + AddressSanitizer (needs compiler-rt)   |
| `x64-clang-p2996-libcxx-{Release,Debug}-trace`  | `-ftime-trace` + `-print-stats` for compile profiling |
| `linux-gcc-trunk-Debug`                         | WSL/Linux GCC trunk build                      |

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

## clang-p2996 Toolchain

This project is built with a fork of Clang that includes preview support for [P2996 reflection](https://github.com/bloomberg/clang-p2996). `bootstrap.py` downloads the prebuilt artifact; `scripts/install_clang_p2996.py` is the same step run standalone.

Both scripts auto-resolve the [latest GSEngine release](https://github.com/DhirenTheHeadlights/GSEngine/releases) by default. A [weekly CI job](.github/workflows/build-clang-p2996.yml) builds from the upstream `p2996` branch tip and rolls the default release forward.

The toolchain also ships custom in-tree clang-tidy checks (`scripts/gse_tidy_checks/`) — no-anonymous-namespace, no-get-prefix, redundant-namespace-qualifier stripping, concept-in-template-param, etc. — that get stitched into `clang-tidy` at build time.

To build the clang toolchain locally run from an **x64 Native Tools Command Prompt**:

```
python scripts/build_clang_p2996.py --sha <commit>
```

Useful flags: `--tools-only` (rebuild clangd + clang-tidy + clang-format without touching the compiler), `--clangd-only`, `--link-jobs N` (cap parallel link memory).

## Code Style

This project follows the standard library's coding conventions. See [`docs/STYLEGUIDE.md`](docs/STYLEGUIDE.md) for the full guide. The main branch is protected, so all contributions are made through pull requests.

## License

[MIT](LICENSE) — do whatever you want.
