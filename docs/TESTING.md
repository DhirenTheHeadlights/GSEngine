# Testing

The master record of what GSE verifies, how each check runs, and where the gaps are. Update the status table when a check is added, wired or retired. When a planned item lands, delete its plan text and keep only the row.

## Status

| Tier | Check | Where | How it runs | Verdict | Automated |
|---|---|---|---|---|---|
| Compile | `static_assert` blocks | `Math/Units/Units.cppm:468`, `Ecs/SystemManifest.cppm`, `Graphics/AssetTypes.cppm`, `Ecs/SharedView.cppm`, `Json/Value.cppm`, `Syntax/Lexer.cppm` and others | every compile | compile error | yes |
| Compile | Lossy integer unit construction | `Math/Units/Quantity.cppm`, `internal::unit::operator()` | every compile | `static_assert`; names the remedy | yes |
| Compile | Semantic token contract | `Editor/Tests/SemanticContract.cpp`, `Editor/Tests/CheckSemanticContract.cmake` | custom command on `gse_tokens_plugin` | build error | yes, every editor build |
| Runtime | Math contracts (`pre`, `contract_assert`) | `gse.math`: vec/mat indexing, inverse, asin/acos, sqrt, fmod, look_at, perspective, orthographic, SIMD spans, rect/circle | Debug and RelWithDebInfo; `ignore` in Release | `assert_fail` exits 3 | only when a run hits one |
| Runtime | `gse::assert` | everywhere | any run | logs `[Assertion Failure]`, exits 3 | only when a run hits one |
| Unit | Units, vector/matrix/quaternion math, binary archive, bitstream and packet sequencing, ring buffers and work stealing | `Math/Tests/Units.cppm`, `Math/Tests/Math.cppm`, `Containers/Tests/Archive.cppm`, `Network/Tests/Net.cppm`, `Concurrency/Tests/Concurrency.cppm` | `Sandbox.exe --engine-test-all`, `--engine-test-tags units,math,archive,net,concurrency` | non-zero exit, `[error]` lines under the `test` category | yes, once wired to CI |
| Violation | Math `pre` / `contract_assert` reporting | `Math/Tests/Contracts.cppm` | the same run re-launches the exe with `--engine-test-only <name>` per test | passes only when the child exits 3 and its report names the predicate | yes, once wired to CI |
| Scenario | 60 annotated scenarios | `Sandbox/Source/Sandbox/Scenarios.cppm` | `Sandbox.exe --engine-bench-scenario <name>` | logs p50/p95/p99 and a world-state hash | no |
| Scenario | Perf baseline | `Runtime/Bench.cpp:147` | `--engine-bench-update-baseline`, then a normal run | logs `REGRESSED` | no; the process still exits 0 |
| Gate | CPU/GPU parity + determinism | `scripts/parity_gate.py` | `python scripts/parity_gate.py [--backend both]` | exits 1 on a `require` failure | no |
| Gate | Stress invariants | `scripts/stress_battery.py` | manual, on state dumps | exits 1 on a jointed failure | no |
| Data | State dump scan/compare | `Startup/StateDumpTools.cpp` | `--scan-states`, `--compare-states-a/-b` | prints the divergence | no |
| Data | Divergence sweep | `scripts/divergence_sweep.py` | manual | report only | no |
| Tool | Semantic audit | `Editor/Source/Tools/SemanticAudit.cpp` | `SemanticAudit --db ... --plugin ...` | exits 1 on failure | no |
| Tool | Time report self-test | `Tools/TimeReport/time_report_check.py` | manual | exits 1 on failure | no |
| CI | GCC trunk toolchain | `.github/workflows/build-gcc-trunk.yml` | weekly | toolchain only, never builds the engine | yes |

## Gaps

