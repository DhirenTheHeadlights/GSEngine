# Descriptor Buffer Crash Investigation

## Status: Descriptor infrastructure FIXED, physics OOB bugs REMAINING

The descriptor buffer migration is complete and stable. No validation errors, no GPU crashes
with physics disabled. The remaining crashes are pre-existing physics buffer overflow bugs
that were previously masked by traditional descriptor pools (which silently handle OOB reads).

## Hardware
- NVIDIA GeForce RTX 5090 (Blackwell)
- VK_EXT_descriptor_buffer enabled
- Descriptor buffer properties:
  - offset alignment: 64
  - storage buffer descriptor: 16 bytes
  - combined image sampler descriptor: 4 bytes
  - acceleration structure descriptor: 8 bytes

---

## Descriptor Buffer Bugs Found and Fixed

### 1. `bindDescriptorBuffersEXT` offset invalidation (DescriptorHeap.cppm, RenderGraph.cppm)

`descriptor_heap::bind()` called `bindDescriptorBuffersEXT` every time any descriptor set was
bound. Per Vulkan spec, this invalidates all previously set offsets.

**Fix:** Call `bindDescriptorBuffersEXT` once at the start of `render_graph::execute()`.
`bind()` and `commit()` now only call `setDescriptorBufferOffsetsEXT`.

### 2. Compute queue missing `bindDescriptorBuffersEXT` (GpuCompute.cppm, GpuFactory.cppm)

The VBD physics compute queue uses a separate command buffer from the render graph. After
fix #1 moved `bind_buffer()` to only run in the render graph, the compute command buffer
never had `bindDescriptorBuffersEXT` called. All `setDescriptorBufferOffsetsEXT` calls
offset into an unbound buffer -- the GPU read garbage descriptor data.

**Fix:** `compute_queue` now stores a `descriptor_heap*` and automatically calls
`bind_buffer()` in `begin()`, just like the render graph does for graphics.

### 3. Descriptor writer only writing shader-reflected bindings (GpuDescriptorWriter.cppm, GpuDevice.cppm)

`descriptor_writer::commit()` iterated over the **shader's** SPIR-V reflected bindings to
determine which descriptors to write. When a shader uses a shared layout (e.g., VBD physics
predict shader uses only 5 of 10 bindings), Slang strips the unused 5 from the SPIR-V.
Result: 5 descriptor slots were never written -- `collision_state`, `collision_pairs`,
`color_data`, `solve_state`, `warm_starts` all had garbage/null addresses.

**Fix:** Added `const shader_layout* external_layout` to `shader_cache_entry`. When set,
`commit()` iterates the external layout's full binding list (all 10 bindings) instead of
the shader's stripped reflection. Names match via the serialized `.glayout` file.

### 4. Double `load_shader_layouts` destroying pipeline layout handles (GpuDevice.cppm)

`resource_manager::compile()` called `load_shader_layouts()` at the end. With two `compile()`
calls during startup (one for shaders in Engine.cppm, one for assets in Renderer.cppm), layouts
were destroyed and recreated. Pipelines created with the first set of layouts held dangling
`VkDescriptorSetLayout` handles.

**Fix:** Early-return guard in `load_shader_layouts()` if layouts already loaded. The first
`compile()` in Engine.cppm is necessary because physics system init fetches shaders eagerly
before the renderer's `compile()` runs.

### 5. UI renderer stale descriptor offset (UiRenderer.cppm)

The UI renderer deduplicated descriptor commits by texture ID. With descriptor buffers,
each draw needs its own transient descriptor region.

**Fix:** Always commit fresh descriptors per batch. Skip draw if no valid descriptor.

---

## VBD Physics Shader Refactor

### Shared layout system
- Moved VBD shaders from `Shaders/Compute/` to `Shaders/VBDPhysics/`
- Created `Shaders/Layouts/vbd_physics.slang` declaring all 10 bindings
- All VBD shaders use `[Layout(LayoutKind::VBDPhysics)]` attribute
- Shader compiler detects the attribute and assigns the shared `VkDescriptorSetLayout`
- Ensures byte-compatible descriptor regions across all physics shaders

### Infrastructure improvements
- `gse::flat_map` container (MSVC lacks `std::flat_map`)
- `struct_writer` / `struct_reader` for buffer building with `write_index` bounds assertions
- Replaced cached offset structs with inline `flat_map` lookups (-190 lines)
- Shader compiler auto-discovers dependencies via recursive directory scan
- `ShaderLayout` uses `std::optional` and `std::span`, no heap allocation

---

## Remaining Physics OOB Bugs (NOT descriptor-related)

These are pre-existing bugs exposed by descriptor buffers. With traditional descriptor pools,
OOB reads returned zero/garbage silently. With descriptor buffers, they cause GPU faults.

### Root cause: stale/corrupt data in GPU buffers

The `collision_build_adjacency` shader (single-threaded, `numthreads(1,1,1)`) builds per-body
adjacency lists from contact and joint data. Multiple downstream shaders then read through
these lists. Crashes occur when body indices in contacts or joints exceed `body_count`,
causing OOB accesses on stack arrays (`slot_offsets[500]`, `body_color[500]`) or buffer regions.

### Sources of bad body indices

