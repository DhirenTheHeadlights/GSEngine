# Testing

The master record of what GSE verifies, how each check runs, and where the gaps are. Update the status table when a check is added, wired or retired. When a planned item lands, delete its plan text and keep only the row.

## Status

| Tier | Check | Where | How it runs | Verdict | Automated |
|---|---|---|---|---|---|
| Compile | `static_assert` blocks | `Math/Units/Units.cppm:468`, `Ecs/SystemManifest.cppm`, `Graphics/AssetTypes.cppm`, `Ecs/SharedView.cppm`, `Json/Value.cppm`, `Syntax/Lexer.cppm` and others | every compile | compile error | yes |
| Compile | Semantic token contract | `Editor/Tests/SemanticContract.cpp`, `Editor/Tests/CheckSemanticContract.cmake` | custom command on `gse_tokens_plugin` | build error | yes, every editor build |
| Runtime | Math contracts (`pre`, `contract_assert`) | `gse.math`: vec/mat indexing, inverse, asin/acos, sqrt, fmod, look_at, perspective, orthographic, SIMD spans, rect/circle | Debug and RelWithDebInfo; `ignore` in Release | `assert_fail` exits 3 | only when a run hits one |
| Runtime | `gse::assert` | everywhere | any run | logs `[Assertion Failure]`, exits 3 | only when a run hits one |
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

- **No unit tier.** Nothing runs a function and compares its result. No `add_test` exists and no framework is vendored.
- **Scenarios cannot fail.** `main` returns 0 after a baseline regression, a settle-cap abort or a stale coroutine. These only log at error level.
- **Scenarios cannot observe.** `scenario::context` exposes `frame()` and `channels()`, so a scenario can push input but cannot read world state. `wait_until` is not built.
- **Rollback pairs are never compared.** For each `rollback_reference` / `rollback_replay` pair, a matching hash is the pass condition, but nothing checks it.
- **Contract violations are untested.** No test exercises a `pre` and checks that it is reported through `assert_fail`.
- **Stale docs.** `scenario_authoring.md:143,208` says a tripped assert hangs. It now exits 3.

## Proposed framework: `gse.test`

This is a proposal and nothing below is built. It reuses what the engine already does: registration by reflection over annotated functions (the same shape as `gse.scenario` and the system manifest), argument parsing through `parse_args`, and failure reporting through `gse.log`. There are no macros and no DSL. A test is a plain function.

Settled decisions:
- **No exceptions.** Every check returns `bool`, and a test stops early with `if (!ctx.expect(...)) return;`. Code under test that returns `std::expected` is checked with `expect_value`, which prints the error when the check fails.
- **Tests live next to their module**, in a `Tests/` folder beside the source: `Engine/Engine/Source/Math/Tests/`, `Editor/Editor/Source/Lint/Tests/`, `Sandbox/Sandbox/Source/Tests/`.
- **Engine, editor and game all run tests** through one runner function. Each executable sweeps its own test namespaces.

### Declaring tests

A test module exports a namespace of annotated functions. The test name comes from `identifier_of`, so it is never typed twice.

```cpp
export module gse.tests.math;

import std;

import gse.math;
import gse.test;

export namespace gse::tests::math {
	[[= test::unit{}]] auto vec_dot_is_commutative(
		test::context& ctx
	) -> void;

	[[= test::unit{ .tags = "units" }]] auto sqrt_of_area_is_length(
		test::context& ctx
	) -> void;

	[[= test::violation{ .expect = "index < N" }]] auto vec_index_out_of_range(
		test::context& ctx
	) -> void;
}

auto gse::tests::math::vec_dot_is_commutative(test::context& ctx) -> void {
	const vec3f a{ 1.f, 2.f, 3.f };
	const vec3f b{ 4.f, 5.f, 6.f };
	ctx.expect_eq(dot(a, b), dot(b, a));
}

auto gse::tests::math::sqrt_of_area_is_length(test::context& ctx) -> void {
	const auto area = meters(3.f) * meters(3.f);
	if (!ctx.expect(area > decltype(area){})) {
		return;
	}
	ctx.expect_near(sqrt(area), meters(3.f), meters(1e-6f));
}

auto gse::tests::math::vec_index_out_of_range(test::context&) -> void {
	vec3f v;
	std::ignore = v[3];
}
```

