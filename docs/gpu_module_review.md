# GPU Module Review (`gse.gpu`)

Review of the `gse.gpu` module conducted per `docs/CODE_REVIEW_GUIDE.md` and `docs/STYLEGUIDE.md` against the working tree of the `dev` branch on 2026-08-07.

Scope — the full module, 30 files / ~8,070 lines:

- `Engine/Engine/Import/Gpu.cppm` (18)
- `Source/Gpu/Context.cppm` (132), `Context.cpp` (100)
- `Source/Gpu/Device/` — `Device.cppm` (546), `Device.cpp` (818), `DeviceVulkanBackend.cppm` (831), `DeviceDx12Backend.cppm` (797), `Frame.cppm` (82), `Frame.cpp` (377), `Swapchain.cppm` (242), `PresentPacer.cppm` (107), `BackendState.cppm` (7), `VideoBackend.cppm` (9), `VideoEncoder.cppm` (54), `VideoEncoder.cpp` (46)
- `Source/Gpu/Graph/` — `RenderGraph.cppm` (243), `RenderGraph.cpp` (1320), `TransientPool.cppm` (168), `TransientPool.cpp` (437), `CommandContract.cppm` (323), `CommandDispatch.cppm` (23), `PassRecorder.cppm` (3), `PassRecorder.cpp` (256)
- `Source/Gpu/Resources/` — `Buffer.cppm` (27), `Buffer.cpp` (44), `Image.cppm` (23), `Image.cpp` (44)
- `Source/Gpu/Submission/` — `Task.cppm` (156), `SyncToken.cppm` (39)
- `Source/Gpu/Shader/ShaderCodegen.cppm` (798)

Method: static inspection only, per the guide's verification rules. **Nothing in this review was compiled or run.**

Facts were verified against the consumed APIs rather than assumed: `gse.gpu_record`'s pass construction (`RenderPass.cpp:139-201`, `RenderPass.cppm:43`), `frame_resource_bin` (`GpuBackend/FrameResourceBin.cppm:13-79`), `buffer`'s host-dirty flag (`GpuBackend/Buffer.cppm:96-110`), `queue_type`/`queue_id`/`max_frames_in_flight` (`GpuBackend/Enums.cppm:124-138`), `per_frame_resource` (`Concurrency/PerFrameResource.cppm:19`), and the `execute_frame` / bindless-creation call sites in `Runtime/Engine.cpp:289,452` and the renderers.

Adjacent modules (`gse.gpu_backend`, `gse.gpu_record`, `gse.vulkan`, `gse.dx12`) were read only as far as needed to verify a contract. They are not reviewed here.

---

## High

### 1. The swapchain load/clear fixup runs after recording — it is dead code, and `m_swapchain_load` is dead state

`Source/Gpu/Graph/RenderGraph.cpp:780-797`, with `set_swapchain_clear` (`RenderGraph.cpp:109`) and the clear command buffer (`RenderGraph.cpp:1259-1297`)

- **Impact.** The `op` parameter of `set_swapchain_clear(value, op)` has no effect, and the "first swapchain writer clears, every later one loads" policy this block encodes is never applied. Actual behavior comes from the dedicated clear command buffer prepended at `:1296`, which hardcodes `load_op::clear`. Correctness therefore rests on an unstated convention that every swapchain-targeting pass declares `load_color()`. A pass that declares `clear_color()` on the swapchain silently erases everything drawn before it — the exact failure this block exists to prevent, now unguarded.
- **Mechanism.** `record_range` bakes `pass.color_outputs[ci].op` into `begin_rendering` at `:351` while draining, and the drain loop at `:509-534` finishes all recording before the fixup at `:780` mutates those same fields. Every command buffer is already `end()`ed by then; nothing reads `color_outputs[].op` or `.clear_value` afterward. `m_swapchain_load` (`RenderGraph.cppm:226`) is read only inside the dead block.
- **Resolution.** Delete the fixup block, `m_swapchain_load`, and the `op` parameter of `set_swapchain_clear`, leaving the prepended clear as the single clear authority — that matches what the module already does. If the per-pass policy is the desired behavior instead, the fixup must move ahead of `record_range` and the dedicated clear command buffer goes away. Both are coherent; keeping both is not.
- **Prevention.** One-off refactor orphan — recording moved ahead of planning when passes became coroutines, and this block did not move with it. Deleting the dead state is what makes a recurrence conspicuous: with `m_swapchain_load` gone, re-adding a load-op policy forces the author to place it relative to recording.

