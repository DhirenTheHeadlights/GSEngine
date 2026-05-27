# GSEngine Rendering — Consolidated Plan

Single source of truth for the rendering pipeline. Lifetime tracker of what ships, what's planned, what's out of scope.

For workstream-specific deep dives, see:
- [restir_plan.md](restir_plan.md) — phased ReSTIR (DI → GI → PT) rollout
- [native_capture.md](native_capture.md) — Vulkan Video screenshot/clip system (shipped, kept as design record)
- [extension_adoption_plan.md](extension_adoption_plan.md) — Vulkan extension audit
- [vulkan_extensions.md](vulkan_extensions.md) — target hardware capability dump

---

## Current pipeline (shipped)

```
┌───────────────────────────────────────────────────────────────┐
│ Per-frame setup                                               │
│  ├─ BLAS build/cache per unique mesh                          │
│  ├─ TLAS rebuild from instance transforms                     │
│  ├─ Material palette upload (geometry_collector-owned SSBO)   │
│  ├─ Light data upload                                         │
│  └─ Halton-23 jitter applied to projection (for TAA)          │
├───────────────────────────────────────────────────────────────┤
│ Depth + velocity prepass (mesh shaders, MRT)                  │
│  ├─ Task shader: meshlet frustum + backface-cone cull         │
│  ├─ Mesh shader: emit meshlet triangles + curr/prev clip_pos  │
│  └─ Output: depth (swapchain), velocity (r16g16_sfloat)       │
├───────────────────────────────────────────────────────────────┤
│ DDGI probe update (compute, RT)                               │
│  ├─ 16×6×16 probes, camera-anchored                           │
│  ├─ 64 rays/probe via inline RayQuery → material_palette      │
│  └─ Output: 8×8 octahedral irradiance atlas (RGBA16F)         │
├───────────────────────────────────────────────────────────────┤
│ Light culling (compute)                                       │
│  ├─ 16×16 tile, samples depth                                 │
│  └─ Output: light_index_list, tile_light_table SSBOs          │
├───────────────────────────────────────────────────────────────┤
│ Forward shading (mesh shaders → frag)                         │
│  ├─ Cook-Torrance PBR (GGX + Smith + Fresnel-Schlick)         │
│  ├─ Tile light lookup                                         │
│  ├─ Inline RayQuery — shadows, AO, reflections                │
│  ├─ DDGI atlas sample (trilinear + octahedral)                │
│  ├─ Bindless texture sampling                                 │
│  └─ Output: targets::hdr_color (RGBA16F)                      │
├───────────────────────────────────────────────────────────────┤
│ Atmosphere / clouds (composite into hdr_color via blend)      │
├───────────────────────────────────────────────────────────────┤
│ TAA (fullscreen fragment, MRT)                                │
│  ├─ Read: hdr_color + velocity + history[prev]                │
│  ├─ Neighborhood-clamped YCoCg blend, 2-frame warmup          │
│  └─ Output: history[curr] + targets::post_taa_color           │
├───────────────────────────────────────────────────────────────┤
│ Bloom (compute, mip chain on post_taa_color)                  │
├───────────────────────────────────────────────────────────────┤
│ Tonemap (fragment, AgX)                                       │
│  ├─ Reads: post_taa_color + bloom mips                        │
│  ├─ Optional debug: velocity-buffer HSV viz                   │
│  └─ Output: swapchain color (LDR, B8G8R8A8)                   │
├───────────────────────────────────────────────────────────────┤
│ UI / world-text overlays → swapchain                          │
└───────────────────────────────────────────────────────────────┘
```

### Feature status