### Pieces

- **Annotations.**
  - `test::unit{ .tags, .needs_gpu, .isolated }` marks an in-process test.
  - `test::violation{ .expect }` marks a test that must end in a contract or `gse::assert` failure whose comment contains `.expect`.
- **Checks.** Each check returns `bool` and, on failure, records the caller's `source_location` plus a message.
  - `expect(cond)`
  - `expect_eq(actual, expected)` prints both values through their `std::formatter`, so quantities, vectors and matrices print with their units.
  - `expect_near(actual, expected, tolerance)` takes quantities as they are and never strips units.
  - `expect_value(std::expected<T, E>)` fails and prints the error when the expected holds one.
  - `expect_error(std::expected<T, E>)` fails when the expected holds a value.
- **Registry.** `test::registry<^^Ns>()` sweeps a namespace recursively with `members_of`, the same way `scenario::registry` does. There is no static-init registration.
- **Runner.** `test::run(tables, config) -> int` takes one or more registry spans. It returns 0 on a pass and 1 on any failure, and logs under a `test` category so `gse_log_query` can filter a run. Its flags come from a reflected `test::config` parsed by `parse_args`: `filter`, `tags`, `list`, `only`, `repeat`, `seed`.
- **Out-of-process tests.**
  - `assert_fail` ends in `_Exit(3)`, which is what production wants, so the runner never suppresses it.
  - For violation tests and `.isolated` tests, the runner re-launches its own executable with `--test-only <name>`.
  - A violation test passes when the child exits 3 and the child's log holds `.expect`.
- **Headless by default.** A test with `.needs_gpu` gets a device, or is reported as skipped when there is none.

### Hooking in the editor and the game

Every sweep happens in the executable's own TU, as the Sandbox scenario sweep already does, since a namespace sweep cannot move into a module.

| Executable | Sweeps | Invoked as |
|---|---|---|
| `GSETests` | `^^gse::tests` | `GSETests --test-filter math` |
| `Editor.exe` | `^^gse::tests`, `^^gse::editor::tests` | `Editor.exe --test-filter lint` |
| `Sandbox.exe` | `^^gse::tests`, `^^sandbox::tests` | `Sandbox.exe --engine-test-filter net` |

`test::config` is a nested member of each executable's argument config. When any test flag is present, `main` calls `test::run` and returns its code before the engine starts a window. `GSETests` exists so engine tests build and run without the editor or a game.

### Scenario tier

Scenarios stay the integration tier and gain a verdict.

- `scenario::context` gets the same `expect` family plus read-only world access. That also makes `wait_until(ctx, predicate)` possible.
- `finish_bench` returns a status. `main` returns non-zero on a failed expectation, a settle-cap abort or a baseline regression.
- `scenario::info` gets `.compare_with = "rollback_reference"`, which runs the paired scenario and requires equal world-state hashes. This covers the rollback pairs and the CPU/GPU identity half of `parity_gate.py`.
- `parity_gate.py` keeps its positional and cascade checks until those move into scenarios as `expect_near` on state records.

## Priority list

Order is by value per cost. The first criterion is whether the code has already broken once. The second is whether it runs headless with no GPU. Each item names the tier and the module whose `Tests/` folder it belongs in.

### P0: pure code, past regressions, runs anywhere

1. **Units** (`Math/Units`, unit tier).
   - Conversions round-trip.
   - Dimension products hold at runtime as well as in the `static_assert`s.
   - Integer-literal time does not wrap (`seconds(5)`).
   - Mixed-unit multiply lands at the right scale.
   - asin/acos/sqrt/fmod domain contracts.
