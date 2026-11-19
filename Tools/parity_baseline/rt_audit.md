# Ray-query / TLAS audit (updated 2026-10-31, milestone 1 seam landed)

Check that this list is complete with:

```
rg -n "RayQuery|TraceRayInline|scene_tlas" Engine/Resources/Shaders
rg -ln "shaders::tlas|make_acceleration_structure_arg|build_tlas|create_blas" Engine/Engine/Source/Graphics
```

The first must hit only `Standard3D/lighting_ray_traced.slang` and `Standard3D/probe_radiance_ray_traced.slang`. The second must hit only `ForwardRenderer.cpp`, `GiProbeRenderer.cpp` and `RtShadowRenderer.cpp`, and in the first two only inside the ray-traced entry and its binding. Everything else that matches lives under `Gpu/`, `GpuRecord/`, `Vulkan/`, `Dx12/` and `External/`, which is backend plumbing, not a consumer.

`Graphics.lighting_technique` (`gse::lighting::data`, registered before staging in `Engine.cpp`) picks one set of systems at startup: `ray_traced` keeps `rt_shadow` and disables `raster_shadow`, `gtao`, `ssr`; `raster` disables `rt_shadow`. Forward and GI read the technique's bindings from the `renderer::lighting_source` channels, never from `rt_shadow` directly.

## Ray consumers and their raster counterparts

| Seam | Ray-traced implementation | Raster implementation |
|---|---|---|
| `IShadowVisibility` (`lighting_common.slang`) | `lighting_ray_traced.slang` `trace_shadow` | `lighting_raster.slang`: 4 sun cascades (`raster_shadow`, 2048² each) + local spot/point tiles (64 × 512², nearest lights first), PCF by shadow quality |
| `IAmbientOcclusion` | `trace_ao` | `gtao` system: horizon-based GTAO + depth-aware blur, sampled per pixel |
| `IReflection` | `trace_reflection` (reads `material_palette[CommittedInstanceID]`) | `ssr` system: depth march against last frame's TAA history; misses fall back to sky blended with GI-probe irradiance |
| `IProbeRadiance` (`probe_radiance.slang`) | `probe_radiance_ray_traced.slang` | `probe_radiance_raster.slang`: `gi_probe` rasterizes 16 probes × 6 faces at 16² per frame (round robin, reset on `temporal_reset_request`) and the update reads the capture |

## TLAS producers (absent on the raster path)

- `RtShadowRenderer.cpp`: BLAS cache, per-frame `gpu::build_tlas`, owns the TLAS bindless slots and publishes `lighting_source::ray_traced_scene`.
- `Shaders/Bodies/Compute/tlas_transform_update.slang`: still compiled out (`use_gpu_tlas_transform_update = false`).
