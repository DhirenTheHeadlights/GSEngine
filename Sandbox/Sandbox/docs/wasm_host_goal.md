# Goal: a real browser host layer for the engine

## Aim

The headless goal (`Sandbox/docs/wasm_browser_goal.md`) is **met**, and the quality goal (`Sandbox/docs/wasm_quality_goal.md`) is **met**. So the engine ticks in a page, but only as a batch job. The page has no canvas. `Os/Web/Window.cpp` is all stubs: `viewport` is `{0,0}`, `minimized` is always true, and `tick`/`wait_events` do nothing. No input reaches the engine. Every write goes to the host worker's in-memory map (`page/host.mjs`), so nothing survives a reload.

This goal makes the page a real host, so a **non-headless** run can do three things in a browser. It can own a window (a canvas). It can receive keyboard, mouse, wheel, text and pointer-lock input through the same `surface.input_events` queue the Win32 arm feeds (`Os/Win32/Window.cpp:604-916`). It can keep its writable roots (settings, saves) across reloads.

Rendering is **out**. WebGPU, the device arm, and pack decompress/sha256 (deferred by the owner in quality-goal row 44) belong to the GPU goal. That goal builds on this one, because it needs this canvas and resize path. Runs here use `--no-engine-gpu`.

## Pass criteria

A steering run decides from artifacts written by a node gate. The gate is `Engine/cmake/wasm/host_gate.mjs`, emitted by the wasm preset. Per the owner's rule, any Python is only an optional wrapper. The gate drives headless Chrome over CDP (`serve.mjs` already launches it).

1. **Window.** With a new scenario `web_host_probe` (not headless, `--no-engine-gpu`), the engine log reports `viewport` equal to the canvas backing size (CSS size × devicePixelRatio). After the gate calls CDP `Emulation.setDeviceMetricsOverride` to a second size, the log shows a resize and the new viewport within 2 frames. After `Emulation.setFocusEmulationEnabled` / a visibility change, `minimized` follows `document.visibilityState`. Required.
2. **Input.** The gate injects a fixed script through CDP `Input.dispatchKeyEvent` / `dispatchMouseEvent`: every key in `gse.os:keys`, the mouse buttons, moves, both wheel axes and a UTF-16 surrogate-pair text entry. The scenario logs each `input::*` event it receives. The logged sequence must equal the expected sequence exactly, in order, with mouse y in the same flipped convention as Win32 (`height - y`). Required.
3. **Pointer lock.** A captured-cursor request produces `mouse_raw_moved` deltas under CDP-injected motion. This is `report` only, because pointer lock in headless Chrome may not be grantable. If it is not, the gate records why.
4. **Persistence.** Run A writes a settings value and a save file. The page then reloads (same origin, fresh workers). Run B reads both back, byte-identical. Storage is OPFS behind the existing WASI `path_*`/`fd_*` calls in `host.mjs`, scoped to the writable roots, so `/fs` assets stay fetched read-only. Required.
5. **No regression, held over 2 consecutive steering checks.** Native `gse_build` is green. The native `scene_reset_cpu` hash stays `0xe836d31697879e27`. `wasm_parity_gate.py` is still 3/3 with wasm `0x7f70bb7d195a6d58`, final drift ≤ 2 mm.
6. Criteria 1, 2 and 4 hold on 3 consecutive gate runs. The gate writes `report.md` under the build's output, so a fresh session can re-check without reconstructing the invocation.

## Charter

**Authorized, no asking:**
- `Engine/Engine/Source/Os/Web/**`, plus new `Web/` impl units for `gse.os` partitions that already have a neutral interface.
- `Engine/cmake/wasm/**`: the page, host and guest workers, the protocol, `serve.mjs`, the new `host_gate.mjs`, and `host_imports.txt` entries for new `gse_host_*` imports.
- How events cross from page to guest. The recommended shape is a shared-memory ring that the page's DOM listeners fill and `window::tick` drains on the engine thread, with the canvas handed to the guest as an `OffscreenCanvas`. Steering may change the mechanism if a measurement says so.
- New scenarios in `Sandbox/Sandbox/Source/Sandbox/Scenarios.cppm` per `docs/scenario_authoring.md`.
- OPFS-backed writable roots in `host.mjs`.
- `gse_build` (both presets), `gse_run`, the parity gate and the new gate, plus `--engine-setting` overrides.
- Running headless and headed Chrome from the gate.