### 2. DX12 backend creation dereferences an empty optional in headless mode

`Source/Gpu/Device/DeviceDx12Backend.cppm:787`

- **Impact.** Undefined behavior at boot for any headless run (`win == nullopt`) that selects DX12 — either by setting `backend = dx12` or by hitting the Vulkan-unavailable fallback at `Device.cpp:68-74`. Headless is a supported path: `device::create` takes `std::optional<shared_view<window::data>>`, `context::init` skips swapchain creation when the optional is empty (`Context.cpp:26`), and the Vulkan backend branches on it throughout.
- **Mechanism.** `create_dx12_device_backend` writes `std::make_unique<dx12::device>(*win, validation_layers_enabled, cfg)` unconditionally. The Vulkan twin at `DeviceVulkanBackend.cppm:794-812` guards every window-dependent step (`win ? required_window_extensions() : {}`, `if (win) instance.create_surface(*win)`, `win ? pick_surface_format(...) : b8g8r8a8_srgb`).
- **Resolution.** Thread the optional through the DX12 path as the Vulkan path does, or fail explicitly if DX12 genuinely requires a window. While there, align the return shapes: `create_vulkan_device_backend` returns `gpu::expected<vulkan_backend_creation>` and `create_dx12_device_backend` returns a bare struct with no failure channel, so the fallback path in `device::create` cannot report a DX12 failure at all.
- **Prevention.** The asymmetric creation signatures are the tell. Unifying both backends on the `expected` shape and on identical parameter nullability makes a missing headless branch a compile-visible gap rather than a runtime crash.

### 3. Transient aliasing lifetimes are computed in drain order but executed in topological order

`Source/Gpu/Graph/TransientPool.cpp:102-129` (`compute_lifetime`), `:131-174` (`greedy_color`), with the topological sort at `Source/Gpu/Graph/RenderGraph.cpp:708-727` and the planning call at `RenderGraph.cpp:291-298`

- **Impact.** Two transient resources placed in the same aliased memory block — because their drain-order intervals are disjoint — can overlap in actual execution order. The later resource's writes then clobber the earlier resource's still-live contents. Symptom is frame corruption attributed to the reading pass, on either queue, since alias blocks are shared across queues.
- **Mechanism.** `compute_lifetime` returns `[first, last]` as indices into `pass_kind_order`, which `execute` builds from the *drain order* of round one (`:292-297`). `greedy_color` then aliases any two intervals where `active[c].last_end < iv.first_pass`. Execution order, however, is Kahn's algorithm over the dependency graph using a FIFO `std::queue` (`:711-727`), which does not preserve drain order between independent passes. Concrete case: passes A, B, C, D drained in that order with a single edge A→C sort as A, B, D, C. A transient used by `{B, C}` gets interval `[1,2]`; one used by `{D}` gets `[3,3]`; disjoint, so both take colour 0 and bind at offset 0 of the same block (`TransientPool.cpp:395`, `:423`). D actually executes between B and C and destroys the first transient before C reads it. The alias-discard barrier emitted at first use (`RenderGraph.cpp:1085-1097`) synchronizes the incoming image but by design cannot protect the outgoing one — that is what the disjoint-lifetime guarantee was supposed to do.
- **Resolution.** Make the topological sort stable on drain index: pop from a min-heap (or otherwise select the lowest-index ready node) instead of `std::queue`. That single change makes drain-order intervals valid execution-order intervals, restoring the invariant `greedy_color` assumes. It also makes the "both write, recording order picks the visual order" warning at `:685-695` deterministically true rather than dependent on queue pop order.
- **Prevention.** Recurring class — the guide's paired-derivations case, with "when is this resource live" derived once from drain order and once from execution order. The stable sort collapses them into one authority; alternatively `plan()` could take the sorted order, but that inverts the current call sequence (planning must precede recording, which precedes sorting), so the stable sort is the smaller change.

---

## Medium

### 4. `m_pending_graphics_extra_signals` is not cleared with its three siblings

`Source/Gpu/Graph/RenderGraph.cpp:208-210`, with the drain in `Context.cpp:84-98`

