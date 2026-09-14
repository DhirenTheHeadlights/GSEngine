# Headless with no GPU device

Written 2026-09-13. Motivation: a dedicated server on a machine whose GPU cannot satisfy the
engine's Vulkan or DX12 requirements. The target box is an AMD Radeon R7 200 on driver 22.6.1,
which reports none of `VK_EXT_shader_object`, `VK_EXT_descriptor_heap`, `VK_EXT_mesh_shader`,
`VK_KHR_acceleration_structure`, `VK_KHR_ray_query`, or `VK_EXT_host_image_copy`, and whose
driver branch predates D3D12 enhanced barriers. Goal: run the CPU solver with no device at all,
degrading with a log line instead of an abort.

## What research settled

**The device is created in system init, not lazily.** `Runtime/Engine.cpp:175` registers
`gpu::context` unconditionally, ahead of the `if (m_config.render)` block at `:181`.
`Gpu/Context.cppm:82-86` declares `init` as a `system_init` hook; `Gpu/Context.cpp:17-40` calls
`device::create` inside it. Both the render path (`Engine.cpp:220`) and the headless path
(`Engine.cpp:276`) run inits, so the device exists before `engine::initialize` returns either way.
`Gpu/Context.cpp:33-34` also builds `frame` and `render_graph` unconditionally, with no window.

**Disabling the system is the cheap route; a null device is not.** Every guard in the tree tests
whether the state is present, never whether `d.device` is non-null. `Gpu/Context.cpp:140` is the
only exception. `Runtime/Engine.cpp:597-601` guards on `try_state_of` and then calls
`gpu::context::wait_idle`, which dereferences `d.device` unguarded at `Gpu/Context.cpp:110-112`.
Keeping the system registered with a null device means auditing every `d.device->` site. Put
`id_of<gpu::context::data>()` into the `disabled` set at `Engine.cpp:188-192` instead.

**The scheduler handles that cleanly.** `Ecs/Scheduler.cpp:1159-1163` marks disabled roots
inactive and `:1185-1194` propagates to every system with a required dep. Inactive systems are
silently dropped with a reason logged at `:1262`, not asserted. `check_closed_dep_graph`
(`:880-923`) cannot fire, because propagation guarantees no registered node keeps a required dep
on an unregistered state. Optional deps stay optional: `promote_optional_deps` (`:655-677`)
promotes only when the provider is registered.

**The CPU physics path is already clean.** `physics::init` and `physics::frame` take
`std::optional<shared_view<gpu::context::data>>` (`Physics/System.cppm:785-790`, `:826-834`) and
early-return without it (`System.cpp:1161-1168`, `:2221-2224`). `physics::prepare` uses
`runs_after_optional` (`System.cppm:791`). `gpu_solver_active` is
`use_gpu_solver && buffers_created()` (`System.cpp:345-351`). The only `gpu::` type on the CPU
path is a never-dereferenced snapshot pointer at `System.cppm:213`. No work needed here.

**`physics::shadow_step::init` is the precedent to copy.** `Physics/VBD/ShadowStep.cpp:391-395`
already logs "shadow step: there is no gpu context, so the harness cannot run" and returns.
Degrade with a log line, do not assert.

**Asset loading is the real blocker, and it is not a gate.** `register_loaders()` runs in the
headless branch too, at `Runtime/Engine.cpp:262`, over the same `game_assets` set that includes
`graphics::asset_types`. Four loaders suspend on `gpu::on_gpu`: `Graphics/2D/Texture.cpp:51`,
`Graphics/3D/Model.cppm:122`, `Graphics/3D/Animations/SkinnedModel.cppm:228`, and
`Graphics/2D/Font.cpp:112` through a nested texture load. The awaitable
(`Gpu/Context.cppm:158-176`) pushes a resume request whose only consumer is `gpu::context::run`
(`Gpu/Context.cpp:114-119`). With the system unregistered nothing ever resumes it, the slot stays
`loading` with `load_in_flight` set (`Assets/AssetRegistry.cppm:367-388`), and `asset::shutdown`'s
`task::wait_idle()` (`Assets/AssetState.cppm:84`) can hang.

The scheduler cannot see this. `asset::data` is the *producer* of `gpu_resume_request`
(`Assets/AssetState.cppm:25,37,53-56`) and declares no dep on `gpu::context::data`, so channel
starvation analysis (`Scheduler.cpp:1196-1211`), which only deactivates consumers of unproduced
channels, leaves asset alive.

