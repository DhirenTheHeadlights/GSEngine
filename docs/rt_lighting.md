# Ray Traced Lighting — Design Document

## Status

| Feature | Status | Quality Presets |
|---------|--------|-----------------|
| RT Shadows | Done | Off / Hard / Low / Medium / High |
| RT Ambient Occlusion | Done | Off / Low / Medium / High / Ultra |
| RT Reflections | Done | Off / Low / Medium / High |
| PBR Material Pipeline | Done | Cook-Torrance BRDF, material palette |
| Material Embedding | Done | No .gmat files, embedded in .gmdl/.gsmdl |
| Blender Exporter PBR | Done | Reads Principled BSDF, writes embedded materials |
| RT Global Illumination | Not started | — |

---

## Architecture

All RT features are inline ray queries in the forward fragment shader (`meshlet_geometry.slang`). No separate RT pipelines, no RT shading table.

```
depth prepass → light culling → TLAS rebuild → forward shading
                                                    ├─ Cook-Torrance BRDF (PBR)
                                                    ├─ RT shadows (per-light visibility)
                                                    ├─ RT AO (hemisphere occlusion)
                                                    └─ RT reflections (material palette hit shading)
```

Each feature has:
- Quality enum in ForwardRenderer.cppm with UI property via save::register_property
- Push constant int field in meshlet_geometry.slang
- Config struct + get_*_config() function in the shader
- Distance-adaptive sampling and fade-out

## Material Pipeline

Material is a pure value type (struct, not a resource). Stored by value in mesh/skinned_mesh.

```
Blender → exporter.py → .gmdl v4 / .gsmdl v2
                         └─ PBR data embedded per mesh:
                            base_color, roughness, metallic + texture filenames
```

Material palette on GPU: `StructuredBuffer<MaterialData>` at binding (5, 0).
Built per-frame from unique materials via `flat_map<const material*, uint32>` in render_data.
TLAS instance custom_index = palette index for RT hit-point shading.

---

## RT Shadows

Inline ray queries. Binary shadow factor (hit = 0, miss = 1). Distance-adaptive sample counts with jitter via interleaved gradient noise + golden angle rotation.

| Preset | Rays | Range | Fade Start |
|--------|------|-------|------------|
| Hard | 1 | 50m | 40m |
| Low | 1 | 100m | 80m |
| Medium | 2 near / 1 far | 200m | 150m |
| High | 4 near / 2 mid / 1 far | 500m | 400m |

## RT Ambient Occlusion

Cosine-weighted hemisphere rays around the surface normal. Returns occlusion factor [0, 1] applied to ambient term.

| Preset | Rays | Range | Fade Start |
|--------|------|-------|------------|
| Low | 1 | 5m | 4m |
| Medium | 2 near / 1 far | 10m | 8m |
| High | 4 near / 2 mid / 1 far | 20m | 15m |
| Ultra | 8 near / 4 mid / 2 far | 30m | 25m |

## RT Reflections

Traces reflection rays from low-roughness surfaces. Hit-point shading uses material palette base_color (no texture sampling at hit points — requires bindless). Fresnel-weighted blending.

| Preset | Rays | Range | Max Roughness |
|--------|------|-------|---------------|
| Low | 1 | 100m | 0.1 |
| Medium | 1 | 200m | 0.3 |
| High | 2 | 500m | 0.5 |

## Cook-Torrance BRDF

GGX distribution + Smith geometry + Fresnel-Schlick. Roughness clamped to min 0.08 for BRDF eval (prevents GGX fireflies on mirror surfaces). Specular output capped at 32.0. Diffuse normalized by /PI.

Camera world position extracted via `mul(inv_view, float4(0,0,0,1)).xyz`.

---

## Phase 3: RT Global Illumination (Not Started)

Probe-based (DDGI-style) irradiance grid. Decouples GI ray cost from screen resolution.

### Architecture

```
new compute pass: probe_update
    ├─ each probe traces N rays (cosine-weighted hemisphere)
    ├─ shade hit points with direct light via material palette
    ├─ encode radiance + distance into octahedral maps
    └─ cascaded update: fraction of probes per frame

forward fragment shader:
    └─ sample 8 nearest probes (trilinear), blend into ambient
```

### Data Structures

- Probe grid: uniform 3D, configurable spacing (1-4m)
- Per-probe: 8x8 octahedral irradiance (RGBA16F) + 16x16 distance (RG16F)
- Storage: 2D texture atlases

### Quality Presets

| Preset | Rays/Probe | Probes/Frame | Spacing |
|--------|-----------|--------------|---------|
| Low | 64 | 1/8 | 4m |
| Medium | 128 | 1/4 | 2m |
| High | 256 | 1/2 | 1m |

### Prerequisites

- Material palette (done)
- TLAS with custom_index (done)
- Probe atlas texture allocation
- Probe placement strategy for dynamic scenes

### New Files

| File | Purpose |
|------|---------|
| GIProbeRenderer.cppm | Probe grid management, atlas allocation, compute dispatch |
| gi_probe_update.slang | Probe ray trace + irradiance encoding |
| gi_probe_sample.slang | Probe sampling functions for forward shader |

---

## Future Improvements

- **Bindless textures**: texture sampling at RT hit points (reflections + GI)
- **Normal mapping**: TBN matrix in vertex output, sample normal map in forward shader
- **Roughness/metallic texture maps**: sample in forward shader alongside albedo
- **Temporal denoising**: reproject AO/reflection from previous frame
- **Specular antialiasing**: modify roughness based on normal derivatives