- **Impact.** A semaphore signal pushed via `add_graphics_signal` on a frame that never reaches `end_frame` survives into the next executed frame, producing a duplicate signal entry in one submit — validation failure, and a double-signal on imported semaphores in attached mode.
- **Mechanism.** `execute` clears `m_pending_aux_submissions`, `m_pending_graphics_extra_waits`, and `m_pending_graphics_buffers` at entry, but not `m_pending_graphics_extra_signals`. The other three are cleared precisely because `execute` can return early at `:212` when no frame is in progress; signals were left out. They drain only through `take_graphics_extra_signals` in `context::end_frame`.
- **Resolution.** Clear all four together. Better: group the four pending vectors into one aggregate cleared as a unit, so the next field added inherits the drain contract instead of having to remember it.
- **Prevention.** Local, but the aggregate is the proportionate guardrail — four parallel members with one shared lifecycle rule is exactly the shape where one gets missed.

### 5. The bindless slot-resource table races between concurrently scheduled resource creators

`Source/Gpu/Device/Device.cpp:741-753` (`set_slot_resource`, `resource_for_slot`), reached from `create_buffer` (`:717`), `create_image` (`:729`), `write_storage_buffer` (`:768`), `write_sampled_image` (`:781`)

- **Impact.** Undefined behavior — concurrent `std::vector::resize` on `m_slot_resources`, plus torn reads from `resource_for_slot` running against a reallocating vector. Manifests as corrupted resource refs feeding the render graph's barrier derivation, which is the machinery that decides GPU hazards.
- **Mechanism.** `set_slot_resource` grows the vector with no synchronization. The schedule serializes `gpu::context::run` against readers, but does not serialize *readers against each other*: two systems that both hold `shared_view<gpu::context::data>` can run concurrently, and `const std::unique_ptr<device>` yields a mutable `device&`. Both `Physics/VBD/GpuSolver.cpp:353` and `Graphics/Renderers/GeometryCollector.cpp:403` create bindless buffers this way, as do the atmosphere, cloud, bloom, capture, cull, depth-prepass and forward renderers.
- **Resolution.** Consistent with the engine's no-runtime-locks rule (the schedule owns hazard serialization, not mutexes): preallocate `m_slot_resources` to the bindless heap capacity in the constructor, next to the pass-marker ring allocation. Writes then target distinct, stable objects and the container's shape never changes, so no synchronization is required for the common path.
- **Prevention.** Recurring class — mutable device state reachable through nominally read-only shared views. Fixed-shape storage removes the class structurally; a mutex would reintroduce the runtime locking the ECS design deliberately excludes.

### 6. `is_swapchain` is written by `gse.gpu_record` and read by no one; the graph re-derives the fact

`Source/Gpu/Graph/RenderGraph.cppm:28`, `:36`; written at `Source/GpuRecord/RenderPass.cpp:141`, `:151`

- **Impact.** Duplicated derivation of one fact, already divergent: `to_color_output_info` computes `is_swapchain = target == nullptr && !transient_target`, while `resolve_color_target` (`RenderGraph.cpp:264-275`) answers the same question by null-checking and additionally redirects through `m_offscreen_target`. In offscreen mode the stored flag says "swapchain" for a pass that renders to the offscreen image. The first consumer to trust the field (an editor overlay, a debug view, a future queue-assignment rule) inherits the wrong answer.
- **Mechanism.** The field predates the resolve helpers and was never removed when they took over the decision. Nothing in the module reads it.
- **Resolution.** Delete both `is_swapchain` fields and their two writers, or make the flag the single authority that `resolve_*` consults. Duplicated derivation is the defect even while both copies are individually explicable.
- **Prevention.** Local. The guide's paired-derivations section already names this shape; the deletion is the fix.

### 7. The window/swapchain nullability contract is implicit and inconsistently enforced

`Source/Gpu/Device/Frame.cpp:80`, `:134`, `:154`, `:159`, `:362`; `Source/Gpu/Graph/RenderGraph.cpp:44`, `:346`, `:368`, `:440`