This reaches the dedicated server directly. `player_spawner::run` is a server system
(`Sandbox/Sandbox/Source/GameSystems.cpp:38-42`); it calls `character_model` which requests
`SkinnedModels/x_bot.v3` (`Sandbox/Sandbox/Source/Sandbox/RuntimeSpawns.cppm:1090`).
`spawn_character` guards with `if (!model.valid()) return;` (`:1112`), so the server silently
never spawns a character. Any path reaching `handle::acquire()` asserts instead
(`Assets/ResourceHandle.cppm:125-127`).

**"Try Vulkan, then DX12, then none" is not reachable today.** `Gpu/Device/Device.cpp:35-92` only
falls back when Vulkan returns `unexpected`, which happens at exactly one place,
`Vulkan/Device.cpp:752-758`. Every earlier Vulkan failure is a hard exit:
`Vulkan/Instance.cppm:292` asserts on instance creation, `:122-123` dereferences
`enumeratePhysicalDevices` unchecked, `Vulkan/Device.cpp:582` asserts on an empty device list,
`:643` asserts on extension enumeration. `gse::assert` is `_Exit(3)` in release
(`Import/Assert.cpp:28-48`), so there is nothing to catch.

DX12 has no failure channel at all. `create_dx12_device_backend` returns a plain aggregate, not
`expected`, and asserts internally at `Dx12/Device.cpp:154-155`. Worse, it dereferences the
window optional unconditionally at `Gpu/Device/DeviceDx12Backend.cppm:833`, and
`dx12::device`'s constructor takes a non-optional `shared_view<window::data>`
(`Dx12/Device.cppm:116`) and calls `hwnd_from_glfw_window` at `Dx12/Device.cpp:168`. DX12 is
structurally windowed-only. The existing headless GPU path works only because Vulkan succeeds.
`backend_kind` has no `none` (`GpuBackend/Enums.cppm:9-12`).

**The condition cannot be derived, so it needs an explicit field.** `!render && !use_gpu_solver
&& !headless_gpu` fails on both GPU terms:

- `engine_config.use_gpu_solver` is not authoritative. `Engine.cpp:130-132` pins the setting up
  when the config is true but never pins it down, so `--setting Physics.use_gpu_solver=true`
  (which Sandbox itself passes at `Main.cpp:127`) enables the GPU solver with the config false.
  That is already a latent bug at `Engine.cpp:287`.
- `m_headless_gpu` reads `physics::shadow_step::data.enabled`, and that state is deferred. It does
  not exist until `register_deferred()` at `Engine.cpp:255`, which is 56 lines after
  `resolve_activation` at `:199`.

Add an explicit `engine_config` field. Do not reconstruct the answer from settings reads before
registration; that duplicates the resolution layering at `Save/SaveSystem.cppm:491` and will
drift.

## Phases

### Phase 1: run with no device, accepting no graphics assets

**Implemented 2026-09-14.** `engine_config.gpu`, a plain bool defaulting to true, so the CLI
grows `--no-engine-gpu` through the existing `Meta/Args.cppm` reflection with no parser change.
When false, `Engine.cpp` inserts `id_of<gpu::context::data>()` into `disabled` and logs one
runtime line. `gpu::context` stays *registered*: only the `disabled` set is used, because skipping
`register_systems` as the original plan suggested would make every dependent print the misleading
"required dependency not provided — likely a missing registration" warning, whereas a disabled
root prints "disabled in this mode" and each dependent prints "dependency 'gpu::context' is
inactive". `promote_optional_deps` walks `m_nodes`, i.e. registered nodes after activation, so an
inactive provider never promotes an optional dep to required.

The shutdown site already guarded on `try_state_of`, and inactive candidates never reach
`register_node`, so `m_states` has no entry and the guard holds. No change was needed there. The
`Engine.cpp` headless-frame gate now reads `physics::data::use_gpu_solver` instead of
`m_config.use_gpu_solver`, which also closes the latent `--engine-setting Physics.use_gpu_solver=true`
gap.

**Gate:** the dedicated server boots to a running simulation loop with `--no-engine-gpu`, and the
runtime log names every system dropped and why, via the existing line at `Scheduler.cpp:1262`.

### Phase 2: make graphics assets loadable without a device

This is the real work and it decides whether the server is useful. Two shapes, pick one:

1. Give `gpu::on_gpu` a no-device resume path so the four loaders complete with CPU-side data
   only: vertices, skeleton, and hull populated, GPU buffers left null, slot marked loaded.
   Every consumer that reaches for a buffer must then tolerate null.
2. Split `graphics::asset_types` so a no-GPU run registers only the CPU subset, then make
   `game_assets` at `Engine.cpp:262` and every server-side `asset::get` call site work against
   that subset.

Option 1 keeps one asset graph and one call-site contract, at the cost of a null-buffer invariant
spread across the graphics layer. Option 2 keeps the invariant local, at the cost of two asset
type sets and a second set of call sites to keep honest.

