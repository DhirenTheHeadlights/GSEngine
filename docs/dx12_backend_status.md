# DX12 backend — hang diagnostics and new-machine setup

The bring-up itself is done; the backend is at parity with Vulkan for the
engine's graphics, compute, RT and readback paths. What remains here is the
part a fresh machine or a fresh hang cannot recover from code alone.

## Reading the diagnostics

- **Queue-op ring**: `dump_dred_once` prints the last 256 queue ops
  (`[seq] <queue> <kind> fence=<ptr> value=<v> lists=<n>`). Ops are
  recorded BEFORE the API call, so on a hang the newest entry is the
  in-flight op. Kinds: `wait`/`signal`/`execute` (queue-side),
  `cpu_signal`/`cpu_wait` (CPU-side: swapchain acquire signal, present
  wait, frame-begin `wait_for_fence`, `wait_idle`).
- **Sync-point registry**: every created fence/semaphore with timeline
  flag, CPU-side expected value, and `GetCompletedValue`, with a
  `<< LAGGING` marker when completed < expected.
- **Post-TDR caveat**: after device removal `GetCompletedValue` returns
  `UINT64_MAX` for everything, so LAGGING markers are meaningless in a
  post-removal dump — the ring HISTORY is the reliable signal. Any
  CPU wait recorded after the stall "returned" only because the removed
  device satisfies all waits.
- **DRED**: breadcrumb nodes carry real queue/list names
  (`gse.graphics_queue`, `gse.worker compute f1 #3`,
  `gse.frame_primary graphics f0`, `gse.transient graphics p2 #0`).
- Fence shape per frame: compute graph timeline waited/signaled by
  compute each frame and waited by graphics; three per-image
  render_finished fences signaled by graphics then CPU-waited by present;
  per-slot image_available fences CPU-signaled at acquire; per-queue
  in-flight fences signaled at submit end and CPU-waited at frame begin.
- Known cost still in the tree: `dx12::swapchain::present` CPU-blocks on
  the `render_finished` fence before `Present` (`Dx12/Swapchain.cppm`), a
  full CPU⇄GPU serialization per frame.

## New-machine setup (things git does NOT carry)

1. **Slang port pin (CRITICAL for correct rendering)**: the engine needs
   shader-slang **2026.12** (for `DescriptorHandle` bindless /
   `spvDescriptorHeapEXT`), but the pin lives in the vcpkg submodule's
   WORKING TREE only (`ports/shader-slang/{portfile.cmake,vcpkg.json}` —
   uncommitted, submodule checks out 2025.14.3 by default). A fresh clone
   silently builds the old Slang: shaders compile but
   `__slang_resource_heap` never maps (`findCapability=0`) and everything
   renders garbage. The pinned files are carried in this repo at
   `Engine/External/pinned-ports/shader-slang/`. On a new machine, after
   submodule init:

       cp Engine/External/pinned-ports/shader-slang/* \
          Engine/External/vcpkg/ports/shader-slang/
       # then reconfigure so vcpkg rebuilds shader-slang 2026.12

2. **Toolchain**: builds use a custom gcc-trunk (C++26 modules +
   reflection) at `~/.gcc-trunk/current`, built with mcfgthread. Shell
   PATH must include `~/.gcc-trunk/current/bin` BEFORE building (otherwise
   every module .ddi scan fails silently — cc1plus can't load) and before
   RUNNING the binary (needs the gcc runtime DLLs; bare exe exits 127).
3. **Slang session gotcha** (already in code, do not "optimize"): one
   Slang ISession per compile is load-bearing — caching a session across
   compiles crashes.
4. `settings.ini` overrides code defaults (backend selection etc.).