- **Impact.** In headless mode a pass that omits an explicit target dereferences a null `m_swapchain` — a crash far from the cause, in the graph's attachment-resolution path rather than at the point where the unsupported request was made.
- **Mechanism.** The real invariant is `win` present ⟺ swapchain present ⟺ swapchain-targeting passes permitted, established by construction in `context::init` (`Context.cpp:26-38`) but asserted nowhere. `frame::begin` treats `win` as maybe-null at `:80` and then dereferences it at `:134`, `:154`, `:159` guarded only by `m_swapchain`; `frame::end` does the same at `:362`. `render_graph::create_framebuffer_image` dereferences `m_swapchain->extent()` unconditionally at `:44`, and the record path falls back to `m_swapchain->image_view(...)` / `m_swapchain->depth_image()` whenever a target resolves to null.
- **Resolution.** One assert at `frame::begin` entry (`m_swapchain` implies `win`) and one in each resolve fallback (default-targeted pass requires a swapchain). The guide's preference applies directly: where a shared path is impossible, prefer the conspicuous failure over the merely consistent one.
- **Prevention.** Local given the current two-caller topology. If a third front-end appears, the invariant belongs in the type — a swapchain-bearing frame and a headless frame as distinct constructions rather than one class with a nullable member.

---

## Low

### 8. Write-only state

- `frame::m_present_ids_in_flight` (`Frame.cppm:79`) — filled at `Frame.cpp:73`, `:363` and assigned at `:367`, never read.
- `present_pacer::m_multiple` (`PresentPacer.cppm:34`) — assigned in both branches at `:63`, `:67` and reset at `:83`, never read.
- `transient_image_allocation` / `transient_buffer_allocation`'s `color`, `first_pass`, `last_pass` (`TransientPool.cppm:84-85`, `:91-92`) — the planner's intermediates persisted into the allocation records and never consulted; `transient_images()` copies only `resource`/`aspects`/`format`.

Dead writes disguise intent and mask regressions: a reader added later cannot tell whether the value was maintained deliberately. Delete, or wire up the consumer that was intended.

### 9. The dispatch exclude list is duplicated

`Source/Gpu/Graph/CommandDispatch.cppm:16` (`recorder_excludes`) duplicates `Source/Gpu/Graph/CommandContract.cppm:316` (`command_dispatch_excludes`) — same two entries, same purpose, one used to build the dispatch and one to define it. Module-linkage names are nameable across partitions of the same module and `:command_dispatch` already imports `:command_contract`, so one list serves both. Two lists means a future exclusion can be added to one and not the other, and the resulting mismatch is a `define_aggregate` shape error at a confusing site.

### 10. `render_pass_data::record_ctx_slot` is a `void*`

`Source/Gpu/Graph/RenderGraph.cppm:68`, cast at `RenderGraph.cpp:429`

`recording_context_init` is defined ten lines above in the same interface (`RenderGraph.cppm:77-84`), and `gse.gpu_record` already types its own copy of this pointer as `recording_context_init*` (`RenderPass.cppm:43`), passing it straight through in `to_pass_data`. The `static_cast<recording_context_init*>` is an escape hatch with no external contract behind it. Type the field.

### 11. Hand-rolled enum labels and a redundant queue enum pair

- `queue_label` (`RenderGraph.cpp:799-801`) maps a queue index to `"graphics"`/`"compute"` by hand. The reflection-based global formatter already prints enum names — `Device.cpp:114` formats `pass_marker_domain` directly. Format the `queue_type` instead of the index.
- `queue_id` and `queue_type` (`GpuBackend/Enums.cppm:124-136`) are two identical two-enumerator enums, bridged by an ad-hoc ternary at `Task.cppm:120` (`m_queue->id() == queue_id::graphics ? queue_type::graphics : queue_type::compute`). If both must exist, one named conversion should own the mapping; adding a third queue with the current shape means finding every ternary.

### 12. Redundant `retain` overloads

`Source/Gpu/Submission/Task.cppm:30-36` — the `buffer&&` and `std::vector<buffer>&&` overloads do exactly what the member template at `:23-27` does with `T = buffer` / `std::vector<buffer>`. Overload resolution prefers the concrete ones, so they are live but add nothing. Delete both.

### 13. `:pass_recorder` is a pure re-export shim