| Area | Status | Notes |
|---|---|---|
| Mesh shaders (task + mesh) | ✅ Done | `draw_mesh_tasks_indirect` in static path; skinned uses `draw_indirect` |
| Meshlet bake (model compiler) | ✅ Done | `.gmdl` v4 / `.gsmdl` v2 |
| Forward+ light culling | ✅ Done | 16×16 tiles, GPU-only |
| Depth prepass | ✅ Done | MRT-extended for velocity output |
| **Motion vector buffer** | ✅ Done | Depth prepass writes velocity to `targets::velocity` |
| PBR / Cook-Torrance | ✅ Done | Material palette owned by `geometry_collector` |
| Material palette | ✅ Done | `StructuredBuffer<material_data>` shared between forward + GI |
| TLAS rebuild | ✅ Done | Per frame, instance custom_index = palette index |
| RT shadows | ✅ Done | Quality: Off / Hard / Low / Medium / High |
| RT AO | ✅ Done | Cosine-weighted hemisphere |
| RT reflections | ✅ Done | Hit-point material palette shading |
| Bindless textures | ✅ Done | `bindless_texture_set`, capacity 4096, retire queue |
| Persistent / push / bindless descriptor sets | ✅ Done | Three-way split |
| Auto pipeline-reflection resource tracking | ✅ Done | `bind_descriptors` populates `note_touched` |
| Render graph multi-queue | ✅ Done | Graphics + compute with timeline semaphores |
| Render graph topo sort + barrier emission | ✅ Done | Reads/writes data flow + `after<T>` + `.in_chain<T>()` |
| Transient image/buffer pool | ✅ Done | Alias-aware memory, per-frame slots |
| **MRT** (multi-render-target color attachments) | ✅ Done | Variadic `color_targets<F...>`, per-pass `color_outputs` vector |
| Custom color/depth attachments | ✅ Done | Persistent or transient targets, per-pass inheritance format |
| Native capture (screenshot/video) | ✅ Done | Vulkan Video encode |
| **HDR pipeline** | ✅ Done | Forward → `targets::hdr_color`, post-TAA → `targets::post_taa_color` |
| **Tone mapping** | ✅ Done | AgX, fragment-based |
| **TAA + Halton jitter + history** | ✅ Done | Per-frame jitter, ping-pong history, neighborhood clamp |
| **Bloom** | ✅ Done | Mip chain on `post_taa_color`, one image per mip workaround |
| **DDGI** | ✅ Done | 16×6×16 probes, 64 rays each, material-tinted hits |
| **Camera-system shared prev matrices** | ✅ Done | `prev_view_matrix`, `prev_projection_matrix`, `prev_jitter_ndc` |
| **Instance prev model matrix** | ✅ Done | Cached per (entity, model_index) in `geometry_collector` |
| **mixed_mat type discipline** | ✅ Done | Bracket op deleted; use `at`/`set`/`transform_point` |
| RT temporal denoiser | ❌ Missing | Cheap RT presets remain noisy without one |
| ReSTIR DI | ❌ Planned | See [restir_plan.md](restir_plan.md) Phase 1 |
| ReSTIR GI | ❌ Planned | See [restir_plan.md](restir_plan.md) Phase 2 |
| ReSTIR PT | ❌ Deferred | See [restir_plan.md](restir_plan.md) Phase 3 |
| FSR / temporal upscale | ❌ Missing | Reuses TAA's history infrastructure |
| Bindless buffers (instance/material SSBOs) | ❌ Missing | Pure refactor; would unlock GPU-driven culling |
| Subresource tracking in graph | ❌ Missing | Needed when bloom-style mip chains grow; "one image per mip" works around it for now |
| Async transfer queue | ❌ Missing | Defer until profiler shows graphics-queue starvation |
| DDGI: temporal accumulation | ❌ Missing | Atlas rewrites each frame instead of EMA-blending |
| DDGI: distance/visibility atlas | ❌ Missing | Light leaks through thin walls; Chebyshev test would prevent |
| DDGI: probe relocation | ❌ Missing | Probes can end up inside geometry |
| DDGI: real hit normal | ❌ Missing | Uses `-ray_dir` placeholder; fetch from meshlet vertex buffer |
| External denoiser integration (NRD / OIDN) | ❌ Missing | Prereq for ReSTIR; useful for current RT path too |
| Visibility buffer | ❌ Deferred | Revisit if Nanite-style virtual geometry becomes a goal |

---

## Priority order

