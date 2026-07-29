# Handoff — `gpu::buffer` / `gpu::image` → `handle<T>` migration

**Status: in progress, tree compiles + runs.** Last verified state = device foundations + relayer done; first field-migration slice (LightCulling) applied and *pending a build*.

> There is also unrelated in-tree work from earlier this session (git explorer status-coloring: `Editor/.../Git/*`, `gse.ide.git` system). That's a separate feature and not part of this migration.

---

## Goal

Delete the `gpu::buffer` and `gpu::image` frontend wrapper classes. Renderers hold bare `gpu::handle<gpu::buffer>` / `gpu::handle<gpu::image>`; the **device is the single source of truth** for resource metadata.

## Why

The wrappers are redundant CPU-side mirrors of records the device *already* keeps (Vulkan's `m_live_buffers`/`m_live_images`; the handle-keyed `resource_arena` in `GpuBackend/Arena.cppm`). Sharing those non-copyable wrappers across systems was the root cause of the whole shared-view copyability mess. A `handle<T>` is a trivially-copyable `uint64` id → snapshot-safe, single source of truth, zero duplication.

---

## What's DONE (in the tree)

- **Device foundations, both backends.** `vulkan::device` and `dx12::device` each have handle→metadata records + populate them at create:
  - `live_buffer { bindless_slot slot; device_size size; device_address address; std::byte* mapped; }`
  - `live_image { handle<image_view> view; bindless_slot storage_slot, sampled_slot; image_format_value format; vec3u extent; image_view_create_info view_info; }`
  - Vulkan's `m_live_*` pre-existed and was extended; **DX12 had no such table — it was added from scratch** and populated at the bound create paths.
  - **9 accessors** on each concrete device: `buffer_slot`, `buffer_address`, `buffer_size`, `buffer_mapped` (take `handle<buffer>`); `image_sampled_slot`, `image_storage_slot`, `image_format`, `image_extent`, `image_view` (take `handle<image>`). All const, lock `m_mutex` (now `mutable`).
- **Relayer plumbed.** The 9 accessors are exposed through `gpu::device` + `vulkan_device_backend` + `dx12_device_backend`, so `gpu_s.device->buffer_slot(h)` works.
- **`SharedView.cppm` reworked** (this unblocked the migration): pointer-like `[[= gse::shared]]` fields (`unique_ptr`, `shared_ptr`) are now **reassign-safe** — `publish_kind::decay` snapshots store `&member` (stable address) and read `member->get()` at access time, so reassigning yields the *current* pointee, never a freed one. The `pinned<T>` wrapper + `static_assert` ban we briefly tried were **DELETED** as redundant; git/search/capture reverted to plain `unique_ptr`.

## What's PENDING BUILD

- **LightCulling slice** (first field→handle proof): `LightCullingRenderer` `light_index_list_buffers` + `tile_light_table_buffers` changed `per_frame_resource<gpu::buffer>` → `per_frame_resource<gpu::handle<gpu::buffer>>`; create sites now `.handle()`; its own dispatch reads + `ForwardRenderer.cpp:408-409` reads now use `gpu_s.device->buffer_slot(...)`. **Build this first on the new machine.** If it compiles and renders, the pattern is proven.

---

## The field→handle pattern (apply to every remaining field)

1. **Field type:** `gpu::buffer` → `gpu::handle<gpu::buffer>`; `per_frame_resource<gpu::buffer>` → `per_frame_resource<gpu::handle<gpu::buffer>>`; `std::array<gpu::image, N>` → `std::array<gpu::handle<gpu::image>, N>`. (Handle fields are trivially copyable → `value` snapshot = a real deep copy, not aliased-live. Safety bonus.)
2. **Producer create:** `d.x = device->create_buffer(...)` → `d.x = device->create_buffer(...).handle()`. Safe because `~image`/`~buffer` are no-ops (they don't free — the device arena owns the resource with frame-deferred retirement), so discarding the temporary wrapper leaks nothing.
3. **Every read**, producer *and* consumer: `x.slot()` → `device->buffer_slot(h)`; `img.sampled_slot()` → `device->image_sampled_slot(h)`; `.device_address()` → `device->buffer_address(h)`; etc.
4. Consumers already have `gpu_s.device` (via `shared_view<gpu::context::data> gpu_s`).

**After editing a field, `grep` its name across `Engine/Engine/Source` to catch every reader** — a missed `.slot()` on a now-handle field is a compile error.

---

## Remaining work (ordered)

### A. Rest of the pure-slot buffers (no new machinery)
- `GeometryCollector`: `instance_buffer`, `normal_indirect_commands_buffer`, `material_palette_buffers`. Same pattern as LightCulling.

### B. Producer ops (needed before host-written buffers can migrate)
- Add to both concrete devices + relayer: `host_write(handle<buffer>, const void*, size_t, size_t offset)`, `host_dirty(handle)`, `mark_host_dirty(handle)`, `clear_host_dirty(handle)`.
- Move the `mutable std::atomic<bool> m_host_dirty` from `gpu::buffer` INTO `live_buffer`. **Gotcha:** atomic is non-movable → the `m_live_buffers.emplace(k, live_buffer{...})` sites must switch to `operator[]` / in-place construction.
- `RenderGraph.cpp`'s `append_host_dirty_barriers` reads `buf->host_dirty()` → change to `device->host_dirty(handle)`.
- Then migrate host-written buffers: `AtmosphereRenderer` `atmosphere_ubo_buffer`.

### C. `sample_image(handle)` (needed for images)
- `RecordingContext::sample_image(const image&, stages)` (RecordingContext.cppm:151) → add/replace with a `sample_image(gpu::handle<image>, stages)`. Its impl only reads `img.handle()` + `img.format()`; resolve format via `device->image_format(handle)`. Verify `recording_context` has device access first.
- Then migrate images: `GiProbe` `irradiance_atlas`; `Atmosphere` `transmittance_lut`/`sky_view_lut`/`ap_volume`; `Bloom` `mips_down`/`mips_up`; `Taa` `history`; `SceneSnapshot` `snapshots`.

### D. Delete the wrappers
- Once no `[[= gse::shared]]` field and no call site holds `gpu::buffer`/`gpu::image`, delete the classes from `GpuBackend/Buffer.cppm` + `Image.cppm`. Remaining references (e.g. loop bounds like `per_frame_resource<gpu::buffer>::frames_in_flight` I left in place) will surface as compile errors — fix them then.

---

## Migration checklist (the 14 wrapper shared fields)

**Buffers → `handle<gpu::buffer>`**
- [x] `LightCullingRenderer.cppm:34/35` — `light_index_list_buffers`, `tile_light_table_buffers` *(pending build)*
- [ ] `GeometryCollector.cppm:153/160/161` — `instance_buffer`, `normal_indirect_commands_buffer`, `material_palette_buffers`
- [ ] `AtmosphereRenderer.cppm:187` — `atmosphere_ubo_buffer` *(host-written → do step B first)*

**Images → `handle<gpu::image>`**
- [ ] `GiProbeRenderer.cppm:62` — `irradiance_atlas`
- [ ] `AtmosphereRenderer.cppm:177/179/180` — `transmittance_lut`, `sky_view_lut`, `ap_volume`
- [ ] `BloomRenderer.cppm:49/50` — `mips_down`, `mips_up` (`std::array`)
- [ ] `TaaRenderer.cppm:34` — `history` (`std::array`)
- [ ] `SceneSnapshotRenderer.cppm:13` — `snapshots` (`per_frame_resource`)

---

## Critical gotchas

- **DX12 is the active backend on the laptop** (no Vulkan extension support). Both backends must stay in lockstep — the `gpu_dispatch` vtable is reflection-generated and `build_dispatch<gpu_dispatch, vulkan_device_backend, B>` wires each backend. Adding a device method needs matching methods on **both** `vulkan_device_backend` (`return device_config.fn(a);`) and `dx12_device_backend` (`return device->fn(a);`), plus the public `gpu::device` method (`return m_vt->fn(m_backend.get(), a);`). Test on DX12.
- **Do NOT re-strip `gpu::image`'s special members.** It must stay `class image final : public non_copyable` with explicit `~image() = default` + defaulted move ctor/assign. Removing them makes GCC-modules implicitly *delete* the destructor (ill-formed default) → breaks `Swapchain`. Same applies to `gpu::buffer`.
- **DX12 populate coverage gap:** the `live_*` tables are populated only at the *bound* create paths (`create_buffer` both returns, `create_image`). NOT at `create_buffer_unbound`/`create_image_unbound` or aliased-memory paths. A *shared* resource created via an unbound path → accessor returns a default slot → wrong bindless descriptor → **silent GPU corruption, not a crash.** If something renders wrong after a migration, suspect this.
- **Match each file's exact style** (annotation placement, param wrapping, `[[nodiscard]]` inline vs own-line). `gpu::device`/backends vary per file — read neighbors before editing.

## Build / environment

- Preset: `x64-mingw-gcc-RelWithDebInfo`. Toolchain: gcc-trunk at `~/.gcc-trunk/current/bin` (needs re-bootstrapping on the new machine if not present).
- **Build in the IDE / warm environment, not from a bare CLI** — `cmake --build --preset` from a fresh shell triggers a `CONFIGURE_DEPENDS` reconfigure that re-runs `vcpkg install` (and rebuilds `shader-slang` from source — slow). Incremental IDE builds don't.
- The vcpkg deps (release triplet) are cached per-user; a fresh machine will do a one-time full `vcpkg install`.

## Reference facts

- Device arena: `resource_arena` / `retiring_pool` in `GpuBackend/Arena.cppm` — handle-keyed, frame-deferred retirement (`retire` / `collect(frame)`). This is why `create(...).handle()` + discard is safe and resize-free works.
- `gpu::handle<T>` is `struct { uint64 value; }` (`GpuBackend/Core.cppm`) — trivially copyable.
- Consumers only ever need the **bindless slot** (an index), never the resource object — confirmed in `ForwardRenderer::frame` (every cross-system GPU read is `.slot()`/`.sampled_slot()`). Barriers come from render-graph pass ordering (`.after<...>()`), not per-resource registration.

---

## Uncommitted files (this migration)

`Engine/.../Vulkan/Device.{cppm,cpp}`, `Engine/.../Dx12/Device.{cppm,cpp}`, `Engine/.../Gpu/Device/{Device.cppm,Device.cpp,DeviceVulkanBackend.cppm,DeviceDx12Backend.cppm}`, `Engine/.../Ecs/SharedView.cppm`, `Engine/.../GpuBackend/Image.cppm`, `Engine/.../Graphics/Renderers/{LightCullingRenderer.cppm,LightCullingRenderer.cpp,ForwardRenderer.cpp}`, plus `Editor/.../Git/GitSystem.cppm`, `Editor/.../Search/SearchSystem.cppm`, `Engine/.../Graphics/Renderers/CaptureRenderer.cppm` (the pinned→unique_ptr reverts).

**Commit + push before switching machines** (or this doc + the changes won't be on the new box).
