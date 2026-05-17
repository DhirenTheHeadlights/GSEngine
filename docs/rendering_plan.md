# GSEngine Rendering — Consolidated Plan

Single source of truth for the rendering pipeline. Consolidates and replaces the previous forward+, RT lighting, and bindless renderer plan docs.

---

## Current pipeline (shipped)

```
┌───────────────────────────────────────────────────────────────┐
│ Per-frame setup                                               │
│  ├─ BLAS build/cache per unique mesh                          │
│  ├─ TLAS rebuild from instance transforms                     │
│  ├─ Material palette upload (per-unique-material flat_map)    │
│  └─ Light data upload (camera-space + world-space)            │
├───────────────────────────────────────────────────────────────┤
│ Depth prepass (mesh shaders)                                  │
│  ├─ Task shader: meshlet frustum + backface-cone cull         │
│  ├─ Mesh shader: emit meshlet triangles                       │
│  └─ Output: depth (swapchain depth_image)                     │
├───────────────────────────────────────────────────────────────┤
│ Light culling (compute)                                       │
│  ├─ 16×16 tile, samples depth                                 │
│  └─ Output: light_index_list, tile_light_table SSBOs          │
├───────────────────────────────────────────────────────────────┤
│ Forward shading (mesh shaders → frag)                         │
│  ├─ Cook-Torrance PBR (GGX + Smith + Fresnel-Schlick)         │
│  ├─ Tile light lookup                                         │
│  ├─ Inline RayQuery — shadows, AO, reflections                │
│  ├─ Bindless texture sampling                                 │
│  └─ Output: swapchain color (LDR, B8G8R8A8)                   │
└───────────────────────────────────────────────────────────────┘
```

### Feature status

| Area | Status | Notes |
|---|---|---|
| Mesh shaders (task + mesh) | ✅ Done | `draw_mesh_tasks_indirect` in static path; skinned uses `draw_indirect` |
| Meshlet bake (model compiler) | ✅ Done | `.gmdl` v4 / `.gsmdl` v2 |
| Forward+ light culling | ✅ Done | 16×16 tiles, GPU-only |
| Depth prepass | ✅ Done | Used for light culling + RT z-test |
| PBR / Cook-Torrance | ✅ Done | Material palette on GPU, embedded in `.gmdl` |
| Material palette (bindless-style) | ✅ Done | `StructuredBuffer<MaterialData>` |
| TLAS rebuild | ✅ Done | Per frame, instance custom_index = palette index |
| RT shadows | ✅ Done | Quality: Off / Hard / Low / Medium / High |
| RT AO | ✅ Done | Cosine-weighted hemisphere |
| RT reflections | ✅ Done | Hit-point material palette shading |
| Bindless textures | ✅ Done | `bindless_texture_set`, capacity 4096, retire queue |
| Persistent / push / bindless descriptor sets | ✅ Done | Three-way split |
| Auto pipeline-reflection resource tracking | ✅ Done | `bind_descriptors` populates `note_touched` |
| Render graph multi-queue | ✅ Done | Graphics + compute with timeline semaphores |
| Render graph topo sort + barrier emission | ✅ Done | Reads/writes data flow + explicit `after<T>` + `.in_chain<T>()` |
| Transient image/buffer pool | ✅ Done | Alias-aware memory, per-frame slots |
| Intra-frame alias barriers | ✅ Done | First-use barrier carries observed dst stages/access |
| Custom color/depth attachments | ✅ Done | Persistent or transient targets, per-pass inheritance format |
| Native capture (screenshot/video) | ✅ Done | Vulkan Video encode |
| HDR pipeline | ❌ Missing | Forward writes straight to LDR swapchain |
| Tone mapping | ❌ Missing | — |
| TAA + motion vectors | ❌ Missing | — |
| Bloom | ❌ Missing | — |
| FSR / temporal upscale | ❌ Missing | — |
| RT global illumination (DDGI) | ❌ Missing | — |
| Temporal denoising for RT | ❌ Missing | Currently relies on multi-ray averaging |
| Visibility buffer | ❌ Deferred | Phase 4 of original plan; optional |

---

## Priority order

Top to bottom = do first to last. Each item lists its main dependency.

1. **HDR forward path** — landing this unblocks every item below.
2. **Tone mapping pass** — requires (1).
3. **Bloom** — requires (1) and subresource tracking in the graph.
4. **Motion vector buffer (depth prepass MRT)** — requires (1); enables (5)–(7).
5. **TAA** — requires (1), (4), cross-frame history.
6. **FSR / temporal upscale** — requires (4), (5)'s history infrastructure.
7. **RT temporal denoiser** — requires (4), (5)'s history. Big quality win.
8. **DDGI** — independent, depends only on BLAS/TLAS + material palette (both shipped).
9. **Bindless buffers (instance/material SSBOs in bindless set)** — pure cleanup; current sets work.
10. **Async transfer queue** — defer until profiler shows graphics queue starvation.
11. **Visibility buffer** — deferred indefinitely; revisit if Nanite-style LOD becomes a goal.

