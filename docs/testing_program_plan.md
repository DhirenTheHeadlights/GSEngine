# Testing program

Goal: every silent-wrongness class in the engine is caught by something that runs without anyone remembering to run it.

Two halves to that sentence. P0 closed the second one: `ci.yml` builds the engine and runs the suite on every push to `main`/`dev` and every pull request, a scenario run exits non-zero when anything in it went wrong, and `scenario::context` can read world state to form an expectation. What remains is coverage — the rows below — rather than plumbing.

This doc tracks what the program is converging on. [TESTING.md](TESTING.md) tracks what verifies **today** and how each landed check runs. Flip a marker here when something lands, and add its row there.

A row earns its place by naming the real failure mode it catches — a bug this repo has already paid for, or a named class from [TESTING.md](TESTING.md)'s "what is worth testing here". A row that cannot name one is not written. The hierarchy in that section still governs: a `static_assert` beats a contract, a contract beats a test, and one shared authority beats all three.

---

## Status

| Check | Tier | Status | Catches / note |
|---|---|---|---|
| `push_constant_layout_is_portable<T>` | Compile | ✅ Done | DX12 cbuffer straddle reading garbage where Vulkan scalar layout was fine. `PipelineBuilder.cppm:261` |
| `binding_args_layout_is_flat<Pack>` | Compile | ✅ Done | Bindless slot scramble from C++ padding shifting later binding indices. `PipelineBuilder.cppm:52` |
| Lossy integer unit construction | Compile | ✅ Done | Retired a runtime test outright; covers every call site that will ever exist |
| Semantic token contract | Compile | ✅ Done | Fails the editor build |
| Math contracts (`pre`, `contract_assert`) | Runtime | ✅ Done | `ignore` in Release — see the violation-tier gap below |
| Units, math, archive, bitstream, queues | Unit | ✅ Done | 21 tests; the queue ones are soak tests, worth `--engine-test-repeat` |
| Settings merge-on-save | Unit | ✅ Done | A foreign write to the ini does not lose the other writer's keys. `SaveRegistry.cppm:108` |
| SDK feed ordering + round trip | Unit | ✅ Done | Auto-update picking the wrong release |
| Math contract violation reporting | Violation | ✅ Done | 8 predicates, but has never actually run — Release reports them skipped |
| Scenario verdict plumbing | Scenario | ✅ Done | `bench_state::failures` is the verdict; `bench_exit_code` → `gse::start` → `main`. `Bench.cpp` |
| `scenario::context` expectations + world read | Scenario | ✅ Done | Checks shared with `test::context` through `test::check_sink`; registry exposed as const views. First user is `expect_bodies_sane` on the two pyramid scenarios |
| CI builds and runs the suite | CI | ✅ Done | `.github/workflows/ci.yml` on push and pull request, over the shared `gse-build-setup` composite action |
| `shader_struct` field layout | Compile | ❌ Missing | P1. std140-habit pad fields → DXIL "undefined values to UAV", invisible on Vulkan |
| Rollback pair hashes (`.compare_with`) | Scenario | ❌ Planned | P2. 6 reference/replay pairs exist, the hash prints, nothing compares it. Largest unguarded correctness property in the engine |
| Run-to-run determinism | Scenario | ❌ Planned | P2. `require` — same backend, same solver, identical hash |
| CPU/GPU positional parity | Scenario | ❌ Planned | P2. Tolerance, not hash identity. Retires the identity half of `parity_gate.py` |
| Cross-backend frame A/B | Scenario | ❌ Planned | P3. UI-black, AO banding, bindless scramble — all gross, all caught at a loose threshold |
| Trivial image invariants | Scenario | ❌ Planned | P3. Not uniformly black, no NaN/Inf, coverage in range |
| `id_of<T>()` agrees across modules | Unit | ❌ Missing | P4. Module-linkage statics not merging made cross-module components invisible (camera follows = 0) |
| Quantity parse ↔ format round trip | Unit | ❌ Missing | P4. Guards every `settings.ini` key and every scenario `settings` override; a mis-parse silently invalidates a benchmark |
| Pack self-containment | Unit | ❌ Missing | P4. Release run #2 died on baked resources absent from a clean runner |
| `profile_aggregator` self-time | Unit | ❌ Missing | P4. Misreporting here silently misdirects every perf decision |
| `shared_view` pointer-field decay | Unit | ❌ Missing | P4. Reassigned `unique_ptr`/`shared_ptr` `[[= shared]]` fields reading a stale snapshot |
| `layout_store` read-after-write | Unit | ❌ Missing | P4. `read()` returns your own last async write, not disk — already bit us once |
| State dump round trip + version reject | Unit | ❌ Missing | P5. A version mismatch misread instead of rejected |
| Archive across a schema change | Unit | ❌ Missing | P5. Extends the existing round trip: field added, removed, `archive_skip`ed, `skipped_fields()` reports it |
| `id` / `slot_map` generation reuse | Unit | ❌ Missing | P5. A stale handle that resolves is silent and arbitrarily delayed |
| Malformed JSON / source returns an error | Unit | ❌ Missing | P5. Parsers asserting or reading past the end on a user's truncated file |
| ECS scheduler graph | Unit | ❌ Missing | P5. Declared deps outranking derived (the cycle that used to assert); one writer per external resource |
| Hull / contact manifold properties | Unit | ❌ Missing | P5. Hull of a known point set, manifold symmetry under A/B swap, penetration sign |
| SIMD vs scalar agreement | Unit | ❌ Missing | P5. Two implementations of one function is the definition of silent divergence |
| Replication convergence (loopback) | Scenario | ❌ Planned | P6. Desync: both peers keep running, both look fine locally, a player finds out |
| Reliable channel + peer liveness | Unit | ❌ Missing | P6. Resend rebinds the sequence, a silent peer is reaped, goodbye tears the peer down |
| Settings scope split | Unit | ❌ Missing | P7. A `project_scope` field must land in the project file and everything else in the user ini. Merge is covered; the split is not |
| GUI interaction policy | Unit | ❌ Missing | P7. `clip_for(layer)` around `popup`, `dismissed_by_outside_press`. Symptom points at rendering, not input |
| `gui_scale` ratio round trip | Unit | ❌ Missing | P7. Position-ratio/design-size layout surviving a DPI change |
| Asset `engine:` namespace shadowing | Unit | ❌ Missing | P7. Reserved namespace must reject a shadowing asset |
| Asset compiler determinism | Unit | ❌ Missing | P7. Nondeterministic font/texture output silently busts caches and pack hashes |
| Missing asset returns an error | Unit | ❌ Missing | P7. A stale orphan `.gfont` tripping `gse::find`'s assert instead of failing the load |
| `parse_args` exit paths | Unit | ❌ Missing | P7. Mostly earns its place by exercising the out-of-process runner without contracts enabled |
| HTTP failure paths | Unit | ❌ Missing | P7. 404, timeout, truncated body must return an error — the auto-update launcher rides on this |
| `file_watcher` debounce | Unit | ❌ Missing | P7. Coalescing a burst of writes into one reload |
| Violation tier actually running | Violation | ❌ Missing | Needs a RelWithDebInfo suite run; contracts are `ignore` in Release, which is both what the editor's build row produces and the preset CI builds |
| Video encode → decode round trip | Unit | ❌ Deferred | No decoder exists in-engine (encode + mux only). Nearly free once one lands for its own reasons |
| Golden reference images in-repo | — | ❌ Deferred | Rots on every legitimate shader improvement, binary churn, and the update ritual trains you to accept diffs |