Top to bottom = do first to last.

1. **External denoiser integration** (NRD or OIDN). Prereq for ReSTIR; also improves current RT shadow/AO/reflection quality immediately.
2. **DDGI hardening** — temporal accumulation + distance atlas + real hit normals. Lifts DDGI from "DDGI Lite" to shippable. ~250 LOC across the existing files.
3. **ReSTIR DI** — see [restir_plan.md](restir_plan.md). Subsumes the inline shadow loop and unlocks 16K+ light counts.
4. **RT temporal denoiser** (per effect) — only needed if ReSTIR doesn't subsume the relevant RT effect first. After ReSTIR DI ships, the shadow/AO inline paths become dead.
5. **ReSTIR GI** — see [restir_plan.md](restir_plan.md). Competes with DDGI; keep DDGI as low-quality preset.
6. **FSR / temporal upscale** — reuses TAA's history infrastructure. Cheap once TAA is in (which it is). Defer until there's a need to render below output resolution.
7. **Bindless buffer tail** — material palette + instance data into bindless slots. Enables single-`drawIndexedIndirect` GPU-driven culling. Pure structural; no visible change.
8. **Subresource tracking in graph** — only land when a second feature needs it (current bloom uses the "one image per mip" workaround).
9. **Async transfer queue** — defer until profiler demands.
10. **Visibility buffer** — deferred indefinitely.

---

## Detailed sections

### 1. External denoiser integration

**Goal:** plumb a production denoiser (NVIDIA Real-Time Denoisers / NRD, or Intel Open Image Denoise / OIDN) over the existing RT signals.

**Why now:** all of the planned ReSTIR work needs a denoiser pair to look good at 1 spp. Current RT shadow/AO is noisy on the cheap presets; a denoiser fixes both problems at once.

**Recommendation:** NRD. It's purpose-built for ReSTIR-class signals and ships presets (ReBLUR for diffuse/specular, ReLAX for high-variance, SIGMA for shadows). Vulkan-compatible static lib.

**Scope:**
- Inputs NRD wants: HDR per-effect signals (shadow visibility, AO term, reflection color), normal+roughness G-buffer, depth, motion vectors.
- We already produce motion vectors and depth.
- Need to split inline RT in `meshlet_geometry.slang` into separate compute passes that output per-effect buffers (one pass per signal: shadow, AO, reflections).
- Forward shader then samples the *denoised* per-effect buffers instead of doing the inline RT.

**Estimated work:** ~2–3 weeks. Library integration + 3 compute passes + forward refactor.

This is also the right time to delete the existing per-light RT shadow loop, since denoised shadows render with one ray total regardless of light count.

### 2. DDGI hardening

DDGI as shipped is "DDGI Lite" — same architecture as the NVIDIA RTXGI library but missing key features. From cheapest to most invasive:

- **Real hit normal in the compute shader** (~50 LOC). Currently uses `-ray_dir` as the hit normal, which is a stand-in. Real fix: in `gi_probe_update.slang`, use `q.CommittedTriangleBarycentrics()` + `q.CommittedPrimitiveIndex()` + the meshlet vertex buffer to compute the actual normal. Requires binding the vertex buffer in the GI compute (already SSBO-bound in forward; same source).
- **Temporal accumulation per probe texel** (~60 LOC). Single biggest visual win. Currently each frame's atlas write completely replaces the previous value. Change to EMA: `new_value = lerp(old_value, fresh_sample, 0.03)`. Requires sampling the *previous* atlas inside the compute shader (so atlas needs read+write storage_image usage, or sample previous via a sampler binding).
- **Distance / visibility atlas** (~150 LOC). Second per-probe atlas (16×16 RG16F: mean, mean²). On forward sample, perform a Chebyshev visibility test (mean ± k·stddev vs. surface distance) and weight contributions. Kills the light-leak-through-walls artifact. Doubles atlas memory (still small in absolute terms).
- **Probe classification** (~30 LOC). Skip probes whose rays are nearly all very short (probe inside geometry) or all very long (probe outside the scene). Saves compute.
- **Probe relocation** (~80 LOC). Offset probes that landed inside geometry along the average miss direction. Fixes the worst per-probe artifacts.