---

## Detailed sections

### 1. HDR forward path

**Goal:** forward shading writes to an offscreen HDR target instead of the swapchain. Tone mapping later resolves it to swapchain LDR.

**Scope:**
- Declare an HDR color image (e.g. `r16g16b16a16_sfloat`) via `gpu::transient_image` with `used_by = { forward_pass, tonemap_pass, … }`.
- Forward pass's `.color({ … .transient_target = hdr })` instead of swapchain.
- Tonemap pass reads `hdr` via `rec.resolve` + samples; writes swapchain.

**Status:** the API for this is in. ForwardRenderer still writes swapchain directly. Migration is one-renderer scope; no graph changes needed.

**Files:**
- `Engine/Engine/Source/Graphics/Renderers/ForwardRenderer.cpp` — change `.color(clear_color(...))` to use the HDR handle.
- New: `Engine/Engine/Source/Graphics/Renderers/TonemapRenderer.cppm/.cpp`.
- New shader: `Compute/tonemap.slang` (or fragment-based; either works).

### 2. Tone mapping

ACES or AgX. Compute-shader-based is simplest (reads HDR storage image, writes swapchain via storage image). Fragment-based is fine too — full-screen triangle. Either way, the pass reads `hdr` and writes `swapchain`.

**Optional:** auto-exposure via compute histogram. Adds one compute pass before tonemap.

### 3. Bloom

Standard downsample / blur / upsample chain on the HDR target.

**Blocker:** the render graph today emits barriers with `base_mip_level=0, level_count=1`. A pyramid pass that writes different mips of one image in sequence will false-conflict (graph sees same image pointer, same level=1 range, treats both as the whole image).

**Two options:**
- **Subresource tracking in the graph.** Extend `resource_ref` / `note_touched` to carry a mip-range + layer-range. Per-pass barrier emission emits the actual range. ~150-line graph diff.
- **One image per mip level.** Cheap workaround — each mip is a separate transient. The pool will alias them since their lifetimes overlap only at the boundary. Slightly more bookkeeping in the bloom renderer; no graph diff.

Recommended: workaround for first bloom landing; revisit subresource tracking when it bites a second feature.

### 4. Motion vector buffer (MRT in depth prepass)

**Goal:** depth prepass outputs depth + velocity in one pass.

**Blocker:** the render graph today supports one color attachment per pass. `color_output_info` and the inheritance arrays are sized for one.

**Scope:**
- Extend `color_output_info` to be a small vector (cap at e.g. 4).
- Extend `color_attachment` user struct similarly, or take a span.
- `inheritance.color_formats` grows from `array<vk::Format, 1>` to `vector<vk::Format>` per pass.
- `begin_rendering` builds N attachment infos, one per color target.

~80-line graph diff. Worth doing before TAA needs it.

### 5. TAA + cross-frame history

