# Pre-goal baseline: `lighting_parity`

Captured 2026-10-28 with `gse_run scenario=lighting_parity` from the editor (attached mode), on the build that added the scenario.

## World-state hashes

| Run | Requested backend | Backend actually used | Adapter | Hash |
|---|---|---|---|---|
| 20261028_232555 | default | dx12 | 0x8086 | `0x51dde0fb01aecaea` |
| 20261028_232849 | default | dx12 | 0x8086 | `0xe594e88bde0cd6f9` |
| 20261028_233534 | default | dx12 | 0x8086 | `0x51dde0fb01aecaea` |
| 20261028_233717 | default | dx12 | 0x8086 | `0x51dde0fb01aecaea` |
| 20261028_233854 | `Graphics.backend=dx12` | dx12 | 0x8086 | `0x51dde0fb01aecaea` |
| 20261028_234028 | `Graphics.backend=dx12` | dx12 | 0x8086 | `0x51dde0fb01aecaea` |
| 20261028_234205 | `Graphics.backend=dx12` | dx12 | 0x8086 | `0x51dde0fb01aecaea` |

Run 20261028_232849 came from a binary that differed only by a trailing `wait_frames(ctx, 1)` in the scenario body. Every later run, on the same binary, hashes `0x51dde0fb01aecaea`, so the stable hash is that one. The single divergent value has not been explained. It was also the run with a 302 ms p50, against 5–9 ms for the others.

## What this baseline does not have yet

- **No Vulkan run.** Attached runs present into the editor's surface ring (`attached: created 3-surface ring ... on dx12`), so they run on the editor's backend whatever `Graphics.backend` is set to. A Vulkan baseline needs a standalone run, or an editor running on Vulkan.
- **A usable tolerance.** Frames now save in attached runs (the request was dropped whenever the editor's surface ring had no free slot on the one frame it was visible). Three runs on 2026-10-29 (`screenshot_20261029_000412.png`, `_000534.png`, `_001640.png`, all hash `0xe594e88bde0cd6f9`) differ from one another far more than the placeholder tolerance allows:

  | Pair | Max | Mean | Over 8 | PSNR |
  |---|---|---|---|---|
  | 000412 vs 000534 | 85 | 6.66 | 12.1% | 28.75 dB |
  | 000412 vs 001640 | 90 | 3.79 | 10.5% | 30.26 dB |
  | 000534 vs 001640 | 83 | 4.87 | 10.9% | 29.72 dB |

  A tolerance wide enough to pass these would pass most real regressions, so the constants in `Tools/frame_parity.py` were left as they are. The frame that gets captured has to become deterministic first. The likely sources, none of them confirmed yet: wall-clock-driven content (sky, clouds), temporal accumulation (TAA, RT history) depending on how many frames actually rendered, and the capture landing on whichever frame first finds a free surface slot.

The hash moved from `0x51dde0fb01aecaea` to `0xe594e88bde0cd6f9` between the runs above and 2026-10-29. A build of the tree with the capture fix reverted also hashes `0xe594e88bde0cd6f9`, so the change came from other edits in the tree, not from the fix.

Compare frames with `python -I Tools/frame_parity.py diff a.png b.png` or `noise run1.png run2.png run3.png --report out.json`.

## 2026-10-30: editor-gated frame rate and the temporal reset

**Editor gate (attached runs).** The game can be at most one frame ahead of the newest frame the editor has drawn. The editor only signals `consumed` from its own graphics submit (`Editor/Editor/Source/Viewport/Viewport.cpp`, releasing `produced - 2`), and `render_graph::output_ready` checks that value. So in the editor the game's displayed fps is capped by the editor's frame rate. When a slot is not free, the game runs update and skips render. Measured attached before this change: `lighting_parity` p50 92 ms with 191 of 480 frames never rendered; a plain run spun ~1,400 loop iterations/s and handed only 20–36 frames/s to the editor. Standalone has not been measured, because `gse_run` always attaches. Proposed fix (not applied, it changes attached-mode defaults on both sides): mailbox handoff, where the editor publishes the slot it is displaying and the game overwrites the oldest one that is not being displayed.

**Scenario lockstep.** Under a fixed-step bench, once the world has settled, the engine now waits up to 2 s on `consumed` instead of skipping render. The `attached surface:` line now reports the time spent blocked on the editor (0.5–1.5 s per 120 frames on this machine). Post-settle skips went from about 45% to 0.

**Temporal reset.** `lighting_parity` pushes `camera::temporal_reset_request` after spawning the lights. On that request the camera jitter index, TAA history, the forward / gi_probe / cloud frame counters and auto-exposure (snapped to target) all reset.

Four DX12 runs (`screenshot_20261030_161014`, `_161255`, `_161422`, `_161603`):

| Pair | Hash | Max | Mean | Over 8 | PSNR |
|---|---|---|---|---|---|
| 161014 vs 161255 | e594 / e594 | 66 | 1.91 | 10.3% | 31.11 dB |
| 161014 vs 161603 | e594 / e594 | 61 | 1.89 | 10.0% | 31.06 dB |
| 161255 vs 161603 | e594 / e594 | 66 | 1.88 | 10.2% | 31.17 dB |
| any vs 161422 | e594 / **51dd** | 186–189 | 9.2 | 36% | 23.1 dB |

The mean error halved and is now very consistent between pairs, but about 10% of pixels still differ by more than 8, so the frame is not deterministic yet. The tolerance constants were left unchanged. Remaining sources, not yet confirmed:
- accumulated history that the reset does not clear: the GI irradiance atlas and the cloud reprojection history
- stochastic RT shadow/AO noise
- GPU ordering in light culling and OIT

Run 161422 hashed `0x51dde0fb01aecaea`, the other known value. The world-state hash still flips between two values from run to run. That is a simulation divergence, not a rendering one, and it predates this change.

## 2026-10-31: milestone 1, raster lighting baseline (DX12)

`raster/lighting_parity_raster_dx12.png` is the raster-path baseline: `gse_run scenario=lighting_parity` with `--engine-setting Graphics.lighting_technique=raster` and validation layers on. Three runs (`screenshot_20261031_180814`, `_180929`, `_181030`) all hash `0x3e7d6aee4a690d14` and agree within the `frame_parity.py` tolerance (max 1, mean 0.0000, 0 px over 8, 106–108 dB; `raster_noise.json`). No `dx12_validation` errors; the only new warnings are clear-value performance hints for the shadow atlases and probe capture targets. No RT shadow system or TLAS ran, and no RayQuery pipeline was built.

After the seam refactor, the ray-traced default still hashes `0x3e7d6aee4a690d14`, and its frame matches the pre-change baseline `screenshot_20261031_171014` on three runs (`_175425`, `_180403`, `_181218`: max 1, mean 0.0000, 0 px over 8).

Raster vs ray-traced on the same scene: max 68, mean 0.162, 0.92% of pixels over 8, 43.7 dB. The differences are softer local-light shadows and a few unshadowed distant point lights, because the local tile budget covers the ~10 nearest lights.