2. **Math contracts** (`Math`, violation tier). One test per `pre` and `contract_assert`: vec/mat indexing, matrix and quaternion inverse, look_at, perspective, orthographic, the SIMD span sizes, rect and circle. These also prove the `__tu_has_violation` bridge still routes to `assert_fail`.
3. **Vector, matrix and quaternion math** (`Math`, unit tier).
   - Inverse times the original is identity.
   - Quaternion to matrix agrees with the direct matrix.
   - The degenerate-input policy for normalize, project and angle_between (they return zero) is pinned.
4. **Binary archive** (`Containers/Archive`, unit tier). Reflected structs round-trip, `archive_skip` is honoured, and a truncated payload is rejected rather than read.
5. **Bitstream and packet header** (`Network`, unit tier). Bit-exact round trips, quantized fields, and a header sequence that wraps.
6. **Ring buffers and the work-stealing queue** (`Concurrency`, unit tier, `.repeat` for stress).
   - MPSC publish-after-write is the regression.
   - Also SPSC order and steal/pop exclusivity.
   - Threads come from `task::spawn`, never `std::thread`.
7. **`parse_args`** (`Meta/Args`). Flag naming, `--no-` negation, clamp annotations, nested groups. Unknown-flag and bad-value exits run as isolated tests.
8. **JSON** (`Json`, unit tier). Parse/write round trip, reflected structs, and malformed input returning an error rather than asserting.
9. **Lexer and classifier** (`Syntax`, unit tier). Token kinds and `splice_position`. The editor's highlighting depends on them.
10. **ID, slot map and flags** (`Core`, `Containers`, unit tier). Generation reuse, stale-handle rejection, and flag set algebra.

### P1: engine logic, headless

1. **Scheduler graph** (`Ecs`, unit tier).
   - Edges are derived from access.
   - Declared dependencies outrank derived ones, which is the cycle that used to assert.
   - An external resource has exactly one writer.
2. **Settings scope split** (`Save`, unit tier). A `project_scope` field lands in the project file and everything else in the user ini. Both round-trip through `SaveSystem` against a temp root.
3. **State dump** (`Runtime/StateDump`, unit tier). Write/read round trip, and a version mismatch is rejected.
4. **Reliable channel and peer liveness** (`Network`, unit tier over a loopback endpoint).
   - A resend rebinds the sequence.
   - A silent peer is reaped.
   - A goodbye tears the peer down.
5. **Scenario verdicts** (scenario tier). The CPU pyramid and CPU parity scenarios give the same hash across two runs.
6. **Rollback pairs** (scenario tier, `.compare_with`). Each `rollback_replay*` matches its reference hash.

### P2: editor

1. **Lint rules** (`Editor/Lint`, unit tier on fixture sources). Unused import following `[module.import]/7`, narrowable imports, redundant qualifiers in out-of-line scope, and unused-name placeholders.
2. **Build output parsing** (`Editor/BuildRunner`, unit tier). SARIF display columns versus byte columns, and the inbox queue order.
3. **GUI interaction policy** (engine GUI, unit tier on a headless `draw_context`).
   - `clip_for(layer)` above and below `popup`.
   - `dismissed_by_outside_press` with `keep_open` and `suppressed`.
   - A press consumed by the first drawer.
   - Font wrap honouring newlines.
4. **Markdown format** (`Editor`, unit tier). The formatter is idempotent: format(format(x)) equals format(x).

### P3: GPU and game

1. **CPU/GPU identity** through `.compare_with`, retiring the identity checks in `parity_gate.py`.
2. **Positional and cascade parity** moved from `parity_gate.py` into scenario `expect_near`.
3. **Perf baselines** as scenario failures instead of log lines.
4. **Sandbox gameplay and networking** (`sandbox::tests`, `net_walk_*` with expectations).

## Phases

1. Build the `gse.test` module, `test::run` and `GSETests`, with P0 items 1–3 as the first consumers.
2. Add the rest of P0, then the editor and Sandbox hooks.
3. Scenario verdicts, `wait_until`, and P1 items 5–6.
4. The rest of P1, then P2.
5. P3 and a build-and-test CI workflow.