Order recommended: hit-normal → temporal accumulation → distance atlas. Stop there unless artifacts demand the last two.

### 3. ReSTIR DI / GI / PT

See [restir_plan.md](restir_plan.md). Three-phase rollout with explicit prerequisites, considerations, decision points.

Short summary:

- **Phase 1 (DI, ~800 LOC, 4–6 weeks):** replaces forward+ tile culling and inline shadow loop. Handles arbitrary light counts at constant cost.
- **Phase 2 (GI, ~1200 LOC, 8–10 weeks):** full-resolution indirect diffuse. Competes with DDGI; keep DDGI as low preset.
- **Phase 3 (PT, 6+ months):** path-traced reference quality in real time. Defer; revisit after 1 + 2 ship.

Prerequisite for any phase: denoiser integration (§1) + per-pixel reservoir storage infrastructure.

### 4. FSR / temporal upscale

Render forward at lower resolution, reconstruct at output resolution using the existing TAA history.

The infrastructure is in: jittered projection, motion vectors, ping-pong history. FSR is a port of the reference shader.

**Scope:**
- Render most of the pipeline at, e.g., 1280×720 (or 0.5–0.85 of swap-extent).
- TAA / FSR pass upsamples to native swap-extent with the same neighborhood-clamp + history blend.
- UI renders at native resolution.

**Estimated work:** ~2 weeks. Mostly a shader port from FidelityFX SDK.

Defer until there's a content reason to want sub-native render resolution. At current scene complexity it's not needed.

### 5. Bindless buffer tail

The texture half of bindless is shipped (`bindless_texture_set`, capacity 4096, retire queue). The buffer half isn't.

**Goal:** material palette and instance data become bindless `StructuredBuffer<T>` slots, accessed by 32-bit indices baked into draw streams. Removes the per-pass descriptor write for these resources.

**Payoff:** GPU-driven rendering. Compute culling writes a single draw stream with packed `(instance_idx, material_idx)` per draw; the renderer becomes one `drawIndexedIndirect` call regardless of mesh count.

**Scope:**
- Buffer-flavoured bindless service — analogue of `bindless_texture_set`, slot-based registration.
- Migrate `material_palette_buffers` (currently in geometry_collector) to a bindless slot.
- Migrate `instance_buffer` similarly.
- Compute culling shader writes one `draw_indexed_indirect_command` per visible mesh into a draw stream.
- Forward renderer becomes a single indirect draw call.

This is the original "Phase 4 of the bindless plan" — the actual payoff. Doesn't change pixels but cuts draw-call overhead substantially.

**Estimated work:** 3–4 weeks. Touches the descriptor system, geometry_collector, forward renderer.

### 6. Subresource tracking in graph

Today the graph emits barriers with `base_mip_level=0, level_count=1`. Bloom works around this by allocating one image per mip; chains of dependent mip writes would false-conflict otherwise.

**Trigger to actually do this:** when a second feature trips over the same limitation. Until then, the "one image per mip" workaround is fine.

**Scope (when needed):** `resource_ref` gains `mip_base/count, layer_base/count`. `note_touched` accepts them. Barrier emission emits the actual range. ~150-line graph diff.

### 7. Async transfer queue

Today TLAS rebuild and vertex uploads share the graphics queue. The graph supports the queue split (graphics + compute); adding a transfer queue is mechanical.

**Trigger:** profiler shows graphics-queue starvation during heavy upload frames. Until that's measurable, no point.

### 8. Visibility buffer

Deferred indefinitely. Revisit if/when Nanite-style virtual geometry becomes a goal. Mesh shaders + meshlet culling get us most of the way there for current scene complexity.

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
- **Cross-frame history**: `per_frame_resource<gpu::image>` — NOT a transient. Created at startup, double-buffered, lives across the whole session. Used by TAA history; future ReSTIR reservoirs follow the same shape.
- **Whole-session persistent**: plain `gpu::image` / `gpu::buffer` created at startup. For the depth_image, palettes, BLAS storage, DDGI atlas, TAA history images.

