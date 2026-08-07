# DX12 Module Review (`gse.dx12`)

Review of the `gse.dx12` module conducted per `docs/CODE_REVIEW_GUIDE.md` and `docs/STYLEGUIDE.md` against the working tree of the `dev` branch on 2026-08-07.

Scope — the full module, 15 files / ~3,774 lines:

- `Engine/Engine/Import/Dx12.cppm` (14)
- `Source/Dx12/Device.cppm` (629), `Device.cpp` (1775)
- `Source/Dx12/Commands.cppm` (602)
- `Source/Dx12/CommandPools.cppm` (171), `Swapchain.cppm` (158), `Conversions.cppm` (98), `Queue.cppm` (97)
- `Source/Dx12/Bindless.cppm` (73), `Sync.cppm` (49), `Pipeline.cppm` (45), `Fault.cppm` (41), `AgilitySdk.cpp` (14), `Resources.cppm` (7), `Accel.cppm` (1)

Method: static inspection only, per the guide's verification rules. **Nothing in this review was compiled or run.**

Facts were verified against the consumed APIs rather than assumed: `bindless_slot_pool::offset` (`GpuBackend/Bindless.cppm:119-121`), `max_frames_in_flight` (`GpuBackend/Enums.cppm:138`), `image_format` enumerators (`GpuBackend/Image.cppm:16-31`), `sampler_desc` fields (`GpuBackend/Sampler.cppm:28-40`), the root-signature layout and QPC conversion in `gse.directx` (`External/DirectX.cppm:1175-1181`, `:1724-1760`), and the actual per-frame call sites for the stubbed commands (`Physics/VBD/GpuSolver.cpp:1175-1415`, `GpuRecord/RecordingContext.cpp:410-619`).

`Source/External/DirectX.cppm` (the d3d12 header wrap) and `Source/Gpu/Device/DeviceDx12Backend.cppm` (the seam forwarder) were read as far as needed to verify a contract; they are not reviewed here. The `gse.vulkan` module was read for comparison only — it is the hand-architected reference this module is measured against.