**Owner's call, escalate:**
- Any change to a `gse.os` **interface** partition (`Win32/Window.cppm`, `:input*`, `:keys`, `:clipboard`) or to native behaviour. The web arm must fit the existing API. If it cannot, that is a design question.
- Anything in `Gpu/`, `Vulkan/`, `Dx12/`, WebGPU or rendering.
- Clipboard (the async Clipboard API is not reachable synchronously from a worker), audio, network.
- Exceptions/setjmp work, and the held image decode.
- New modules, or changes to the arm-selection mechanism.
- Commits and pushes. The owner commits.
- Reverting previously-working code to chase a new bug.

**Off limits:** `settings.ini`. Invoking cmake/ninja/compilers directly.

## The work, in the order the dependencies imply

1. Read `:window`'s full contract (`Win32/Window.cppm:164-335`) and what a non-headless `--no-engine-gpu` boot demands of it. Measure this with a `gse_run` of the current wasm build before designing.
2. Canvas and resize: page `ResizeObserver` → shared state → `viewport`, `frame_buffer_resized` and `minimized`.
3. The input ring, plus the DOM `code` → `gse` key map, generated from `:keys` rather than hand-listed.
4. `wait_events`/`post_wake` on `Atomics.wait`/`notify`, so a reactive loop sleeps in the browser as it does natively.
5. OPFS persistence.
6. Pointer lock.
7. `host_gate.mjs` and `web_host_probe`.

## Assumptions

- **Scoping could not run anything.** `viewport {0,0}` / `minimized true` are read from source, not observed. Step 1 measures them.
- **A non-headless `--no-engine-gpu` boot might still require a surface from `gse.gpu`.** If it does, that coupling is escalated, not stubbed in `Gpu/`.
- **CDP-injected events reach DOM listeners like real ones.** Chrome documents this for `Input.dispatch*`. Pointer lock is the known exception, which is why (3) is `report`.
- **OPFS sync access handles are only available in workers.** The host worker is one, so the WASI shim can stay synchronous. If OPFS is unavailable in the CI browser, IndexedDB with a preload is the fallback, recorded in the ledger.

## Ledger

