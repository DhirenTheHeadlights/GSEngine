# Forward+ Renderer Migration — Scope of Work

## Overview

Migrate GSEngine from a deferred rendering pipeline (G-buffer geometry pass → full-screen lighting pass) to a modern Forward+ renderer with mesh shaders, clustered lighting, inline ray tracing, and optionally a visibility buffer. The goal is a cutting-edge, GPU-driven pipeline that eliminates CPU bottlenecks, supports arbitrary material complexity, and enables pixel-perfect ray-traced shadows.

## Current Architecture

- **3-pass deferred pipeline**: shadow pass → geometry pass (G-buffers: albedo + octahedron normals + depth) → full-screen lighting pass
- **GPU-driven rendering**: compute frustum culling → indirect indexed draws, batch-level AABB culling
- **Traditional vertex pipeline**: vertex/index buffers, `drawIndexedIndirect`, no mesh shaders
- **Shadow maps**: 2D array (directional/spot, 1024²) + cubemaps (point, 512²), PCF 3×3, depth bias hacks
- **Slang shaders** with reflection-based descriptor management
- **Dynamic rendering** (`VK_KHR_dynamic_rendering`), no `VkRenderPass` objects
- **Phase-based ECS scheduler** with channel-based data flow between systems
- **No ray tracing, no mesh shaders, no descriptor indexing, no visibility buffer**
- **No index deduplication** in mesh compiler — trivial indices `[0, 1, 2, ..., N-1]`

## Target Architecture

```
┌─────────────────────────────────────────────────────────────┐
│ Per-Frame Setup                                             │
│  ├─ TLAS rebuild (from per-mesh BLAS)                       │
│  ├─ Cluster light assignment (compute)                      │
│  └─ Upload instance/material/meshlet buffers                │
├─────────────────────────────────────────────────────────────┤
│ Depth Prepass (+ motion vectors)                            │
│  ├─ Task shader: per-meshlet frustum + occlusion cull       │
│  ├─ Mesh shader: emit meshlet triangles → depth + velocity  │
│  └─ Output: depth buffer, motion vector buffer              │
├─────────────────────────────────────────────────────────────┤
│ Forward Shading Pass                                        │
│  ├─ Task shader: reuse meshlet visibility from depth pass   │
│  ├─ Mesh shader: emit meshlet triangles                     │
│  ├─ Fragment shader:                                        │
│  │   ├─ Lookup cluster light list by (screen_xy, depth)     │
│  │   ├─ Evaluate material (bindless textures)               │
│  │   ├─ RT shadows via rayQueryEXT (inline ray tracing)     │
│  │   └─ Accumulate lighting                                 │
│  └─ Output: HDR color buffer                                │
├─────────────────────────────────────────────────────────────┤
│ Post-Processing                                             │
│  ├─ TAA (using motion vectors)                              │
│  ├─ Tone mapping                                            │
│  └─ Optional: bloom, SSAO, SSGI                             │
└─────────────────────────────────────────────────────────────┘
```

---

## Phase 1: Foundation — Meshlets & Mesh Shaders

**Goal**: Replace the traditional vertex pipeline with mesh shader rendering. No lighting changes yet — just get triangles on screen via task/mesh shaders.

### 1.1 Index Deduplication in Model Compiler

The current `.gmdl` compiler stores one vertex per face corner with trivial indices. Before meshlet generation can be effective, vertices must be deduplicated and proper index buffers generated.