### Quality preset surface

Each RT effect has a quality enum in `ForwardRenderer.cppm` (and now `GiProbeRenderer.cppm`, `TaaRenderer.cppm`, `BloomRenderer.cppm`, `TonemapRenderer.cppm`), push-constant-piped to the shader. Settings are reflected via `[[= gse::settings::describe<...>]]` + optional `[[= gse::settings::range<...>]]` annotations. New effects should follow the same pattern — quality enum + `describe` annotation + push-constant field + per-quality config helper.

### Type-safe matrix discipline

`mixed_mat<ColSpec, RowSpec>` (the base of `view_matrix`, `projection_matrix`, etc.) has `operator[]` deleted. Element access goes through `at<C, R>()` (returns the unit-typed value) and `set<C, R>(val)` (takes a unit-typed value). Internal access in matrix methods uses `static_cast<const base&>(*this)[c][r]`. There's intentionally no public `raw()` escape hatch — if you need element-level math, do it on a `mat<T, N, N>` first and wrap in the typed type at the end (perspective() / orthographic() are the template).

### Unit types in push constants

Per project convention: unit types (`length`, `irradiance`, `position`, `vec3<position>`, etc.) are layout-compatible with their underlying float / float3. They can be used directly in `shaders::shader_struct` push constant types without conversion — the slang codegen emits them as float / float3 on the shader side. Never `static_cast` a unit type to float; just use it directly.

Examples in the codebase: atmosphere's `sky_raster_push_constants` uses `vec3<irradiance>`, `atmosphere_length`; gi_probe's `push_constants` uses `vec3<position>`, `length`, `irradiance`.

---

## Out of scope (for now)

- **Visibility buffer**. Revisit if/when Nanite-style virtual geometry becomes a goal.
- **Work graphs / cooperative matrices / Gaussian splatting**. Not on the runway.
- **Audio capture, network streaming, GIF support**. Per `native_capture.md` non-goals.
- **Custom shift mappings for ReSTIR variants** (e.g., ReSTIR for our SDF tracer or VBD-driven physics). PhD-level math; explicitly research scope.
- **Neural radiance caching with runtime training.** Tracked separately; 4–6+ month commitment, requires GPU intrinsics work and numerical-debugging stamina that AI agents are weak at.

---

## Suggested PR slicing (remaining work, in priority order)

1. **NRD or OIDN integration** — denoiser standalone. Plumb HDR + motion vectors + depth, output denoised per-effect buffers. Doesn't need ReSTIR to test; point it at existing inline RT output for verification. ~2–3 weeks.
2. **DDGI hardening** — real hit normals + temporal accumulation (one PR), then distance atlas (separate PR). ~1–2 weeks each.
3. **Forward shader RT refactor** — split inline shadow/AO/reflection into separate compute passes feeding the denoiser. ~1 week. Sets up Phase 1 of ReSTIR.
4. **ReSTIR DI initial + temporal** — barely works, proves the pipeline. ~2 weeks.
5. **ReSTIR DI spatial reuse + bias correction** — DI complete. ~1–2 weeks.
6. **Light list expansion (1024 → 16K+)** — exploits ReSTIR DI's scaling property.
7. **Strip inline shadow loop from forward** — DI fully replaces it.
8. **ReSTIR GI initial + temporal + spatial** — competes with DDGI; ~4–6 weeks.
9. **DDGI ↔ ReSTIR GI quality-preset wiring** — they coexist as user-selectable indirect-diffuse paths.
10. **Bindless buffer tail + GPU-driven culling** — pure structural; no visible change but real perf win.
11. **Subresource tracking in graph** — only when a second feature trips over it.
12. **FSR** — when sub-native rendering becomes valuable.

Items 1–3 are the critical path for any further RT-quality work. Items 4–9 deliver the modern "GI looks right" gap. Item 10 is the GPU-driven payoff. Items 11–12 are situational.