**Implemented 2026-09-14 as option 1, narrowed.** The server does need the graphics types:
`spawn_character` reads `bones()` and `proxy()` off the skinned model to build the physics rig, so
option 2 was never available. But the null-buffer invariant did not have to spread, because every
consumer of a mesh's GPU buffers or a texture's image lives in a renderer
(`GeometryCollector.cpp:231`, `SkinRenderer.cpp:111`, and the draw paths behind them), and every
renderer has a required dep on `gpu::context::data` and is already dropped. Nothing on a no-GPU run
can reach a null buffer.

The shape: `asset::init` takes `std::optional<shared_view<gpu::context::data>>`, the same
signature `physics::init` already uses, and records `asset::data::gpu_available`. The three
loaders (`Texture.cpp`, `Model.cppm`, `SkinnedModel.cppm`; `Font.cpp` goes through the texture
loader) skip the `co_await gpu::on_gpu` block when it is false and `co_return` loaded with CPU
data populated. `gpu::on_gpu` itself is unchanged: the "no device" fact lives on the asset state
struct, not as a bool threaded through the awaitable. The unbound-channel trick (leave
`asset::data::channels` unbound to mean "no GPU") was rejected because `launch_load` asserts
`channels.bound()` as its "run before flush" invariant.

**Gate:** the dedicated server spawns a character and a client connecting to it sees that
character move. Confirm the asset slot reaches `loaded` rather than sitting in `loading`, and that
`asset::shutdown` returns instead of hanging on `wait_idle`.

**Gate result 2026-09-14 (this machine, device disabled by flag):** `Sandbox.exe --no-engine-gpu
--engine-net-role dedicated` runs 120 physics steps per window. A client on the same box
connected; the server's `player_state_broadcast` line went from `5 unpossessed` to `119 sent, 0
body miss`, and the client's `player_sync` line reports the local proxy and `server says` at the
identical position with `mass 78 kg colliding true`. The 78 kg proxy only exists if
`spawn_character` got past `model.valid()` and `rig->bones()`, so the skinned model reached
`loaded` without a device. Not exercised: driving input to watch it walk, and graceful shutdown
(the test killed both processes with `timeout`). Still to do on the real R7 200 box: the same two
commands.

### Phase 3 (optional, separable): graceful device-creation failure

Only worth doing if you want a machine to auto-detect its own incapacity rather than being told
by config. It means converting `Vulkan/Instance.cppm:292`, `:122-123`, `Vulkan/Device.cpp:582`
and `:643` from asserts to `expected` returns, giving `create_dx12_device_backend` a failure
channel, making the DX12 window optional or conditioning the fallback order on whether a window
exists, and adding `none` to `backend_kind`.

Note the ordering problem: `resolve_activation` runs once at `Engine.cpp:199`, before any init,
while a device probe result only exists inside `gpu::context::init`. Runtime-discovered failure
cannot feed the current single-shot activation model. Either add a standalone pre-registration
capability probe, which duplicates the feature checks at `Vulkan/Device.cpp:731-752`, or accept
that phase 1's explicit flag is the answer and stop here.

**Recommendation: stop after phase 2.** Phase 3 buys convenience on one unusual machine and costs
a refactor of both device init paths.

## Out of scope

- Making the renderer or GUI work without a device. They are required-dep systems and already
  deactivate correctly; leave them dropped.
- Video encode. `device::make_video_encoder` already returns nullopt when disabled
  (`Gpu/Device/Device.cpp:447-455`) and its only consumer is render-gated
  (`Graphics/Renderers/CaptureRenderer.cpp:46`).
- The GPU solver and the shadow-step harness. Both already degrade correctly.

## Two adjacent bugs found while scoping

- `Gpu/Device/Device.cpp:70` set `gpu::active_backend = dx12` *before* DX12 was known to work, so
  a DX12 failure left the global claiming a backend that never initialised. **Fixed 2026-09-14**:
  the assignment now follows `create_dx12_device_backend`.
- `Gpu/Context.cpp:33-34` builds `frame` and `render_graph` even when there is no window. **This is
  not a bug and was left alone.** The headless GPU path (`Engine.cpp` `update()`, GPU solver and
  shadow step) records into that render graph and submits through `frame`; without a window they
  are still the solver's execution path.

## Note on VK_KHR_calibrated_timestamps

It is required at `Vulkan/Device.cpp:675` and enabled at `:897` but never called, so today it is a
hard device requirement bought for nothing. It is one of the extensions the target machine lacks.
Either put it to work, which is scoped in [calibrated-gpu-timestamps.md](calibrated-gpu-timestamps.md),
or drop the requirement. Do not leave it as it is.