Standard temporal accumulation. Reads previous-frame post-TAA color, current-frame HDR, motion vectors. Outputs accumulated HDR (becomes next frame's history).

**Cross-frame history pattern:** uses `per_frame_resource<gpu::image>` — explicitly NOT a transient. Frame N reads slot[N⊕1] while writing slot[N]. The graph already handles per-frame buffers identically; images are the same shape.

Jittered projection (Halton-23) goes in the camera system.

### 6. FSR / temporal upscale

Same temporal history pattern as (5). Render forward at lower resolution, reconstruct at output resolution. Mostly a port of the FSR reference shader; no engine-level work beyond what (4) and (5) provide.

### 7. RT temporal denoiser

The cheap RT presets are noisy (1 ray/pixel for shadows at "Hard", 1 ray/pixel for AO at "Low"). Temporal denoising reprojects the previous frame's RT result by motion vectors and blends.

**Scope per RT effect** (shadows / AO / reflections):
- Split inline RT out of the forward fragment shader into a dedicated compute pass that writes a per-effect output texture.
- Denoiser pass: read current + previous (history), reproject, blend, clamp.
- Forward fragment reads the denoised result instead of doing inline RT.

This is a real refactor of the RT path. The inline-RT-in-forward-fragment model from `rt_lighting.md` is elegant for quality presets but doesn't denoise well. Worth doing once TAA infrastructure (motion vectors + history) is in place — they share most of the plumbing.

### 8. DDGI

Probe-based diffuse GI. Independent of everything else above; needs BLAS/TLAS (shipped) and material palette (shipped).

Carry over from `rt_lighting.md` Phase 3 unchanged:

- Uniform 3D probe grid, configurable spacing (1–4 m).
- Per-probe 8×8 octahedral irradiance (`RGBA16F`) + 16×16 distance (`RG16F`).
- Storage: 2D texture atlases.
- Compute pass `probe_update` traces N rays/probe per frame, encodes to atlas.
- Fragment samples 8 nearest probes, blends into ambient.

Quality presets:

| Preset | Rays/probe | Probes/frame | Spacing |
|---|---|---|---|
| Low | 64 | 1/8 | 4 m |
| Medium | 128 | 1/4 | 2 m |
| High | 256 | 1/2 | 1 m |

New files:
- `GIProbeRenderer.cppm` — probe grid + atlas management + compute dispatch.
- `gi_probe_update.slang` — probe ray trace + irradiance encoding.
- `gi_probe_sample.slang` — sampling helpers for forward shader.

### 9. Bindless buffer tail

Layer 1 + 2 of `bindless_renderer_plan.md` are landed (`bindless_texture_set`, capacity 4096, retire queue, broad renderer adoption).

Remaining:
- **Buffer-flavoured bindless service** — analogue of `bindless_texture_set` for `StructuredBuffer<T>` slots. Used by per-material data, per-instance data.
- **Migrate `material_palette_buffers`** from per-frame UBO to a bindless `StructuredBuffer<material_data>` slot.
- **Migrate instance data** similarly. Compute culling writes draw streams with packed `(instance_idx, material_idx)` directly; renderer becomes one `drawIndexedIndirect`.

Phase 4 of the bindless plan ("GPU-driven rendering — the actual payoff") is what this unlocks. Order: do (1)–(7) first since they unlock visible quality; bindless tail is a structural payoff that doesn't change pixels.

### 10. Render-graph hardening

Items that aren't blockers today but will be one day. Each is a self-contained graph change.

- **Subresource tracking** — needed once bloom or mip-chain SSAO lands. Detail: `resource_ref` gains `mip_base/count, layer_base/count`; `note_touched` accepts them; barrier emission emits the actual range. ~150 lines.
- **MRT** — needed for motion vectors. ~80 lines.
- **Async transfer queue** — currently graphics + compute. TLAS rebuild + vertex uploads share the graphics queue. Defer until measurement shows graphics-queue starvation.
- **Pass culling** — drop passes whose writes don't reach the swapchain (or any retained resource). Low priority.
- **Graph viz dump** — debugging aid; no game-facing impact.

---

## Cross-cutting

### Pass chains vs `after<T>` deps

The graph offers two ordering knobs:

- `co_await gpu::pass<X>(ctx).after<A, B>()` — pass X waits on passes of kind A and B.
- `co_await gpu::pass<X>(ctx).in_chain<Y>()` — passes sharing chain id Y execute in declaration order.

Use `after<T>` when X depends on data produced by A/B (which `note_touched` already covers most of the time anyway).
Use `in_chain` when the order matters but isn't data-derived — e.g. UI layering, ping-pong post-process chains.

### Transient resources

- **Within-frame intermediate**: `gpu::transient_image` / `gpu::transient_buffer`. Declared via `co_await`, lifetime hint via `used_by`. Pool aliases non-overlapping lifetimes.
- **Cross-frame history**: `per_frame_resource<gpu::image>` — NOT a transient. Created at startup, double-buffered, lives across the whole session.
- **Whole-session persistent**: plain `gpu::image` / `gpu::buffer` created at startup. For things like the depth_image, palettes, BLAS storage.

### Quality preset surface

Each RT effect already has a quality enum in `ForwardRenderer.cppm`, push-constant-piped to the shader. New effects (TAA quality, GI quality, bloom strength) should follow the same pattern — quality enum + `save::register_property` + push-constant field + `get_*_config()` shader helper.

---

## Out of scope (for now)

- **Visibility buffer** (Phase 4 of original migration). Deferred; revisit if/when Nanite-style virtual geometry becomes a goal.
- **Work graphs / cooperative matrices / Gaussian splatting** — listed in original Phase 6 as future-future. Not on the runway.
- **Audio capture, network streaming, GIF support** — out of `native_capture.md` non-goals.

---

## Suggested PR slicing

The remaining work, broken into reviewable PRs:

1. **HDR forward + tone mapping** — one PR. Smallest unit that ships visible HDR.
2. **MRT in graph + velocity buffer in depth prepass** — one PR. Pure plumbing, no quality change yet.
3. **Bloom** — one PR using the "one image per mip" workaround; revisit subresource tracking later if it bites again.
4. **TAA** — one PR. History buffer, projection jitter, neighborhood clamp.
5. **FSR** — one PR after TAA, reuses history infrastructure.
6. **RT denoiser** — one PR per effect (shadows / AO / reflections). Each splits inline RT out of forward fragment + adds a denoise pass.
7. **DDGI** — one PR. Independent of post-process work, can run in parallel with anything above.
8. **Bindless buffers (material palette)** — one PR. Pure refactor, no visible change.
9. **Bindless buffers (instance data) + GPU-driven culling** — one PR. The actual draw-call fusion payoff.

Items 1–3 are the critical path for any further visual quality work. Items 4–7 deliver the AAA-look gap. Items 8–9 are perf wins that don't change pixels.
