# Calibrated GPU timestamps

Written 2026-09-13. `VK_KHR_calibrated_timestamps` is required at `Vulkan/Device.cpp:675` and
enabled at `:897`, and `vkGetCalibratedTimestampsKHR` is never called. Grepping the tree for
"calibrated" returns exactly those two lines. Meanwhile the render graph anchors the GPU timeline
to the CPU one by hand, and the anchor is wrong in a specific, bounded way. Goal: replace the
hand-rolled anchor with a real calibrated pair, or decide to drop the requirement.

## The defect

`Gpu/Graph/RenderGraph.cpp:854` samples `slot.cpu_ref = system_clock::now<trace::tick_step>()`
while *recording* the profile-begin command buffer, immediately before recording `write_timestamp`
into it at `:856`. `:184-186` then computes:

```
gpu_ref = timestamps[0] * period
offset  = cpu_ref - gpu_ref
```

`timestamps[0]` is written when the GPU *executes* that command. Everything between the CPU
recording the buffer and the GPU running it, the rest of frame recording, the submit, and the
queue wait, lands in `offset` as error. GPU spans are therefore placed earlier on the shared
timeline than they actually ran, by a full submit latency.

Durations are unaffected. They are GPU-to-GPU deltas and stay correct.

## What research settled

**The clock is `steady_clock`, and the epoch is the hard part.** `trace::tick_step` is
`time_t<std::uint64_t>` (`Diag/Trace.cppm:12`); `system_clock::now<Q>()` casts
`main_clock.elapsed<double>()` (`Time/SystemClock.cppm:252-254`); `gse::clock` is built on
`std::chrono::steady_clock` (`Time/Clock.cppm:22,32-37`). On Windows that is QPC-backed, so
`VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_KHR` paired with `VK_TIME_DOMAIN_DEVICE_KHR` is the
right request, and DX12's `GetClockCalibration` returns a QPC value, so both backends converge on
one host domain.

The obstacle is not the domain, it is the epoch. `tick_step` is relative to
`main_clock.m_start_time`; `main_clock` lives in a module-private namespace
(`Time/SystemClock.cppm:58,66`), `gse::clock` exposes no accessor for its start
(`Time/Clock.cppm:7-19`), and `tick_step` is unsigned with `elapsed()` clamping negatives to zero.
A raw calibrated host counter is not comparable to anything on the trace timeline. Either
`gse.time` grows a rebase entry point, or you sample `system_clock::now` and the calibrated host
value together and keep only the difference. This is required plumbing, not a detail.

**The fix is currently unobservable, and that reorders the phases.** `trace::absorb_events`
(`Diag/Trace.cpp:355-395`) materialises spans from `event_type::begin` and `event_type::end` only;
every other type falls through a `continue`. The render graph emits GPU spans as
`begin_async_at` / `end_async_at` (`RenderGraph.cpp:202-203`, `:233-234`), so no `trace::node`
with a GPU virtual tid is ever produced. `git log -S "async_begin"` over `Diag/Trace.cpp` returns
one commit, meaning the consuming branch never existed.

The other sink is offset-invariant: `profile::ingest_gpu_sample` (`Diag/ProfileAggregator.cpp:202-211`)
takes a duration only, and that is what feeds the editor's live GPU table
(`Editor/Editor/Source/Profile/Profile.cpp:1132-1133`, rendered at `:1200`). So changing the
anchor cannot move a single number the editor shows today.

Teaching `absorb_events` to close async pairs by tid and key is a prerequisite. Without it there
is no way to verify the change, and no reason to make it.

**Two hazards that only bite once the anchor is absolute.**

`RenderGraph.cpp` does no `timestampValidBits` masking at all (`:185`, `:195-196`, `:225-228` use
raw values). With a relative anchor a truncated high bit largely cancels between `timestamps[0]`
and `timestamps[i]`. With an absolute calibrated GPU value it will not. The video encoder already
does this correctly: mask built at `Vulkan/VideoEncoder.cppm:342-343`, applied at `:1405-1406`.
`timestampValidBits` is exposed nowhere else, so getting it per queue family into the graph is
part of this work.

`RenderGraph.cpp:202,203,233,234,267` convert `time_t<double>` to `time_t<std::uint64_t>` through
the narrowing converting constructor at `Math/Units/Quantity.cppm:487-497`. A negative start
becomes roughly 1.8e19 ns. Downstream, `Profile.cpp:274-285` and
`Diag/ProfileAggregator.cpp:164-190` derive the flame graph's origin and span from a min and max
over all nodes, and `Profile.cpp:805-806` subtracts unsigned. One out-of-window GPU span silently
redefines the whole time base. Guard before the narrowing.

**Adding a backend entry point is six decl/def pairs and no vtable edit.** `gpu_dispatch` is
reflection-generated: `Gpu/Device/Device.cppm:19` forward-declares it and
`Gpu/Device/Device.cpp:27-29` defines it with `std::meta::define_aggregate` over
`vulkan_device_backend`'s members. `build_dispatch` (`Meta/ReflectedDispatch.cppm:179-184`)
resolves each op by identifier, so `dx12_device_backend` must grow a same-named compatible member
or it is a compile error. Follow `timestamp_period` exactly:

| Layer | Declaration | Definition |
|---|---|---|
| `gpu::device` facade | `Gpu/Device/Device.cppm:43` | `Gpu/Device/Device.cpp:146-148` |
| Vulkan backend | `Gpu/Device/DeviceVulkanBackend.cppm:32` | `:458-460` |
| DX12 backend | `Gpu/Device/DeviceDx12Backend.cppm:30` | `:456-458` |
| `vulkan::device` | `Vulkan/Device.cppm:151` | `Vulkan/Device.cpp:1082-1084` |
| `dx12::device` | `Dx12/Device.cppm:129` | `Dx12/Device.cpp:411-417` |
| `directx` wrapper | `External/DirectX.cppm:729-731` | `:1803-1809` |

The return type belongs in `GpuBackend/Enums.cppm`, which is where `queue_type` and `query_status`
already live.

**Only two timestamp-pool owners exist, and the per-queue shape is already right.** The render
graph owns one slot per queue per frame (`RenderGraph.cppm:275-279`) and the profile-begin loop at
`RenderGraph.cpp:840-859` already samples `cpu_ref` and query 0 per queue, so `offset` at `:186`
is already per queue and per frame. Keep that shape. Vulkan's `TIME_DOMAIN_DEVICE` is device-wide
so one call serves all queues; DX12's `GetClockCalibration` is per queue, and there are two
(`Dx12/Device.cpp:161-162`, accessor at `:922`). There is no DX12 video-encode queue.

The VBD solver's intra-pass marks need nothing new. `recording_context::mark` writes into the same
pool (`GpuRecord/RecordingContext.cpp:478`) and `RenderGraph.cpp:208-239` resolves them with the
same offset, so they inherit the fix.

**The video encoder is a second site with a worse shape.** Pool at
`Vulkan/VideoEncoder.cppm:583-588`, anchor at `:1282`, consumed at `:1390-1421`. At `:1412-1413`
the begin is taken as `cpu_ref` outright and only the span comes from the GPU, so the encode row
is placed at CPU record time with no GPU contribution at all. It is Vulkan-only, so it can call
`vulkan::device` directly and skip the whole `gpu::device` walk.

**Not sites, confirmed.** The `pass_marker` checkpoint ring (`Gpu/Device/Device.cpp:150-232`) is
device-loss forensics with no timing. Present timing (`Vulkan/Device.cpp:466-516`,
`Gpu/Device/PresentPacer.cppm:47-80`) only differences consecutive samples, so it is
domain-agnostic and never reaches the trace timeline.

**`docs/gpu_intra_pass_timing_plan.md` does not cover this.** It describes query 0 at `:77-78` as
"the CPU to GPU reference stamp" and its open questions at `:322-331` do not mention the anchor.
Its accuracy gate at `:299-302` is "children sum to within a few percent of the parent", which is
duration-based and blind to offset error. This work is unscoped relative to that plan.

**No diagnostic exists that could fail on misalignment.** There is no test directory.
`gse_trace_query` (`Tools/gse-mcp/server.mjs:332-435`) parses text log files, not span timelines,
and cannot see this. `read_profile_slot` has no sanity checks at all, unlike the encoder's
`if (end_ticks < begin_ticks) return;` at `VideoEncoder.cppm:1407-1409`. Building the check is
part of the scope.

## Phases

### Phase 1: make GPU spans observable

Teach `absorb_events` (`Diag/Trace.cpp:355-395`) to close `async_begin` and `async_end` pairs by
tid and key into `trace::node`s, the way it already does for begin and end. Add the negative-start
guard before the `time_t<std::uint64_t>` narrowing at `RenderGraph.cpp:202,203,233,234,267`, and a
`end >= start` check in `read_profile_slot` mirroring the encoder's.

**Gate:** GPU lanes appear in the editor flame graph, the lane labels at `Profile.cpp:404-425`
resolve, and the frame origin and span at `Profile.cpp:274-285` are not distorted by them. At this
point the existing misalignment becomes visible, which is the diagnostic phase 2 needs.

### Phase 2: real calibration for the render graph

Add `timestampValidBits` per queue family into the graph and mask every raw read. Add the backend
entry point down the six-layer walk above, implement it on Vulkan with domain enumeration via
`vkGetPhysicalDeviceCalibrateableTimeDomainsKHR`, and add the `gse.time` rebase so a host counter
can be expressed on `main_clock`'s epoch. Replace the `cpu_ref` sample at `RenderGraph.cpp:854`
with a calibrated pair.

**Gate:** in a trace, no GPU span begins before the CPU span that submitted it, and the gap
between submit and GPU start is visible rather than collapsed to zero. Record the observed shift
so the before and after are comparable.

### Phase 3: DX12 parity and the video encoder

Add `directx::clock_calibration` as a free function in `External/DirectX.cppm` alongside
`timestamp_frequency`, call it per queue, and give `dx12_device_backend` the same-named member the
reflected dispatch requires. Separately, fix `VideoEncoder.cppm:1412-1413` to anchor the encode
row on its GPU begin stamp rather than `cpu_ref`.

**Gate:** the same trace check passes on the DX12 backend, and the video encode lane moves to
where the GPU actually encoded.

## The alternative: drop the requirement

If nobody is going to do phase 1, delete the require at `Vulkan/Device.cpp:675` and the enable at
`:897`. Nothing calls the extension, the editor's GPU numbers are duration-based and unaffected,
and it is one fewer hard device requirement on machines that cannot meet it. See
[headless-no-gpu-device.md](headless-no-gpu-device.md) for the machine that prompted this. Leaving
it required and unused is the one option with no upside.