- **No CI runs the tests.** Nothing invokes the suite, so a red test is only seen by whoever thinks to run it. This is the single change that would make every row below worth more.
- **The unit tier covers `gse.math`, the binary archive, the network bitstream and the concurrent queues.** Nothing in the priority list below is written yet.
- **The queue tests are soak tests, not proofs.** A publish-before-write or a lost steal is detected only if the interleaving happens to occur, so they are worth running under `--engine-test-repeat`; passing once says less than passing a hundred times.
- **The violation tier has never run.** Contracts are `ignore` in Release, which is the configuration the editor's build row produces, so all eight report as skipped. They need a RelWithDebInfo run to mean anything.
- **Scenarios can neither fail nor observe.** `main` returns 0 after a baseline regression, a settle-cap abort or a stale coroutine, and `scenario::context` exposes only `frame()` and `channels()`, so a scenario can push input but cannot read world state. See Verdict plumbing.
- **Rollback pairs are never compared.** For each `rollback_reference` / `rollback_replay` pair, a matching hash is the pass condition, but nothing checks it. This is priority A item 1.
- **Most math contracts still have no violation test.** `Contracts.cppm` covers eight predicates. `acos`, `orthographic`, `epsilon_equal_index`, `rotate`, the sixteen SIMD span contracts and `Circle.cppm` are untested. `rect_t`'s unconditionally-true `contract_assert` was deleted rather than tested.
- **Stale docs.** `scenario_authoring.md:143,208` says a tripped assert hangs. It now exits 3.

## Framework: `gse.test`

Built. `Engine/Engine/Source/Test/Test.cppm` plus `Runner.cpp`. It reuses what the engine already does: registration by reflection over annotated functions (the same shape as `gse.scenario` and the system manifest), argument parsing through `parse_args`, and failure reporting through `gse.log` under the `test` category. There are no macros and no DSL. A test is a plain function taking `test::context&` and returning `void`, declared in a `Tests/` folder beside the code it covers. The test name comes from `identifier_of`, so it is never typed twice.

- `test::unit{ .tags, .needs_gpu, .isolated }` marks an in-process test; `test::violation{ .expect }` marks one that must end in a contract or `gse::assert` failure whose comment contains `.expect`.
- Checks return `bool` so a test can stop early with `if (!ctx.expect(...)) return;`: `expect`, `expect_eq`, `expect_near`, `expect_value`, `expect_error`. Each is constrained on `std::formattable`, so quantities, vectors and matrices print through their own formatters and `expect_near` never strips units.
- `test::registry<^^Ns>()` sweeps a namespace tree with `members_of`. There is no static-init registration, and the sweep must happen in the executable's own TU.
- `test::run({ .tables, .options, .flag_prefix })` returns 0 on a pass and 1 on any failure. `test::config` is reflected into flags by `parse_args`: `filter`, `tags`, `only`, `list`, `repeat`.
- A violation test and an `.isolated` test run out of process: the runner re-launches its own executable with `<prefix>-only <name>` through `gse::process::run_capture` and reads the child's captured output. A violation passes when the child exits 3 and that output names the predicate. In Release, where contracts are `ignore`, violation tests report as skipped.
- A `.needs_gpu` test reports as skipped; nothing hands the runner a device yet.

`seed` is not implemented. Nothing randomises yet, so there is nothing for it to seed.

### Hooks

`test::config` is a nested member of `engine_config`, so every executable that parses it gets `--engine-test-*` for free. `main` calls `test::run` and returns its code before the engine starts a window whenever `test::requested(config)` is true. Engine test modules are gathered by the `gse.tests` umbrella, so a consumer imports one module and sweeps `^^gse::tests`.

`GSETests` exists for the same sweep with no game attached, but the editor's build row only ever builds the game target, so nothing an agent can invoke compiles it. Until the build row can name it, `Sandbox.exe --engine-test-all` is the way the suite actually runs.

| Executable | Sweeps | Invoked as | State |
|---|---|---|---|
| `Sandbox.exe` | `^^gse::tests` | `Sandbox.exe --engine-test-all`, `--engine-test-filter units` | built, and the only one the editor's build row produces |
| `GSETests` | `^^gse::tests` | `GSETests --test-all` | target exists, never built |
| `Editor.exe` | `^^gse::tests`, `^^gse::editor::tests` | `Editor.exe --engine-test-filter lint` | not wired |


## What is worth testing here

Running the game is already a dense test of every reachable path with an observable symptom, and that covers most defects. A test earns its place only by reaching something the game does not:

- **Silent wrongness.** The run succeeds and a value is quietly wrong. Determinism divergence, a mis-scaled conversion, a serialized field that reads back as something else.
- **Counters that need years to wrap.** The ack bitfield desync needs 2^32 packets. No amount of playing reaches it.
- **Failure paths.** The happy path runs every launch and the error path never, until a user's file is truncated or their JSON is malformed.
- **Hostile interleavings.** A frame picks one thread ordering. It does not pick the adversarial one.