**Verdict.** The module is functional scaffolding with real correctness debt concentrated in exactly the three places `gse.vulkan` is tight: ownership (Vulkan has a reflection-manifest resource arena, retire queues, and a real `collect_garbage`; DX12 has append-only `com_ptr` vectors, a no-op `collect_garbage`, and `mutable` on ten members to launder `const`), single authority for state (Vulkan's `commands` needs no device context by construction; DX12 grew an exported mutable global, a thread-local side map, and two parallel state trackers), and conspicuous failure (`vulkan::commands` has one empty method body; `dx12::commands` has roughly thirty, several on per-frame paths).

Highest-leverage sequence: **1** (unblocks VBD on DX12 and ends the silent-stub class), then **2** and **3** (both are hang-class bugs in the draw path), then **9** and **10** (single-authority cleanups, cheap now and expensive after divergence).

---

## High

### 1. Silently no-op commands sit on live per-frame paths — VBD physics cannot work on DX12

`Source/Dx12/Commands.cppm:467` (`dispatch_indirect`), `:540` (`fill_buffer`), `:542-548` (`copy_buffer_to_image`, `copy_image_to_buffer`, `blit_image`, `copy_image`), `Source/Dx12/Device.cpp:415` (`record_buffer_fill_u32`)

- **Impact.** `Physics/VBD/GpuSolver.cpp:1232-1415` drives the entire colored solve through `dispatch_indirect` and clears solver state through `fill_buffer` every frame. On DX12 all of it silently does nothing — VBD is not slow or subtly wrong, it is absent, and stale solver state is never cleared. `copy_image_to_buffer` and `blit_image` are called from `GpuRecord/RecordingContext.cpp:462,528`, so readbacks return stale contents. Every one of these failures presents as a data problem in the calling system, pointing diagnosis away from the backend.
- **Mechanism.** Empty bodies return `void`; nothing distinguishes "recorded" from "dropped" at any layer. This is the same construction as the `end_commands` stub whose cascade produced the original device-removal saga — a stub that type-checks reads as an implementation.
- **Resolution.** `dispatch_indirect` is mechanical: an `ExecuteIndirect` with a dispatch command signature, mirroring `Device.cpp:910-914`. `fill_buffer` maps to `ClearUnorderedAccessViewUint`, which needs a shader-visible plus CPU descriptor pair — the resource heap already provides both.
- **Prevention.** Recurring class, and the guardrail matters more than any single fix: give every remaining stub a log-once diagnostic (validation builds at minimum) so a dropped command is conspicuous rather than merely consistent. One small helper applied to every `{}` body retires the pattern permanently.

### 2. Per-list pass state lives in a `thread_local` map keyed by recycled list pointers

`Source/Dx12/Device.cpp:299-302` (`graphics_state`), consumed at `:701`, `:714`, `:788`, `:801-807`, reset at `:1157-1159` (`reset_acquired_list`)

- **Impact.** A command list acquired on thread A leaves `pending`, `push_size`, `rtv_formats`, and `compute_pso_bound` in A's map indefinitely. `reset_acquired_list` clears only `compute_pso_bound`, and only in the acquiring thread's map. If a pooled list is ever acquired on one worker and recorded on another — plausible given the task pool's main-thread stealing — the recorder reads a stale entry. A stale `compute_pso_bound = true` lets `dispatch` fire with no PSO bound (`Commands.cppm:456-465`), which is a device-hang class already costly to diagnose on this backend.
- **Mechanism.** The state's natural owner is whatever owns the list's lifecycle, but it is keyed off a raw pointer in a per-thread side table — a second authority that list recycling never reaches. Two facts ("which PSO is pending on this list" and "which list is this") are derived independently.
- **Resolution.** Move `graphics_pass_state` into `command_pools`' `entry` next to the allocator and list, cleared unconditionally on acquire. The pool owns list identity; state stored there resets by construction, with no thread locality and no partial reset.
- **Prevention.** Ownership placement is the guardrail — once the state rides the pool entry, the stale-reuse construction cannot be written.

### 3. Lazily initialized command signatures: data race plus first-caller stride capture

`Source/Dx12/Device.cpp:877-879` (`m_draw_indexed_signature`), `:910-912` (`m_dispatch_mesh_signature`)

- **Impact.** Two workers recording indirect draws concurrently can both observe the null `com_ptr` and both assign it; one thread's reseat frees the object the other just fetched, a use-after-free in the draw path. Independently, the signature bakes the *first* call's `stride`, so any later call site with a different stride silently walks the argument buffer wrong — corrupt draws with no validation error.
- **Mechanism.** Unguarded lazy mutation of a shared member inside a per-draw path, with a per-call parameter that is honored exactly once. Both mutations happen outside `m_mutex`, unlike every other shared-member write in the device.
- **Resolution.** Create both signatures in the constructor from the engine's fixed indirect-argument strides, or cache keyed by stride under `m_mutex`. The laziness buys nothing: the strides are compile-time facts of the indirect argument structs.
- **Prevention.** One-off once initialization moves to the constructor — nothing else in the device lazily initializes shared state, so no broader guardrail is proportionate.

### 4. RTV/DSV view heaps leak descriptors monotonically with no capacity check

`Source/Dx12/Device.cpp:181-184` (capacities 1024 / 256), incremented at `:1023`, `:1028`, `:1620`, `:1626`

- **Impact.** Every swapchain resize recreates render-target images, each consuming view slots that are never reclaimed. After enough resizes or scene reloads the counters walk past the heap end and `CreateRenderTargetView` writes out of bounds — silent corruption or device removal in a long editor session, with the symptom arbitrarily far from the cause.
- **Mechanism.** Allocation with neither a bound nor a free path, compounded by the absence of any image destruction in this backend (finding 7).
- **Resolution.** Minimally `assert(m_rtv_view_next < rtv_view_capacity)` so exhaustion is conspicuous; recycle slots once view and image destruction exist.
- **Prevention.** The assert is proportionate now. Real prevention arrives with the ownership model in finding 7.

### 5. Resource-state tracking has two staleness holes

`Source/Dx12/Device.cpp:481-499` (image barrier, explicit-state path), `:630` (`m_buffer_states.clear()` at present)

- **Impact.** (a) An image barrier carrying an explicit `prev_state` never writes the resulting state back to `m_resource_states` — only the tracked path updates the map — so the next tracked barrier on that image derives a wrong `StateBefore`. (b) `m_buffer_states` is cleared once per frame at present, but D3D12 buffer state decays to `COMMON` after *every* `ExecuteCommandLists`; mid-frame the tracker asserts states the hardware has already dropped. Both produce wrong-`StateBefore` transitions: validation warnings today, real hazards wherever a transition is load-bearing rather than covered by common-state promotion.
- **Mechanism.** Paired-derivation violation. "Current state" has two authorities — caller-supplied versus the map — and the map is maintained on only one of the two paths. Separately, the buffer clear point (present) does not match the actual decay point (per submit).
- **Resolution.** Update the map on the explicit-state path too, and move the buffer-state clear into `queue::submit` after `ExecuteCommandLists`, where decay actually occurs.
- **Prevention.** This tracker is inherently a shadow of driver state. Consistent with the engine's no-manual-barrier-API direction, the honest long-term shape is to lean fully on buffer promotion and decay and stop tracking buffer states at all — fewer states that can disagree.

### 6. `wait_for_fence` discards its timeout, and mislabels the queue in the diagnostic ring

`Source/Gpu/Device/DeviceDx12Backend.cppm:501` (drops `timeout_ns`), `Source/Dx12/Queue.cppm:89-95` (unbounded wait), `:91` (queue hardcoded to `graphics`)

- **Impact.** Where Vulkan surfaces device loss after a bounded wait, DX12 turns a GPU hang into a permanently blocked frame thread — precisely the TDR situations this backend keeps hitting, minus the recovery path. The hardcoded `queue_type::graphics` also falsifies the queue-op ring built specifically for hang forensics, so a compute-side CPU wait is attributed to graphics in the dump.
- **Mechanism.** The seam accepts the parameter and drops it on the floor; the ring record takes a queue argument that the call site fills with a constant instead of the fence's actual queue.
- **Resolution.** Plumb the timeout into `directx::wait_fence` (`WaitForSingleObject` already takes milliseconds) and map the result onto `gpu::result`. Pass the real queue type into `record_queue_op`.

### 7. No ownership or reclamation model: `collect_garbage` is a no-op and nothing can destroy a buffer or image

`Source/Dx12/Device.cpp:1763` (`collect_garbage` empty); append-only `m_owned_buffers`, `m_owned_images`, `m_live_buffers`, `m_live_images`, `m_buffer_by_address`, `m_view_format`, `m_sync_points` (`Device.cppm:566-619`)

- **Impact.** GPU memory and CPU-side maps grow for the process lifetime. Every image and buffer ever created is retained; `retire_semaphore`/`retire_fence` only reset the inner fence and leave the `sync_point` node in the deque forever. The seam calls `collect_garbage` every frame and it lands in an empty function. `gse.vulkan` by contrast has `destroy_buffer`, a family of `retire(...)` overloads, retire queues, and a reflection-manifest resource arena (`Vulkan/Device.cppm:335-359`, `Vulkan/Device.cpp:1468-1535`).
- **Mechanism.** The `mutable` on ten members exists precisely to let `const` creation paths append to these vectors — const-laundering of the kind the style guide's mutation rule exists to prevent, and a symptom of the missing ownership story rather than a local style slip.
- **Resolution.** Mirror the Vulkan retire/collect pattern; the seam plumbing already exists. Note the flip side honestly: the pointer-keyed handle maps are currently safe *because* nothing is ever freed, so implementing destruction without generation-checked handles introduces ABA. The handle representation and the reclamation model need to land together.
- **Prevention.** Recurring class across backends. The strongest practical guardrail is to make the arena itself the reusable layer — `gpu::resource_arena` with a DX12 manifest — so a new backend inherits reclamation rather than reimplementing it.

---

## Medium

### 8. Exported mutable global `active_device`

`Source/Dx12/Device.cppm:628`, consumed by roughly twenty `dx12::commands` methods each guarded by its own `if (active_device)`

- **Impact.** Any importer of `gse.dx12` can reassign the device pointer every command routes through, and the null check is restated at every call site instead of established once.
- **Mechanism.** The dispatch design genuinely forces *some* ambient context: `commands` is constructed from a bare handle by backend-agnostic code, and unlike Vulkan — where the command buffer is self-sufficient under dynamic rendering — DX12 needs the device for PSO resolve and descriptor heaps. That justifies the indirection; it does not justify the visibility.
- **Resolution.** Move the global into the module-private namespace block. The module boundary then owns it, and the repeated null checks collapse to one assumption. Style guide already prefers a plain non-exported namespace over any other hiding mechanism.

### 9. The descriptor and root-constant ABI is stated independently in three files, with one unchecked contract

`Source/Dx12/Pipeline.cppm:43` (`root_constant_count = 64`), `Source/External/DirectX.cppm:1724-1746` (the 32/32 split and register assignment), `Source/Dx12/Commands.cppm:412-434` (`push_data`'s param-index and offset-rebasing convention)

- **Impact.** Three sites must agree byte for byte, and none references the others. Additionally the whole scheme is correct only while every shader's `push_offset_start` is at most 128 bytes (root parameter 1 holds 32 dwords) — nothing asserts this, so a shader with a larger user-push block silently drops constants past dword 32, producing wrong shader inputs with no diagnostic.
- **Mechanism.** Wire-format knowledge duplicated across a layout producer, a serializer, and a consumer. Duplicated derivation is the defect even while all three copies are correct, because only one will be updated.
- **Resolution.** One set of named constants in `:pipeline`, consumed by all three sites, plus `assert(info.push_offset_start <= user_push_capacity_bytes, ...)` in `create_shader_program` (`Device.cpp:1161`).
- **Prevention.** The assert is the guardrail that makes the invariant fail loudly at the boundary where it is first knowable.

### 10. The bindless layout is stated twice, and the texture pool is where the two copies already disagree

`Source/Dx12/Device.cppm:83-92` (`bindless_layout`), `Source/Dx12/Device.cpp:243-258` (pool initialization), `:253` (`m_texture_pool.reset(1024)` only), `:1736,1740` (`register_texture` re-deriving offsets)

- **Impact.** `m_bindless_layout` restates exactly the base-offset, stride, and base-index facts the four `bindless_slot_pool`s already carry. Three pools are fully initialized; `m_texture_pool` receives only `reset(1024)`, leaving `stride = 0`, so `m_texture_pool.offset(slot)` returns 0 for **every** texture slot (`GpuBackend/Bindless.cppm:119-121`). `register_texture` avoids the bug only by re-deriving its offsets from `m_bindless_layout` — that is, the two copies have already diverged and the code routes around the divergence. Any future caller that uses the pool the way all three siblings are used writes descriptor 0 and stomps the first texture.
- **Mechanism.** One fact, two representations, with the incomplete one still reachable through the shared `bindless_slot_pool` API.
- **Resolution.** Initialize `m_texture_pool` like its siblings, switch `register_texture` to `pool.offset()`, then delete `bindless_layout` entirely — the pools *are* the layout.
- **Prevention.** Deleting the second representation is the prevention; a partially initialized pool then becomes unreachable rather than merely unused.

### 11. `create_image_view` is attachments-only behind a general signature, and view handles carry two incompatible meanings

`Source/Dx12/Device.cpp:1015-1032`, with `:1618` (non-attachment images)

- **Impact.** Every non-depth request maps to an RTV; `view_type`, mip range, and layer range are ignored, so an SRV-intent request on a resource lacking `ALLOW_RENDER_TARGET` produces a debug-layer error rather than a view. Separately, `create_image` initializes a non-attachment image's "view" to the bit-cast *resource pointer* while attachment views are *descriptor pointers* — one handle type, two meanings, distinguishable by nothing. A resource-pointer view reaching `cmd_begin_rendering` is dereferenced as a descriptor, and `view_format()` returns `format_unknown` into the PSO key.
- **Mechanism.** An invalid state is representable: the handle type cannot express which flavor it holds, so the pairing is enforced only by convention.
- **Resolution.** Assert attachment usage in `create_image_view`, and stop fabricating pseudo-views for non-attachment images — an empty handle is honest, a wrong-flavored one is a trap.

### 12. Swapchain: settings silently ignored, failures silently reported as success

`Source/Dx12/Swapchain.cppm:61` (discards `present_mode` and `old_handle`), `:111-122` (`acquire_image` returns success on a null swapchain), `:147-157` (`Present(1, 0)` hardcoded, HRESULT ignored)

- **Impact.** The user's vsync setting is a no-op on DX12. A null swapchain reports `{success, image_index 0}`, so the frame proceeds against a nonexistent backbuffer. A device-removed at `Present` is discovered later by an unrelated path, again pointing diagnosis away from the cause.
- **Resolution.** Map `present_mode` to sync interval and `DXGI_PRESENT_ALLOW_TEARING`, return an error result when the swapchain is null, and log a failed `Present` HRESULT. The seven `log::println` calls per swapchain create (`:71-106`) are leftover bring-up diagnostics worth trimming in the same pass.

### 13. Worker command pools: `worker_index` ignored, one mutex serializes all workers, transient pools unlocked

`Source/Dx12/CommandPools.cppm:90-110` (worker acquire), `:120-156` (transient path), `:140` (null check present here but not in the worker path); compare `Vulkan/CommandPools.cppm:325-336`

- **Impact.** The parameter that names the concurrency contract is discarded, and a single mutex serializes every worker's acquire. More seriously, `create_transient_command_pool`, `allocate_transient_primary`, and `transient_pool_try_reset` touch `m_transient_pools` with no lock at all — an `emplace_back` reallocation racing an `m_transient_pools[pool.index]` indexer is undefined behavior if pool creation ever overlaps recording, which the graph's transient-pool creation makes reachable. Finally, `acquire_worker_command_buffer` omits the null check its sibling performs, so on device-removed (where creation returns nulls) `e.allocator->Reset()` dereferences null.
- **Mechanism.** The Vulkan twin indexes per-worker and per-frame slots and therefore needs no shared lock; the DX12 port kept the signature and dropped the indexing, then compensated with a global mutex on one path and nothing on the other.
- **Resolution.** Index worker pools by `worker_index` as Vulkan does — which also deletes the mutex — and give the transient vector the same protection or pre-size it. Add the missing null check.

### 14. Frames-in-flight is stated twice and the two spellings disagree

`Source/Dx12/Device.cpp:169` (`m_frames[qi].resize(3)`) versus `GpuBackend/Enums.cppm:138` (`max_frames_in_flight = 2`), which `CommandPools.cppm:79` does use

- **Impact.** Benign today — an unused third frame target, and `frame_index % 3` still lands on GPU-idle slots — but it is the duplicated-knowledge pattern with one copy already out of step, and the two are used by sibling subsystems of the same backend.
- **Resolution.** Use the constant.

### 15. Dead partitions exported by the aggregator, several of them live traps

`Source/Dx12/Accel.cppm` (one line), `Resources.cppm` (empty `image_view`), `Sync.cppm` (stub class returning null handles), `Bindless.cppm` (stub heaps returning zeroed properties); all re-exported from `Import/Dx12.cppm:5-11`

- **Impact.** No consumer exists anywhere — the seam composes only `fault`, `pools`, `queue`, and `swapchain` (`Gpu/Device/DeviceDx12Backend.cppm:15-20`). These are parallel abstractions with no demonstrated need, and worse they are reachable: `sync::image_available` looks callable and silently returns a null handle, which would produce missing synchronization rather than a compile error.
- **Resolution.** Delete `:sync`, `:bindless`, `:resources`, and `:accel` along with their aggregator lines. Keep `:fault` — it is wired through the seam and its `enabled() == false` legitimately gates the stub queries.

### 16. Fabricated `memory_requirements` in `create_image_unbound`

`Source/Dx12/Device.cpp:978-986` — `texels * 8 * mip_levels`

- **Impact.** Eight bytes per texel regardless of format, and a linear mip multiplier where a mip chain sums to roughly 4/3 of the base level. A full-chain RGBA8 image is overestimated by about 6x. The only consumer is the transient-aliasing planner, which now budgets against fiction.
- **Resolution.** `GetResourceAllocationInfo` returns the real values in one call. If the numbers genuinely do not matter yet because the aliased-memory path is itself a counter stub (`:1133-1136`), say so at the seam rather than inventing plausible-looking arithmetic.

---

## Low

### 17. Silent-fallback `default:` branches in exhaustive enum maps

`Source/Dx12/Conversions.cppm:44` (`dxgi_format_of` → BGRA8), `:96` (`primitive_topology_of` → triangle list), `Source/Dx12/Device.cpp:44-66` (`factor_of` → `blend_inv_dst_alpha`, `op_of` → max, `compare_of` → always)

- **Impact.** `dxgi_format_of`'s default is currently unreachable — all fourteen enumerators are covered — which is exactly why it should go: today it is dead, and the day someone adds a format it converts a compile-time-detectable omission into silently wrong pixels. `factor_of`'s fallback to `blend_inv_dst_alpha` is a particularly poor guess for an unknown blend factor.
- **Resolution.** Drop the defaults from exhaustive switches so a new enumerator fails to compile. Related: `r8g8b8_unorm`/`r8g8b8_srgb` silently remap to their 4-channel forms (`Conversions.cppm:34-35`) while the upload path copies rows unchanged — garbage output if any 3-channel asset exists. Assert instead of remapping.

### 18. `push_data` silently truncates a non-multiple-of-4 tail

`Source/Dx12/Commands.cppm:417` — `data.size() / 4` with no assert.

### 19. BLAS vertex and index formats hardcoded; only `geometries[0]` is built

`Source/Dx12/Commands.cppm:583-590`, `Source/Dx12/Device.cpp:1416-1424` — `format_r32g32b32_float` plus `format_r32_uint`, ignoring what `gpu::acceleration_structure_geometry` declares. True for the engine today; an assert on the incoming format documents the contract at zero cost.

### 20. Style guide items

- Designated-initializer rule violated by mutate-after-default construction: `render_target_blend blend;` then field assignments (`Device.cpp:115-128`), `graphics_pass_state warm;` (`:357`), `gfx_template tmpl;` (`:1182`).
- Redundant `gse::` qualifiers inside `gse::dx12` definitions: `gse::enum_to_string` at `Device.cpp:731`, `:762`.
- `cmd_end` dereferences without the null check every sibling performs (`Device.cpp:433`).
- Trailing whitespace on the blank line at `Device.cppm:553`; doubled blank line at `Gpu/Device/DeviceDx12Backend.cppm:744`.
- Cuddled `} else if` at `External/DirectX.cppm:1716-1718`, against the newline-`else` style used throughout `Source/Dx12/`.

Declaration wrapping, one-parameter-per-line, definitions outside the namespace, and the no-comments rule are correctly observed throughout the module — that part of the earlier style refactor held.

---

## Cross-references

The headless `*win` dereference in `Gpu/Device/DeviceDx12Backend.cppm:787` is already recorded as High #2 in `docs/gpu_module_review.md` and is not re-reported here. It remains open, and it is worth reading together with finding 12: both are cases where the DX12 seam accepts a parameter the Vulkan seam branches on and the DX12 body ignores.

## Cleared suspicions

Recorded so they are not re-investigated:

- `swapchain::past_presentation_timing` is dimensionally sound — `directx::frame_statistics` performs a correct QPC-to-nanoseconds conversion (`External/DirectX.cppm:1175-1181`), so wrapping the result in `nanoseconds(...)` is a real unit construction rather than a raw-number launder.
- The sampler `mip_linear = desc.min == linear` derivation (`Device.cpp:267`) is correct, not a copy-paste slip: `gpu::sampler_desc` has no mip filter field (`GpuBackend/Sampler.cppm:28-40`), so deriving it from `min` is the only available mapping.
- The binary-semaphore `++sp->value` read in `queue::submit` (`Queue.cppm:76-79`) is safe under the engine's producer-before-consumer submission order; it is not the sync bug it resembles.
- Bit-cast handle conversions are whole-handle throughout, matching the established idiom — no `.value` extraction or brace reconstruction anywhere in the module.