---

## Priority order

Top to bottom = do first to last.

1. ~~**P0 — verdict plumbing and CI.**~~ Landed. Everything else was a log line until it did.
2. **P1 — the `shader_struct` layout assert.** Small, independent of P0, and kills a cross-backend class permanently.
3. **P2 — scenario comparison gates.** One mechanism (`.compare_with`), three users: rollback pairs, determinism, parity.
4. **P3 — frame gates.** Reuses P2's comparison plumbing and the capture path that already exists.
5. **P4 — the unit cluster that maps onto bugs already paid for.** Cheapest value in the program after P0.
6. **P5 — silent and found late.** Correctness properties with no observable symptom at runtime.
7. **P6 — network.** Reuses P2's hash comparison.
8. **P7 — cheap and narrow.** Keep only while the cost stays low.

---

## P0 — verdict plumbing and CI — landed

See [TESTING.md](TESTING.md)'s "Verdict plumbing" for what the mechanism is. Three notes worth keeping, because they changed what the plan said:

- The CI extraction is a **composite action** (`.github/actions/gse-build-setup`), not a reusable workflow. A `workflow_call` workflow runs as its own job, and `release.yml`'s packaging steps need the build tree the setup produces, so the steps have to run in the caller's job. The toolchain-release lookup moved into the action so both workflows share one copy.
- CI builds the Release preset, so the **violation tier still reports skipped** there. A RelWithDebInfo configuration doubles runner cost for one tier; that row stays open deliberately.
- **`wait_until(ctx, predicate)` was not written.** The world read makes it possible, but nothing needs a predicate wait until the P2 gates do, and API with no caller is worse than API that arrives with one. It belongs to P2.

The queue soak tests run under `--engine-test-repeat 100` in CI: passing once says much less than passing a hundred times.

Two headless scenarios are gated (`pyramid_cpu`, `rollback_reference`). Whether more of the headless set is useful on a hosted runner is untested; the renderer ones are the open question `release.yml`'s `verify_game` exists to answer, and P3 below covers it.

## P1 — `shader_struct` field layout

The third of the codegen family, and the only one missing. Copy the idiom exactly: a `consteval` predicate beside `push_constant_layout_is_portable` in `gse.gpu:shader_codegen`, asserted from the wrapper type that every buffer-resident shader struct passes through, with a message that names the remedy rather than the rule.