`Source/Gpu/Graph/PassRecorder.cppm` is three lines: `export import :command_contract`. `export import` inside a partition contradicts the "export-import only at the module header" convention (`Engine/Import/*.cppm`), and re-export chains are the shape recorded as BMI-fragile in this project's toolchain notes. Export `:command_contract` from `Engine/Import/Gpu.cppm` directly and retarget the `import :pass_recorder` sites (`Device.cppm:6`, `RenderGraph.cppm:11`, `Buffer.cpp:9`, `Image.cpp:9`, `RenderGraph.cpp:11`). Note `PassRecorder.cpp` is the implementation of `:command_contract`'s `pass_recorder`, so the file pair is also misleadingly named.

### 14. Style-guide deviations

Each is a direct rule in `docs/STYLEGUIDE.md`:

- **Module-private entities defined inside the namespace block** instead of declared-in / defined-out: `device_backend_delete` (`Device.cpp:26-29`), `device_dispatch_for` (`Device.cpp:35-36`), `append_family_binding` (`ShaderCodegen.cppm:642-674`), `emit_one_user_type` / `emit_one_binding` (`ShaderCodegen.cppm:726-747`). `TransientPool.cpp:14-45` does it correctly and is the model.
- **`non_copyable` inheritance is spelled four different ways.** `device` (`Device.cppm:21`) declares `~device()` without re-declaring the move operations, so it is silently immovable — fine for a `unique_ptr`-held type, but if that is intended it should say `non_movable`. `frame` (`Frame.cppm:21`) and `swap_chain` (`Swapchain.cppm:18`) declare no destructor at all. `video_encoder` (`VideoEncoder.cppm:12-23`) and `transient_pool` (`TransientPool.cppm:95-109`) follow the guide's full pattern. Pick one.
- **Empty constructor body not collapsed** — `SyncToken.cppm:27-28` should be `{}` on the signature line (`TransientPool.cpp:176` does this correctly).
- **`std::` / `pair` split across lines** mid-qualified-name at `TransientPool.cpp:39-40`.
- **Shadowed parameter** — the local `backend` at `Device.cpp:45` shadows the `backend_kind& backend` parameter; the DX12 path names its local `backend_ptr`.
- **Duplicate callback alias** — `context::swap_chain_recreate_callback` (`Context.cppm:53`) and `swap_chain::recreate_callback` (`Swapchain.cppm:82`) are both `std::function<void()>` for the same callback.
- **Pointless local** — `auto result = d.frame->begin(window_s); return result;` (`Context.cpp:78-80`).
- **File-scope mutable globals** — `present_total`, `present_queue_full`, `pacing_health_reported` (`Frame.cpp:21-23`). This is per-instance frame state; it belongs on `frame` or on `present_pacer` (which already owns `m_samples_seen` / `m_samples_used` for the same diagnostic).
- **Whitespace** — trailing whitespace at `Task.cppm:114`, `:130`, `Device.cpp:713`, `ShaderCodegen.cppm:138`; doubled blank lines at `RenderGraph.cppm:54-55`, `DeviceDx12Backend.cppm:744-745`; missing trailing newline at `CommandContract.cppm:324`.

### 15. Shader codegen design debt

`Source/Gpu/Shader/ShaderCodegen.cppm`

- **`binding<Set, Slot>` carries its payload as template parameters** (`:123-127`). This is precisely the shape the review guide calls out: a distinct type per binding puts `annotation_from_enum` / `first_annotation_of_type` out of reach, which is what forces the hand-rolled `annotations_of` walk in `find_binding_type` (`:425-433`). A concrete aggregate — `binding{ .set = 0, .slot = 2 }` read through the `gse.meta` helper — restores helper use and deletes the raw `std::meta` loop. The migration touches every shader binding declaration in the engine, so this is a deliberate decision to schedule, not a quiet fix; recording it here so it is not re-derived at each review.
- **Four parallel tag-dispatch chains** — `emit_slang_binding` (`:435`), `descriptor_type_of` (`:593`), `descriptor_count_of` (`:621`), `descriptor_access_of` (`:631`) each switch over the same tag set and each end in a silent `else` fallback. A new binding tag that misses one chain compiles and silently emits storage-buffer / UBO code. Traits on the tag types themselves (descriptor type, count, access, emitter) collapse four tables into one, and a missing trait becomes a compile error. This is the guide's mandatory question about switches mapping enumerators to fixed metadata, one layer over.
- **Duplicated UBO emission** — the `requires { typename T::element; }` branch (`:515-537`) and the final `else` (`:538-559`) differ only in which type they reflect over; ~20 lines are copy-paste twins.