- **File**: `Engine/Engine/Source/Graphics/3D/ModelCompiler.cppm`
- Add vertex deduplication pass (position + normal + texcoord hash)
- Generate proper index buffer
- Optionally run vertex cache optimization (Forsyth or Tom Forsyth's linear-speed algorithm)
- Update `.gmdl` format version (v2) to store deduplicated vertices + indices

### 1.2 Meshlet Generation

Split each mesh into meshlets (~64 vertices, ~124 triangles max) at bake time.

- **File**: `Engine/Engine/Source/Graphics/3D/ModelCompiler.cppm`
- Use meshoptimizer library (`meshopt_buildMeshlets`) or hand-roll
- Store per-meshlet: vertex indices (local), primitive indices, vertex count, triangle count, cone axis + cutoff (for backface cluster culling)
- Compute per-meshlet bounding sphere and cone for task shader culling
- Update `.gmdl` format (v2) to include meshlet data
- Store meshlet vertex buffer, meshlet index buffer, meshlet descriptor array

### 1.3 Meshlet GPU Data Structures

New buffer layout for mesh shader consumption:

```
meshlet_vertices[]     — global vertex buffer (deduplicated)
meshlet_indices[]      — per-meshlet local vertex indices (uint8 or uint32)
meshlet_triangles[]    — per-meshlet packed triangle indices (3× uint8 per tri)
meshlet_descriptors[]  — per-meshlet metadata: vertex_offset, triangle_offset,
                         vertex_count, triangle_count, bounding_sphere, cone
```

- **File**: `Engine/Engine/Source/Graphics/3D/Vulkan/Mesh.cppm` — extend to hold meshlet buffers
- **File**: `Engine/Engine/Source/Graphics/3D/Vulkan/Model.cppm` — load meshlet data from .gmdl v2

### 1.4 Vulkan Extension Enablement

Enable mesh shader and descriptor indexing extensions.

- **File**: `Engine/Engine/Source/Platform/Vulkan/Context.cppm`
- Add `VK_EXT_mesh_shader` to device extensions
- Add `VkPhysicalDeviceMeshShaderFeaturesEXT` to feature chain (`taskShader = true`, `meshShader = true`)
- Query `VkPhysicalDeviceMeshShaderPropertiesEXT` for workgroup size limits
- Add capability detection: log warning if mesh shaders unsupported, fall back to vertex pipeline

### 1.5 Task + Mesh Shaders (Slang)

Write the core shaders that replace the vertex pipeline.

- **Task shader** (`meshlet_task.slang`):
  - One workgroup per batch of meshlets (e.g., 32 meshlets per group)
  - Per-meshlet frustum cull (bounding sphere vs. frustum planes)
  - Per-meshlet backface cone cull
  - Emit surviving meshlets as mesh shader payloads
  - Replaces the `cull_instances.slang` compute shader

- **Mesh shader** (`meshlet_mesh.slang`):
  - One workgroup per meshlet
  - Fetch vertices from `meshlet_vertices[]` via `meshlet_indices[]`
  - Emit up to 64 vertices and 124 primitives
  - Transform vertices by instance model matrix
  - Output position (+ normal, texcoord for forward pass; depth-only for prepass)

### 1.6 Rendering Integration

Wire mesh shaders into the geometry renderer.

- **File**: `Engine/Engine/Source/Graphics/3D/Vulkan/GeometryRenderer.cppm`
- Replace `drawIndexedIndirect` with `drawMeshTasksIndirectEXT` (or `drawMeshTasksEXT`)
- Remove vertex/index buffer binding (mesh shaders fetch from storage buffers)
- Dispatch: `ceil(total_meshlets / meshlets_per_task_group)` task groups
- Remove compute culling dispatch (task shader replaces it)
- Keep instance data SSBO (mesh shader indexes into it)

---

## Phase 2: Forward+ Lighting

**Goal**: Replace the deferred lighting pass with clustered forward shading. G-buffers are eliminated.

### 2.1 Depth Prepass

- Mesh shaders render depth-only (reuse task/mesh shaders from Phase 1 with depth-only fragment)
- Generate motion vectors in a second render target (current - previous frame projected position)
- Output: depth buffer + velocity buffer

### 2.2 Cluster Data Structure

Subdivide the view frustum into 3D clusters (froxels).

- Tile size: 16×16 pixels (XY), log-spaced depth slices
- Cluster count: `ceil(width/16) × ceil(height/16) × depth_slices` (typically 16-24 depth slices)
- Per-cluster: light index list (variable length), light count
- Storage: two SSBOs — cluster light counts + cluster light index lists (compacted)

### 2.3 Light Culling Compute Shader

- **New shader**: `cluster_light_cull.slang`
- Input: light SSBO (positions, radii, types), cluster grid parameters, depth buffer
- Per-cluster workgroup: test each light's bounding sphere against cluster AABB
- Output: per-cluster light index list (append to global light index buffer with atomics)
- Dispatch after depth prepass, before forward shading

### 2.4 Forward Shading Pass

- **New shader**: `forward_shading.slang`
- Mesh shaders re-emit geometry (or use visibility buffer — see Phase 4)
- Fragment shader:
  - Compute cluster index from `gl_FragCoord.xy` and depth
  - Read light list from cluster SSBO
  - Loop over lights, evaluate BRDF (upgrade from Phong to Cook-Torrance PBR)
  - Sample shadow maps (or RT shadows if Phase 3 is done)
  - Output HDR color

### 2.5 Renderer Restructuring

- **Delete**: `LightingRenderer.cppm` (full-screen deferred pass)
- **Rename/Refactor**: `GeometryRenderer.cppm` → `ForwardRenderer.cppm`
- **New system**: `ClusterCullSystem` (compute, runs between depth prepass and forward pass)
- **G-buffer images removed** from `swap_chain_config` (albedo_image, normal_image)
- Single HDR color attachment + depth attachment replaces G-buffer layout

---

## Phase 3: Ray-Traced Shadows

**Goal**: Replace shadow maps with inline ray-traced shadows for pixel-perfect, bias-free results.

### 3.1 Vulkan RT Extension Enablement

- **File**: `Engine/Engine/Source/Platform/Vulkan/Context.cppm`
- Add extensions: `VK_KHR_acceleration_structure`, `VK_KHR_ray_query`, `VK_KHR_deferred_host_operations`
- Add features: `VkPhysicalDeviceAccelerationStructureFeaturesKHR`, `VkPhysicalDeviceRayQueryFeaturesKHR`
- The shader compiler already recognizes `SLANG_BINDING_TYPE_RAY_TRACING_ACCELERATION_STRUCTURE`

### 3.2 Acceleration Structure Management

- **New module**: `Engine/Engine/Source/Graphics/3D/Vulkan/AccelerationStructure.cppm`
- BLAS: one per unique mesh (built once, rebuilt on mesh change)
- TLAS: one per frame (rebuilt every frame from instance transforms)
- Compact BLAS after build for memory savings
- Use `VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR`

### 3.3 Inline RT Shadow Queries

- In `forward_shading.slang`, replace shadow map sampling with:
  ```slang
  RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER> rq;
  rq.TraceRayInline(tlas, flags, 0xFF, ray);
  rq.Proceed();
  float shadow = (rq.CommittedStatus() == COMMITTED_TRIANGLE_HIT) ? 0.0 : 1.0;
  ```
- One ray per light per fragment (or stochastic sampling for many lights)
- Soft shadows via jittered ray origins or cone tracing

### 3.4 Shadow Map Fallback

- Keep `ShadowRenderer.cppm` as fallback for GPUs without RT
- Runtime toggle based on device capabilities
- Shadow map path stays as-is, forward shader branches on a push constant flag

---

## Phase 4: Visibility Buffer (Optional, Cutting Edge)

**Goal**: Decouple geometry submission from material evaluation entirely. Maximum GPU efficiency for complex scenes.

### 4.1 V-Buffer Render Target

- Depth prepass becomes a **visibility pass**: output `(instance_id, triangle_id)` per pixel
- Pack into R32G32_UINT or R32_UINT (24-bit triangle + 8-bit instance, or use 64-bit)
- Mesh shaders emit triangle IDs via `SV_PrimitiveID` + instance ID via payload

### 4.2 Material Evaluation Compute Pass

- Full-screen compute shader reads V-buffer
- Fetches vertex data from global vertex buffer using triangle ID + instance ID
- Computes barycentrics, interpolates attributes
- Evaluates material (bindless textures), does lighting, writes HDR color
- Eliminates all overdraw shading cost — only visible pixels are shaded

### 4.3 Bindless Resource Infrastructure

- `VK_EXT_descriptor_indexing` with `UPDATE_AFTER_BIND`
- All textures in a single large descriptor array (up to 500k+ entries)
- Material ID → texture array indices
- **File**: `Engine/Engine/Source/Platform/Vulkan/Context.cppm` — enable descriptor indexing features
- **File**: new `BindlessResourceManager.cppm` — manages global texture/buffer arrays

---

## Phase 5: Post-Processing & Polish

### 5.1 Temporal Anti-Aliasing (TAA)

- Motion vectors from depth prepass
- History buffer (previous frame color)
- Neighborhood clamping, velocity rejection
- Jittered projection matrix (Halton sequence)

### 5.2 HDR Tone Mapping

- ACES or AgX tone mapping
- Auto-exposure via compute histogram

### 5.3 Screen-Space Effects

- SSAO (GTAO or HBAO+)
- Optional: Screen-space radiance cascades for GI (newer technique, no RT required)
- Bloom (downsample-upsample chain)

### 5.4 Temporal Super Resolution

- FSR 2/3 style temporal upscaling
- Render at lower resolution, reconstruct via motion vectors + temporal accumulation

---

## Phase 6: Advanced Techniques (Future)

These are not part of the initial migration but should inform architectural decisions:

| Technique | Description | Dependency |
|-----------|------------|------------|
| **ReSTIR** | Reservoir-based many-light sampling for RT | Phase 3 |
| **DDGI** | Dynamic diffuse GI probe grid | Phase 3 |
| **Radiance Cascades** | Screen-space GI, no RT needed | Phase 5 |
| **Virtual Geometry (Nanite-style)** | Continuous LOD, software rasterization for small triangles | Phase 1 + Phase 4 |
| **Work Graphs** | GPU self-scheduling (future Vulkan) | Phase 1 |
| **Cooperative Matrices** | ML inference in shaders | Phase 5 |
| **Gaussian Splatting** | Neural scene rendering for distant LODs | Phase 4 |

---

## Migration Strategy

### Parallel Path Approach

Keep the deferred renderer functional throughout migration. Each phase produces a working renderer selectable at runtime:

1. **Phase 1**: Mesh shaders feed into existing deferred G-buffer pass (swap vertex pipeline, keep lighting)
2. **Phase 2**: Forward+ path runs alongside deferred (toggle via config)
3. **Phase 3**: RT shadows available in forward path, shadow maps remain as fallback
4. **Phase 4**: V-buffer as optional mode within forward path

### Key Files Affected

| File | Phase | Change |
|------|-------|--------|
| `ModelCompiler.cppm` | 1 | Index dedup, meshlet generation, .gmdl v2 |
| `Mesh.cppm` | 1 | Meshlet buffer storage |
| `Model.cppm` | 1 | Load meshlet data |
| `Context.cppm` | 1, 3 | Extension/feature enablement |
| `GeometryRenderer.cppm` | 1, 2 | Mesh shader draws, then forward refactor |
| `LightingRenderer.cppm` | 2 | Deleted (replaced by forward pass) |
| `ShadowRenderer.cppm` | 3 | Retained as fallback |
| `ShaderCompiler.cppm` | 1 | Mesh shader stage support (if needed) |
| New: `meshlet_task.slang` | 1 | Task shader |
| New: `meshlet_mesh.slang` | 1 | Mesh shader |
| New: `cluster_light_cull.slang` | 2 | Cluster light assignment |
| New: `forward_shading.slang` | 2 | Forward shading |
| New: `AccelerationStructure.cppm` | 3 | BLAS/TLAS management |
| New: `BindlessResourceManager.cppm` | 4 | Global descriptor arrays |

---

## Dependencies & Prerequisites

- **GPU**: NVIDIA RTX 2000+ or AMD RDNA2+ (mesh shaders + ray query)
- **Vulkan SDK**: 1.3+ with `VK_EXT_mesh_shader`, `VK_KHR_ray_query`
- **Slang**: Must support mesh/task shader targets and `RayQuery` intrinsics
- **meshoptimizer** (optional): For meshlet generation and vertex cache optimization
- **Validation**: Keep validation layers on throughout migration; sync validation will catch barrier bugs
