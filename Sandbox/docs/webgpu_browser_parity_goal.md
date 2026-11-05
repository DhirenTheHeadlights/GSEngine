# Goal: the browser WebGPU frame matches native, and textured scenes load

## Aim

Close the open items from the browser-render cycle. The browser frame must be shown to equal the native WebGPU frame, not just be non-black. It must work in headed Chrome as well as headless. Textured `.gmodel` scenes must load in wasm. The leftover gaps from the cycle must each be closed or documented as a decision.

Native C4 (the blue deficit on lit geometry) is **not** in this goal. It already belongs to `docs/webgpu_backend_goal.md` (criterion C4, ledger 2026-11-02), and two goals editing the same shaders would collide. Steer it there.

## Pass criteria

All are checked from artifacts under `Tools/parity_reports/<stamp>/`, written by `Engine/cmake/wasm/host_gate.mjs` (extended) or a sibling script:

- **B1 Image parity.** `lighting_parity` runs at a resolution pinned by the scenario. The browser readback is compared against a native `Graphics.backend=webgpu` capture at the same size. Pass: mean absolute difference ≤ 2.0 per channel, and ≤ 1% of pixels with a delta over 8. Must hold on 3 consecutive runs.
- **B2 Headed Chrome.** The `render` case passes in headed Chrome (zero WebGPU errors, hash `0x5b344e7d36adc269`, canvas ≥ 50% lit) on 3 consecutive runs. The report records the browser version and mode.
- **B3 Textured scene.** A scenario that loads a textured `.gmodel` runs to exit in the browser, using wasm pack decompression with a sha256 check (the item deferred from `wasm_quality_goal.md` row 44). Its world-state hash must be stable across 3 runs. Its frame must pass B1's thresholds against native.
- **B4 Boot cost.** The report records the browser boot time spent computing the shader-tree digest. Pass: ≤ 250 ms, or a fix that brings it under that.
- **B5 Gaps.** Each remaining item is either implemented or listed in the record as an accepted decision: `mappedAtCreation`, timestamp/occlusion queries, non-WGSL shader modules over the wire, and the fifo-only present mode. Fifo-only is expected to be accepted, since the browser exposes nothing else.
- **No regression.** In every report: native WebGPU still hashes `0x350203be5257f128`, the existing `host_gate.mjs` cases still pass 3/3, and native Vulkan/DX12 hashes are unchanged.

## Charter

**Authorized, no asking:**
- `Engine/Engine/Source/WebGpu/**` (including `Web/Wire.cpp`), `Gpu/Device/Web/**`, `Engine/cmake/wasm/**` (`host_gate.mjs`, `page/*.mjs`, the page), and the WGSL bake in `GpuRecord/Native/ShaderCompile.cpp` (`store_wgsl` stays where it is).
- Adding a resolution pin to `lighting_parity`, and new scenarios in `Sandbox/Sandbox/Source/Sandbox/Scenarios.cppm` for B3, provided existing scenarios' hashes do not move.
- Wasm-side asset code (`Os/Web/**`, the web arm of AssetFormat) for pack decompression and sha256. Vendoring a small decompressor is allowed if it builds under the wasm toolchain.
- Running headless and headed Chrome, node, native `gse_run` with `--engine-setting` overrides, and builds through `gse_build`.
- This record's ledger.

**Owner's call, escalate:**
- Any `.slang` edit, or anything that changes Vulkan/DX12 output or any native hash.
- Changing the wasm memory cap again, toolchain or GCC patches, or moving the snapshot pin.
- Relaxing B1's thresholds, or passing B1 by comparing at a resolution other than the pinned one.
- Turning Chrome's uniformity check back on or restructuring shaders to avoid it.
- `settings.ini`, any commit or push, and any work on native C4.

Suggested order: B1 (resolution pin + image diff), then B2, then B4, then B5, then B3, which is the largest.

Assumptions: B1's thresholds (≤ 2.0 mean, ≤ 1% over 8) are a guess. Browser and native run the same WGSL, so differences should be only rounding, and the gate is set tight. If the first measurement shows a systematic offset, the steering run escalates instead of loosening the thresholds. The B4 budget of 250 ms is also a guess.

## Ledger

(empty)