### 16. Per-frame allocation churn

Grouped because none is individually alarming and all are on the frame path; worth acting on only if profiling implicates them:

- `frame::end` (`Frame.cpp:277`, `:297`, `:314`, `:322`) — four `std::vector`s per frame for wait/signal/command-buffer info.
- `render_graph::execute` — the `n × n` `std::vector<std::vector<bool>> reaches` reachability matrix (`RenderGraph.cpp:599`), rebuilt per frame, plus `update_reachability_for_new_edge`'s O(n²) scan per added edge; per-pass `color_targets` / `color_attachments` vectors inside `record_range` (`:323`, `:329`); three barrier vectors plus three coalescing copies per pass (`:1106-1171`).
- The single-element `clear_attachments` vector at `:1263`.
- `present_pacer::observe`'s `points` vector, allocated per frame (`PresentPacer.cppm:43`).

---

## Observations (verified, not defects)

Recorded so later reviews do not re-litigate them:

- **`active_backend` as a mutable global** (`BackendState.cppm:6`) is the sanctioned seam for the four call sites with no device in hand: the TLAS ABI branch in shader codegen (`ShaderCodegen.cppm:454`), the DXIL switch in `PipelineBuilder.cpp:161`, the attached-surface ring in `Runtime/Engine.cpp:376`, and the editor's cross-process backend check (`Editor/.../Viewport.cpp:122`). Written once during `device::create`. Worth knowing: shader codegen invoked before GPU init would silently emit the Vulkan TLAS ABI.
- **`buffer`'s `mutable std::atomic<bool> m_host_dirty`** (`GpuBackend/Buffer.cppm:110`) is a deliberate design — the graph clears it while walking `const` usage lists (`RenderGraph.cpp:936`) — not the const-bypass of system state the style guide's `mutable` rule targets.
- **The `consteval define_aggregate` dispatch** (`Device.cpp:31-36`, `CommandContract.cppm:318-323`) derives both vtables from the backend's own member list rather than a hand-maintained table. `command_dispatch_for`'s function-local `static constexpr` (`CommandDispatch.cppm:21`) is the BMI-safe placement; `device_dispatch_for` as a namespace-scope variable template would be more consistent moved to match it.
- **The pass-marker checkpoint ring** (`Device.cppm:535-544`, `Device.cpp:148-224`, `:226-402`) — GPU-written begin/body/end checkpoints replayed on device loss, with the record-order-is-not-execution-order caveat stated in the log text itself. Good forensics.
- **Barrier derivation** (`RenderGraph.cpp:952-1054`) correctly treats write-after-read as execution-only (empty `src_access` at `:1017`), tracks per-queue `latest_writes` / `reads_since_write`, and coalesces on the full identity tuple. The alias-discard barriers at first use (`:1056-1099`) are the right place for `discard_contents`.
- **`present_pacer` is fully unit-typed** — `time_t<std::uint64_t>` throughout, ratios taken on quantities (`per_frame / time(m_refresh_interval)` at `:61`), thresholds compared against `time{}`. No `.as<>()` anywhere in the module outside `quantity_cast` at a `system_clock` boundary (`Frame.cpp:103`).
- **Designated initializers, explicit channel-push template arguments, `std::span` returns, `std::ranges` algorithms, structured bindings, and `this auto& self`** are used consistently and correctly across the module. `transient_image` / `transient_buffer` (`TransientPool.cpp:52-72`) are textbook producer-side channel pushes.
- **The bindless-uniform assert** (`Device.cpp:711-715`) encodes a previously-debugged GPU hang in the type-adjacent layer that can catch it. Keep.

---

## Suggested order of work

1. Finding 3 — one-line data-structure swap in the topological sort (`std::queue` → lowest-index-ready selection); restores the aliasing invariant.
2. Finding 1 — delete the dead fixup and `m_swapchain_load`, or move the fixup ahead of recording. Decide which policy the module owns.
3. Finding 2 — thread the optional through the DX12 backend and unify the two creation return shapes.
4. Findings 4, 5, 6 — clear-all-four (or aggregate), preallocate the slot table, delete `is_swapchain`.
5. Finding 7, then the Low cluster.

Findings 15's first bullet (annotation payload as template parameter) is an engine-wide migration and should be scheduled on its own, not folded into this pass.