Before writing a test, check whether something stronger can state the same rule, because a test only fires when someone runs it:

1. **Compile time.** The lossy-integer `static_assert` in `unit::operator()` retired a runtime test outright: it covers every call site that will ever exist and cannot be skipped. Prefer this always.
2. **A contract.** `pre` and `contract_assert` check on every real run, including the game, not only under the runner.
3. **One shared authority.** `sequence_more_recent` removed a whole bug class by leaving only one place where sequences are compared. A helper that makes the defect unwritable beats a test that detects it.
4. **A test**, when none of the above can express it.

Tests also cost build time on a tree that is already critical-path bound, so fewer and sharper beats thorough.

## Priority list

### A: silent, and found late

1. **Rollback pairs** (scenario tier, `.compare_with`). Each `rollback_replay*` must match its reference hash. The pass condition already exists and nothing checks it, which makes this the largest unguarded correctness property in the engine.
2. **CPU/GPU identity and run-to-run determinism** (scenario tier). The same scenario twice, and CPU against GPU, must produce one world-state hash. Retires the identity half of `parity_gate.py`.
3. **State dump** (`Runtime/StateDump`). Write/read round trip, and a version mismatch is rejected rather than misread.
4. **Settings scope split** (`Save`). A `project_scope` field lands in the project file and everything else in the user ini, both round-tripping against a temp root. Gets this wrong and a user's project silently loses settings.
5. **Archive across a schema change** (`Containers/Archive`). Extends the existing round trip: a field added, removed and `archive_skip`ed still reads, and `skipped_fields()` reports it.

### B: unreachable by playing

1. **ID and slot map** (`Core`, `Containers`). Generation reuse and stale-handle rejection. A stale handle that resolves is silent and arbitrarily delayed.
2. **Reliable channel and peer liveness** (`Network`, loopback). A resend rebinds the sequence, a silent peer is reaped, a goodbye tears the peer down.
3. **Malformed input returns an error** (`Json`, `Syntax`). Parsers must reject bad input rather than assert or read past the end.
4. **Scheduler graph** (`Ecs`). Declared dependencies outrank derived ones — the cycle that used to assert — and an external resource has exactly one writer.

### C: cheap, narrow, keep if the cost stays low

1. **GUI interaction policy** (engine GUI, headless `draw_context`). `clip_for(layer)` above and below `popup`, and `dismissed_by_outside_press` with `keep_open` and `suppressed`. Worth it despite the game exercising these, because the symptom of getting them wrong points at rendering rather than input.
2. **`parse_args` exit paths only** (`Meta/Args`, isolated tier). Unknown flag and bad value exit 2. Kept mostly because they exercise the out-of-process runner without needing contracts enabled.
3. **Build output parsing** (`Editor/BuildRunner`). SARIF display columns versus byte columns.
4. **Markdown formatter idempotence** (`Editor`). `format(format(x))` equals `format(x)`.
5. **Lint rules on fixture sources** (`Editor/Lint`).

### Deliberately not tested

- **`parse_args` flag naming, `--no-` negation, nested groups.** Every editor and sandbox launch parses them; a break is immediate and loud.
- **Common lexer token kinds.** The editor repaints them continuously.
- **Further math identities.** Anything the renderer would scream about on frame one is already covered by running it. The existing math tests stay; no more are worth adding.
- **Sandbox gameplay and `net_walk_*` with expectations.** The game is its own harness for these.
- **Anything a `static_assert`, a contract, or a shared helper can state instead.** See the hierarchy above.

## Verdict plumbing

Independent of which tests exist, the checks below report into a log instead of failing a run. Fixing that is worth more than any new test, because an unread verdict is not a check:

1. Scenarios cannot fail. `finish_bench` returns a status, and `main` returns non-zero on a failed expectation, a settle-cap abort or a baseline regression.
2. `scenario::context` gains the `expect` family and read-only world access, which also makes `wait_until(ctx, predicate)` possible. Priority A items 1 and 2 cannot be written until this exists.
3. `scenario::info` gains `.compare_with = "rollback_reference"`, running the paired scenario and requiring equal world-state hashes. `parity_gate.py` keeps its positional and cascade checks until those move into scenarios as `expect_near` on state records.
4. A build-and-test CI workflow, so a red test is seen without someone remembering to look.