1. **Narrow phase contact counter race:** Multiple threads do `InterlockedAdd(collision_state[0], 1)`
   and the old code decremented on overflow, creating a race where `collision_state[0]` could
   undercount actual written contacts. Fixed by removing the decrement (counter may exceed
   MAX_CONTACTS, but `min()` clamp in adjacency builder handles it).

2. **Stale contact_data slots:** `contact_data` buffer was never zeroed between substeps. If
   `collision_state[0]` overestimates written contacts, the adjacency builder reads stale slots
   with garbage body indices. **Partially fixed:** `collision_reset` now zeros `contact_data[bi].body_a`
   and `body_b` for `bi < MAX_CONTACTS`. Dispatch size increased to cover all 2000 slots.

3. **CPU-side joint body index overflow:** When the CPU physics has > 500 bodies (MAX_BODIES),
   `m_body_count` is clamped but joint body indices are not. Joints referencing body >= 500
   cause OOB on the GPU. **Fixed:** Joint upload now filters out joints with OOB body indices
   and uses `struct_writer::write_index` for assertion.

4. **Unclamped `pc.joint_count`:** Unlike contacts (`min(collision_state[0], MAX_CONTACTS)`),
   joint count is passed directly from push constants with no shader-side clamp. This is low
   risk since `m_joint_count` is clamped on CPU, but inconsistent.

### Crash progression (each fix revealed the next)

| Shader | Crash location | Root cause | Fix |
|--------|---------------|------------|-----|
| `collision_reset` | `body_contact_map[bi] = 0` | Missing `bindDescriptorBuffersEXT` on compute queue | Descriptor fix #2 |
| `collision_build_adjacency` | `body_contact_map[1000 + off_a] = ci` | Descriptor writer only writing 5/10 bindings | Descriptor fix #3 |
| `collision_build_adjacency` | `slot_offsets[c.body_a]` | Stale contact data with garbage body indices | Added bounds checks + contact zeroing |
| `collision_build_adjacency` | `slot_offsets[j.body_a]` (joints) | Same pattern for joints | Added bounds checks + CPU-side filter |
| `collision_build_adjacency` | `body_data[j.body_b].orientation` | Joint constraint loop not guarded | Added bounds check |
| `collision_build_adjacency` | `contact_data[ci].body_a` (coloring) | Stale adjacency list entries | Zeroed contact_data in collision_reset |
| `vbd_solve_color` | `body_data[bi].mass` | **STILL CRASHING** -- valid bi, unclear cause |

### Current crash: `vbd_solve_color`

The crash at `body_data[bi].mass` occurs with `bi` that passes the `bi < body_count` guard.
This suggests either:
- Buffer memory corruption from a prior shader's OOB write
- The body_data descriptor becoming stale mid-frame
- A synchronization issue between the adjacency builder's writes and the solver's reads

This crash does NOT occur with physics disabled, confirming it's a physics-specific issue.

### Recommended next steps

1. **Increase buffer limits** -- `MAX_BODIES=500` and `MAX_CONTACTS=2000` may be too small for
   the simulation. Bodies falling through the floor accumulate contacts rapidly.

2. **Add robustness extension** -- `VK_EXT_robustness2` with `nullDescriptor` and
   `robustBufferAccess2` would make OOB reads return zero instead of crashing, matching the
   old descriptor pool behavior.

3. **Validate the full body_contact_map layout** -- The buffer packs 7 regions at fixed offsets.
   Verify the running_offset prefix sum never exceeds the contact storage region (4000 entries).
   A single overflow corrupts everything downstream.

4. **Investigate `vbd_solve_color` specifically** -- The crash at `body_data[bi].mass` with
   valid `bi` suggests memory corruption rather than index corruption. Check whether any
   prior shader in the substep writes past the end of a buffer that aliases body_data's memory.

---

## Key files

| File | Role |
|------|------|
| `Engine/Engine/Source/Platform/Vulkan/DescriptorHeap.cppm` | Descriptor buffer allocation, binding, offset setting |
| `Engine/Engine/Source/Platform/Vulkan/GpuDescriptorWriter.cppm` | Name-based descriptor writing with external layout support |
| `Engine/Engine/Source/Platform/Vulkan/GpuDevice.cppm` | Shader cache, layout loading, Aftermath integration |
| `Engine/Engine/Source/Platform/Vulkan/GpuCompute.cppm` | Compute queue with auto descriptor buffer binding |
| `Engine/Engine/Source/Platform/Vulkan/ShaderLayout.cppm` | Shared layout loading from `.glayout` files |
| `Engine/Engine/Source/Platform/Vulkan/ShaderCompiler.cppm` | `[Layout]` attribute detection, layout compilation |
| `Engine/Engine/Source/Physics/VBD/GpuSolver.cppm` | Physics buffer upload, dispatch, struct_writer/reader |
| `Engine/Resources/Shaders/VBDPhysics/` | All VBD physics compute shaders |
| `Engine/Resources/Shaders/Layouts/vbd_physics.slang` | Shared descriptor layout (10 bindings, set 0) |

## NVIDIA Aftermath SDK

Integrated at `Engine/External/aftermath/`. Enabled at startup via `GFSDK_Aftermath_EnableGpuCrashDumps`.
Crash dumps saved to `Engine/Resources/Misc/gpu_crash.nv-gpudmp`. SPIR-V and debug info saved to
`Engine/Resources/Misc/aftermath_shaders/`. The user requested removing Aftermath after the
descriptor buffer investigation is complete.