Scalar layout on Vulkan and natural packing on DXIL both mean these structs need **no** manual padding. A std140-habit pad field makes the two disagree, and the symptom is DXIL writing undefined values to a UAV while Vulkan looks perfect.

## P2 — scenario comparison gates

`scenario::info` gains `.compare_with = "<scenario name>"`: run the named scenario too and compare. `wait_until(ctx, predicate)` belongs here too, over the world read P0 landed.

What may be required, and what may only be reported, is settled empirically by `Tools/parity_gate.py` and must not be re-litigated:

- **Same backend, same solver, run twice** — world-state hash identity, `require`.
- **Rollback reference vs replay** — hash identity, `require`. This is the pass condition that already exists and is never checked.
- **CPU vs GPU** — hash identity `report` only; the real check is positional with a tolerance (`parity_gate.py` uses 1e-5). Never assert cross-backend or cross-solver bit equality.

`parity_gate.py` keeps its positional and cascade checks until those move into scenarios as `expect_near` over state records.

## P3 — frame gates

Two checks on a **hermetic frame**: pinned frame index, adaptation forced instant, AO `frame_phase` fixed, cloud and sun time fixed. Without that the same backend disagrees with itself and the gate is noise.

- **Cross-backend A/B.** Same scenario and frame index, Vulkan against DX12, compared in-run at a loose threshold (mean absolute error over a few percent). Loose is the design, not a compromise: driver FP, filtering precision and RT traversal order differ legitimately, while every DX12 bug this repo has actually hit was gross — whole-image black, wrong texture, visible banding. A tight threshold would be permanently red and catch nothing extra.
- **Trivial invariants**, which need no second backend and would have caught UI-black and the DXIL UAV bug on their own: the frame is not uniformly black, contains no NaN or Inf, and its coverage is within range.

The capture path is largely plumbed: `CaptureRenderer` does GPU readback into `image::write_png` (`CaptureRenderer.cpp:155`) off a `screenshot_request` channel push, and `scenario::context::channels()` is a channel writer.

Where this gate runs is an open question, and `release.yml` already documents it: its `verify_game` input is staged off because `render_stress` needs a window and the headless scenarios never touch shaders, so there is no headless predicate that exercises the renderer. Either this gate runs on the owner's machine while the headless tiers run in CI, or it needs the DX12 backend's WARP software rasteriser to be good enough on a hosted runner — which `verify_game` exists to find out.

## P4 — bugs already paid for

Each row names a defect this repo has shipped. Cheapest value in the program.

## P5 — silent and found late

Correctness properties with no runtime symptom. The hull and manifold properties are the standout: collision wrongness is arbitrary, delayed, and blamed on the solver.

## P6 — network

Loopback convergence: server and client world-state hashes agree after N frames, reusing P2's comparison. [TESTING.md](TESTING.md) lists `net_walk_*` under "deliberately not tested"; this program overrides that for convergence specifically, because desync is the textbook silent failure — both peers keep running and look fine locally.

## P7 — cheap and narrow

Keep each only while it stays cheap. Tests cost build time on a tree that is already critical-path bound, so fewer and sharper beats thorough.

---

## Non-goals

123 of the engine's 385 module interface units get nothing, deliberately, across the groups below. Running the game is already a dense test of every reachable path with an observable symptom.

- **`gse.vulkan`, `gse.dx12`** (22 headers). Only meaningfully testable against a live device. The P3 frame gate covers the divergence that matters; backend unit tests would not.
- **`gse.gpu_backend`, `gse.gpu_record`** (26 headers). Thin typed surfaces over the backends. A break is a link error or a first-frame failure. The exception is the codegen family, which is compile-tier and already listed.
- **GUI widgets** (24 headers under `Gui/Widgets`: Button, Slider, Tree, TextInput, TabStrip, …). The editor repaints these continuously. Only the interaction policy and scale math are listed, and only because their symptoms misdirect.
- **All `gse.external*` wrappers** (15 headers: freetype, libpng, libjpeg, slang, moodycamel, directx, win32, sockets, cxxabi, aftermath, nsight_perf, miniaudio, msdfgen, …). Vendor surface; breaks are link-time.
- **Trivial `gse.core` and `gse.containers`** (8 headers: misc, flags, percentage, scope_exit, non_copyable, n_buffer, linear_vector, type_pack). Trivial or already compile-time.
- **`gse.time`, `gse.os`, `gse.audio`, `gse.http`, `gse.graph`, `gse.diag:watchdog`, `gse.devtools`, `gse.examples`** (28 headers). Every launch exercises them loudly. Two conditional exceptions: a keybinding serialize round trip, and the `loop_cadence` reactive/continuous policy against a fake clock once the power-efficiency work lands.
- **`gse.test`, `gse.scenario`.** Self-exercised by the suite running at all.
- **`parse_args` flag naming, common lexer token kinds, further math identities, Sandbox gameplay.** Carried over from [TESTING.md](TESTING.md); unchanged.
- **Anything a `static_assert`, a contract, or one shared authority can state instead.** A helper that makes the defect unwritable beats a test that detects it.