- 2026-11-03 — steering #1. Prior phase only wrote the record; no code, build or run. No numbers. Verdict: nothing to judge; next task is work-order step 1 (measure a non-headless --no-engine-gpu wasm boot) plus step 2 (canvas + resize).
- 2026-11-03 — steering #2. Task 82 landed canvas + ResizeObserver → shared mem → `gse_host_surface`; Web Window reports viewport/minimized/framebuffer_resized; `web_host_probe` scenario added. Measured: viewport 754×480 = canvas; CDP resize to 1000×700 seen same frame. Non-headless --no-engine-gpu boot asserted (Engine.cpp:479) via scheduler starvation pass dropping `asset::data`; pass deleted with owner approval. Hashes moved: native scene_reset_cpu 0x51ceab5d23e54919 (ref 0xe836…), wasm 0xe5a535d76e5b2799 (ref 0x7f70…); parity 3/3, drift 0.016 mm — attributed (unproven) to another chat's physics work; re-baseline deferred. Open: canvas size from devicePixelContentBoxSize, not CSS×DPR (headless reports CSS-size box at DPR 2) — wording question for owner at gate time; visibility→minimized unexercised; width/height written non-atomically (gate must not count resize lines exactly). Verdict: criterion 1 partially demonstrated by hand, no gate yet; next is step 3 input ring + key map.
- 2026-11-03 — steering #3. Task (step 3) landed input ring (1024 records, page DOM listeners → `gse_host_input` drain in `Web/Window.cpp::tick`), compile-time DOM-code→key table from `:keys`, Win32-semantics push (flip under ui_focus, clamp, text filter), packed 64-bit canvas size (fixes torn resize read). Hand-verified in headless Chrome over CDP; both presets green. No gate numbers. Gate needs `Dev.global_level=debug` AND `Dev.general_level=debug`; F5 reloads page (skip in script); `world_1`/`f25`/`unknown` have no DOM code (criterion 2 covers rest of `:keys`); 60k+-frame wasm runs OOM in bench report (pre-existing, one "table index out of bounds" trap). Criterion 5 hashes not re-checked. Verdict: criterion 2 demonstrated by hand only; next is step 4 wait_events/post_wake.
- 2026-11-03 — steering #4. Task (step 4) landed `gse_host_wait`/`gse_host_wake` (Atomics.wait/notify on a shared wake word; page bumps it on input push, resize, visibilitychange); `web_idle_probe` scenario (80 frames). Scratch-CDP measured, reactive cadence, 3 runs: idle ~1–2.5 fps (vs ~60 continuous); post_wake→frame 0.8–2.1 ms; key 2–67 ms; resize/visibility same-ms in best run but 6 s / 1.2 s in one run (cause unconfirmed, suspected headless Chrome delivery). Both presets green; criterion 5 hashes unchanged from #2 (still off the record's refs). Verdict: step 4 works by hand; outlier must be re-measured by the gate; next is step 5 OPFS persistence.
- 2026-11-03 — steering #5. Task (step 5) landed OPFS persistence (page main thread loads stored tree, hands to host worker; IndexedDB fallback, `none` otherwise); flush on close/fd_sync/exit, dir ops immediate; `web_persist_write`→reload→`web_persist_read` scenarios (ini token + 8269-byte all-byte-values save). One hand run in headless Chrome: match, exit 0; fresh profile read fails exit 1. Both presets green; criterion 5 hashes unchanged from #2 (still off refs). Open: killed tab loses unflushed write; IndexedDB fallback never exercised; host.mjs edited via script not Edit. Verdict: criterion 4 demonstrated by hand only; next is step 6 pointer lock.
- 2026-11-03 — steering #6. Task (step 6) landed pointer lock: capture → canvas `requestPointerLock`, locked motion → `mouse_raw_moved` (DPR-scaled), capture state follows actual lock (Win32 rule); `web_pointer_probe` scenario (300 frames); window arm logs `cursor capture want … locked …`, page logs denials. One scratch-CDP hand run: boot request denied (needs gesture); after injected click granted; 5 injected (+10,+5) moves → exactly 5 `mouse_raw_moved 10 5`. Both presets green; criterion 5 hashes unchanged from #2 (still off refs). Open: Escape/relock untested, headed Chrome untried. Verdict: criterion 3 demonstrable (not just report) in headless; no gate yet; next is step 7 host_gate.mjs.
- 2026-11-03 — steering #7. Task (step 7) landed `host_gate.mjs` (window/input/persist/pointer cases, `<build>/host_gate/report.md`); page log box repaint throttled to rAF (was delaying resize 50 s); ShadowStep.cpp signature fix. 3 gate runs × 3 repeats (reported, report.md not re-run by steering): input 9/9 (253 events exact), persist 9/9, pointer granted+exact, window 7/9 — both failures resize seen at 3 frames vs 2-frame budget; third gate run all-pass. Wake outlier did not recur. Criterion 5 hashes unchanged since #2 (still off refs; owner re-baseline needed). Verdict: criteria 2,4 pass by gate; 1/6 fail on resize latency (possibly measurement: page line vs engine frame lines via different routes); next is engine-side frame counting of resize latency.
- 2026-11-03 — steering #8. Task landed engine-tick resize latency (gate mark → `window: tick T mark M`, page resize seq → `window: tick T resize seq S`). 3 gate runs × 3 repeats (reported): input 9/9, persist 9/9, pointer pass ×3, window 7/9; latencies −1,3,0,0,0,0,0,0,0 ticks. Failures: one 3-tick lag right after boot warmup (ResizeObserver delivered ~70 ms late; engine read it on arrival tick), one wasm trap `table index is out of bounds` in `scheduler::sync_wait_or_dump` (Ecs/, outside charter). 1 of 3 runs fully passing. Verdict: old latency was mostly measurement; criterion 6 blocked by out-of-charter trap and criterion 5 by owner re-baseline — escalate.
- 2026-11-03 — steering #9. No new run; owner answered #8's escalation "all are fine": (a) a task may investigate/fix the wasm `table index is out of bounds` trap in `scheduler::sync_wait_or_dump` in `Ecs/` (charter extended for this trap only); (b) criterion 5 references re-baselined to native `scene_reset_cpu` `0x51ceab5d23e54919`, wasm `0xe5a535d76e5b2799` (parity 3/3, drift ≤ 2 mm unchanged); (c) gate may wait for boot warmup to finish before the first resize; (d) criterion 1 expected viewport = canvas `devicePixelContentBoxSize`. Verdict: next task is the trap + warmup settle, then 3 consecutive gate runs.
- 2026-11-03 — steering #10. Task landed coroutine "finished" flag in `Concurrency/AsyncTask.cppm` (trap fix; outside the `Ecs/`-only extension — owner to note), gate warmup settle, criterion-1 expected = devicePixelContentBoxSize. Reported: 3×3 gate runs pass pre-review-fix; 1 pass post-fix; native hash `0x51ceab5d23e54919` = ref; parity not re-run post-fix. Steering launched 3 consecutive `host_gate.mjs --repeats 3` + parity gate on the final binary (output `.generated/steer10_summary.txt`, `steer10_gate{1,2,3}.txt`, `steer10_parity.txt`). Verdict: pending those runs.
- 2026-11-03 — steering #11. Steer10's detached runs died: only `steer10_gate1.txt` exists ("window-1 PASS", 14:08), host_gate dir `2026-11-03T20-08-21-035Z` holds just `window-1.log`; no summary, gates 2/3, or parity output; no node/chrome alive. Likely killed when the steering chat stopped — steering-launched background jobs do not survive the turn. No numbers. Verdict: criterion 5/6 unjudged; next task runs the 3 consecutive gate runs + parity + native scene_reset_cpu in-task and reports artifacts.
- 2026-11-03 — steering #12. Re-read task's artifacts myself: 3 consecutive `host_gate.mjs --repeats 3` runs (`host_gate/2026-11-04T01-45-28-499Z`, `…01-47-36-249Z`, `…01-49-11-381Z`) all PASS — window 9/9 (viewport 754×480 = devicePixelContentBoxSize, resize latency 0 ticks ×3, hidden/visible→minimized follows), input 9/9 (253 events exact), persist 9/9, pointer pass ×3. Parity report: 3/3, wasm `0xe5a535d76e5b2799`, native `0x51ceab5d23e54919`, drift 0.016 mm. Both presets green per task. Verdict: criteria 1–4 and 6 met; criterion 5 check 1 of 2 clean; AsyncTask.cppm trap fix (outside the `Ecs/` extension) still awaits owner acceptance. Next: second criterion-5 check.
- 2026-11-03 — steering #13. Read task's artifacts: `.generated/steer13_summary.txt` — both presets 0 errors; native scene_reset_cpu `0x51ceab5d23e54919` (ref), exit 0; parity report (19:55:55) 3/3, wasm `0xe5a535d76e5b2799` (ref), final drift 0.016 mm ×3. Verdict: criterion 5 clean on 2nd consecutive check; with #12, all criteria 1–6 met. Open for owner: accept AsyncTask.cppm trap fix (outside `Ecs/` extension).
