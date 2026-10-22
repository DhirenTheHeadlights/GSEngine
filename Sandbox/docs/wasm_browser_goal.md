# Goal: a headless GSE engine tick running in a browser

*(Amended after the owner proposed the GPU-backend seam shape for the OS layer. Only
"The work" step 2 and the charter's seam line changed; aim, pass criteria and the rest of
the charter are as previously drafted.)*

## Aim

Get the engine itself — not just the toolchain — across to WebAssembly, so that a named
headless scenario boots, ticks on the fixed-step clock and exits inside a browser page,
producing the same world state as the native run. A WebGPU backend and a rendered frame
are explicitly **out** of this goal; they are the next one, after this passes.

The toolchain half is already done and is not what this goal is about. As of step 19
(`docs/wasm_pipeline_plan.md`): 131 interface units compile for wasm32, 0 are blocked, and
the only 2 rejections are `Math/Tests/{Units,Math}.cppm` behind real exceptions. Nothing
the compiler rejects is codegen, the backend, or threads. What stands between here and a
browser is **292 host-bound units behind `gse.win32`/`gse.moodycamel`**, the engine's
~111 `catch` sites, and a host layer that does not exist.

## Pass criteria

A steering run decides the aim is met from artifacts, no judgement:

1. A browser page loads the wasm module and runs the headless CPU scenario
   `scene_reset_cpu` (`Sandbox/Sandbox/Source/Sandbox/Scenarios.cppm:87`, 240 frames, 0
   warmup, no GPU solver) to its own exit, with no manual input.
2. Its console output carries `world-state hash 0x<16 hex>` — the same line
   `Tools/parity_gate.py:11` already greps for — plus the p50 row, and the process exits 0.
3. **Hash identity** against a *native* run of the same scenario and frame count built
   with the portable `:simd` arm (`GSE_SIMD_X86` off). This is the `require` level.
4. **Positional agreement** against the ordinary native AVX2 run, via the existing
   `--engine-bench-state-dump-out` record and `parity_gate.py`'s positional check, at that
   gate's `0.002` tolerance. `require`. Hash identity against the AVX2 build is `report`
   only — see the assumptions.
5. (3) and (4) hold on **3 consecutive runs**, and the hash is stable across page reloads
   within a run of the harness.
6. The harness writes a report artifact under `Tools/` output recording all of the above,
   so a fresh session can re-check it without reconstructing the invocation.

## Charter

**Authorized, no asking:**
- `Tools/build_gcc_wasm.py`, `Tools/module_graph.py`, `Tools/analyze_module_graph.py`, the
  wasm patch series under the toolchain recipe, and a new browser/node harness beside them.
- `docs/wasm_pipeline_plan.md` and this record's ledger.
- `Engine/Engine/Source/Os/**`, `Engine/Engine/Source/External/{Win32,Moodycamel}.cppm`,
  `Engine/Engine/Import/Os.cppm`, new `Os/Web/` sources, and `Engine/cmake/` for impl-unit
  selection.
- **The seam is settled, by the owner: the `gse.gpu` backend shape — a platform-neutral
  interface partition, with each platform's implementation imported only from an impl unit
  so it is never a transitive import — minus the runtime dispatch.** Moving an inline
  implementation out of an interface partition into a `:*_impl` unit is therefore in
  charter even where it touches native-only code, because it is the seam.
- The spine modules the port crosses (`gse.time`, `gse.concurrency`, `gse.diag`, `gse.ecs`,
  `gse.fs`, `gse.save`, `gse.scenario`, `gse.system_manifest` and their siblings), and the
  `catch` sites *inside the modules the port actually reaches* — each one a decision
  between an error channel the caller already has and a genuinely fatal condition.
- New GCC patches (`0018+`). Never re-cut an applied patch — `apply_patches` keys its stamp
  on patch bytes and demands the source tree be deleted.
- Long compute: full toolchain, libgcc and libstdc++ rebuilds (hours) are expected.
- Running sweeps and probes; `gse_run` with `--engine-setting` overrides.

**Owner's call, escalate:**
- Any WebGPU/WGSL/renderer work, or anything that touches `Gpu/`, `Vulkan/`, `Dx12/`.
- Anything that changes native behaviour: native build defaults, `-mavx2`, solver order, or
  anything that would move a native world-state hash. A portable-SIMD *configuration* is
  in charter; changing the default is not.
- Deleting or disabling a native code path to make the wasm build go through.
- Introducing runtime OS dispatch (an erased backend + dispatch table). Ruled out below;
  if a task finds a case that genuinely needs it, that is a design change, not a detail.
- Any commit or push.
- Moving the snapshot pin. Standing rule from step 15: the wasm cross and the native
  toolchain move together, and `20260809` is what the engine builds with.
- `settings.ini`. Diagnostics go on as `--engine-setting` overrides for one run, never
  written to the owner's file.
- Starting on real exceptions (GCC EH region generation). It is the largest remaining
  toolchain item and this goal is designed to *not need it* — the two rejections are test
  modules. If the port hits a `catch` that cannot be converted, escalate rather than
  opening the backend.

## The work, in the order the measurements imply

Not a task list for approval — the order, so a steering run does not have to re-derive it.

1. **`gse.moodycamel`, expected near-free.** `concurrentqueue.h:97` only takes the
   `GetCurrentThreadId` arm on Windows; the fallback at :158 is a `thread_local`-address
   id, which step 19's native TLS now supports. Then `gse.time` behind it. This is the
   cheapest possible check that step 19's claim holds at engine scale.

2. **The OS seam — the `gse.gpu` shape, and it is already most of the way there.**
   The decided shape has two halves, and only one is adopted:
   - **Adopted: the import discipline.** `Gpu/Device/Device.cpp:1-20` is the sole importer
     of `:device_vulkan_backend`/`:device_dx12_backend`, so no consumer of `gse.gpu`
     transitively imports either. This is precisely what the port needs, because the
     measured blocker *is* transitive imports — a dependent cannot compile while something
     it imports has no BMI, which is the whole of step 18d's "292 host-bound".
   - **Rejected: the runtime dispatch.** `Device.cpp:34-39`'s erased `void*` + reflection
     built `gpu_dispatch`, selected at `Device.cpp:52`, exists because Vulkan and DX12
     coexist in one binary. There is exactly one OS per binary and a wasm build can never
     select the Win32 arm, so a dispatch table buys a branch unreachable by construction.
     Impl-unit selection in CMake instead, matching the existing `Os/Win32` + `Os/STB`
     directory split.

   **Measured, and it reduces this step a long way: 9 of `gse.os`'s 12 interface partitions
   already follow the pattern.** `:window` does not import `gse.win32` at all — all 34
   `win32::` uses are in `Window.cpp` — and nor do `:keys`, `:input`, `:input_state`,
   `:input_events`, `:actions`, `:image_loader`, `:player_input`, `:player_controller`.
   `native_window_handle` is already an opaque `void*` (`Window.cppm:93-94`).

   Three interface units leak it, and two leak because their implementation is inline in
   the interface rather than because their API is Windows-shaped:

   | unit | `win32::` uses in the `.cppm` |
   |---|---|
   | `:process` (`Os/Process.cppm:7`) | 63 |
   | `:clipboard` (`Os/Win32/Clipboard.cppm:7-8`) | 47 |
   | `:app` (`Os/App.cppm:10`) | 2 |

   So the step is: move those three bodies into `:process_impl` / `:clipboard_impl` /
   `:app_impl` the way `:window_impl` already exists, then one web impl unit per partition.
   The extraction is a refactor that stands on its own merits natively, independent of wasm.
   `gse.win32` itself needs nothing — its whole body is already inside `#ifdef _WIN32`
   (`External/Win32.cppm:3,20`), so on wasm it is an empty module and the breakage was only
   ever in the callers.

   **Not yet established:** whether `:app`'s exported API survives the move unchanged. It
   traffics in `std::wstring` and `std::span<void* const> handles`
   (`App.cppm:24-31`) for relaunch handoff, which may be a Windows concept rather than a
   Windows spelling. If it is, the web arm is a stub that reports unsupported, and the
   relaunch callers need the same decision the `catch` sites need.

3. **A headless host.** No window, no GPU device, no input. `scene_reset_cpu` is
   `headless = true`, so this is the narrowest host surface the scenario runner will accept,
   and finding out exactly what `headless` still demands of `gse.gpu` is the first thing
   that task measures.

4. **Assets.** The scenario names a scene, so `gse.fs` needs a read-only VFS over preloaded
   bytes rather than wasi's directory access.

5. **The `catch` sites the port crosses**, per site, never as a sweep.

6. **The browser harness.** Extend step 19's node harness to a page; `--export-memory` and
   the Worker-backed `wasi.thread-spawn` carry over. Reuse `parity_gate.py`'s regex
   contract rather than inventing an output format.

## Assumptions, and what I would do if each is wrong

- **Bit-exact native-AVX2 vs wasm hash parity is not assumed, deliberately.** Wasm offers
  128-bit SIMD against an unconditional `-mavx2`
  (`Engine/cmake/GSEEngine.cmake:36`), and `SIMD.cppm`'s portable arm is a different
  computation, not a slower one. That is why (3) compares against a portable-SIMD native
  build and (4) carries a tolerance. **Unverified:** that `GSE_SIMD_X86` can be turned off
  in a native build without editing the engine. If it cannot, criterion (3) drops to
  `report` and (4)'s positional tolerance becomes the only `require` — a weaker but still
  honest gate, and I would say so in the ledger rather than quietly relaxing it.
- **The scoping phase could not run or build anything** (scoping withholds both), so every
  number above is read from `docs/wasm_pipeline_plan.md`, the harness sources and the
  module graph, not measured on the day the goal was written. The first task re-runs the
  sweep before trusting the 131/2/0 counts.
- **`headless = true` may still construct a GPU device.** If it does, step 3 grows to a null
  backend behind `backend_kind` — which is still not WebGPU and still in charter.
- **The three-unit count is a count of interface units importing `gse.win32`, not of
  portability work.** A partition that imports nothing Windows-shaped can still have a
  `.cpp` with no web answer; step 2's extraction is what makes that visible per partition
  instead of all at once.

## Ledger

### 2026-10-18 — the record written, and one of its premises already expired

The approved scope carried a charter item authorizing a single commit of
`docs/wasm_pipeline_plan.md`, on step 19's finding that the file was untracked and that
steps 14-17 in it were an uncommitted reconstruction. **That has expired and no commit is
needed:** the file is tracked and clean at 2363 lines, committed in `c264a6fd`
("tree: wasm toolchain, gui widget skins, agent sync phase, and a CI suite"), with all six
of steps 14-19 present in `HEAD`. The authorization is removed from the charter above
rather than left to send a later run after a file that is already safe — committing is
now wholly the owner's call, with no exception.

Nothing else in the record was changed when writing it. No engine or toolchain work has
been done against this goal yet; step 1 (`gse.moodycamel`, then `gse.time`) is the first
task.

### 2026-10-18 — steering: record relocated to the path the editor reads; step 1 scoped

**Judged:** nothing to judge against the pass criteria. The preceding task's deliverable was
this document; no build and no run, as it reported, so criteria (1)-(6) are all unstarted
rather than failed. The review found no defects and confirmed the one external reference
the criteria lean on (`scene_reset_cpu`, `Scenarios.cppm:87`, `headless = true,
warmup_frames = 0, frames = 240`).

**One defect the review could not see, now fixed.** The record was written to
`docs/wasm_browser_goal.md` at the repo root, but the editor resolves this chat's goal
record to `Sandbox/docs/wasm_browser_goal.md` — a directory that did not exist. The
steering prompt that woke this run therefore said the record "does not exist yet. Write it
first", and every later run would have been told the same and rewritten the record from
scratch, discarding this ledger each time. The file is moved to `Sandbox/docs/`; nothing in
its text changed. Note the asymmetry that caused it: `docs/wasm_pipeline_plan.md` is at the
repo root and the charter's path for it is unchanged, while the goal record alone lives
under `Sandbox/docs/`. Still untracked, and still the owner's call to commit.

**Next:** step 1 as written — `gse.moodycamel`, then `gse.time` — with the sweep re-run
first so the 131/2/0 counts are measured rather than read.

### 2026-10-18 — step 1 done: `gse.moodycamel` translates, and one `throw` now gates the spine

**Measured, not read.** The sweep was re-run before anything changed, and the record's
131/2/0 does not reproduce: the baseline is **121 compiled / 3 rejected / 298 host-bound**,
over 422 interface units rather than 425. The arithmetic closes (`131 − 3 deleted − 6 newly
host-bound − 1 newly rejected`), so nothing regressed — but the third rejection is real and
was invisible to the record: `Time/Timer.cppm` on `Math/Units/Quantity.cppm:887`, in a file
whose last change predates the step-19 sweep. The record's count was not reproducible from
the tree it was recorded against.

**Step 1's own claim holds.** `External/Moodycamel.cppm` compiles for wasm32, and
`Time/FrameSync.cppm` behind it. The `_WIN32` arm of `concurrentqueue.h:97` is one of four
and wasm32 takes the `thread_local`-address arm at `:158`, as predicted; the header's
`catch` sites needed nothing, because `-fno-exceptions` leaves `MOODYCAMEL_EXCEPTIONS_ENABLED`
undefined and its `TRY/CATCH` macros degrade on their own. Nothing was stubbed, disabled or
`#ifdef`-ed away. After: **123 compiled / 4 rejected / 70 blocked / 225 host-bound**,
15 modules translating.

**`gse.moodycamel` is off the host frontier entirely**, and that is the step's real
dividend: the record's "first move is `gse.moodycamel` + `gse.win32`, thirteen modules" no
longer exists as a group. `gse.win32` now prints **sole 4** — the first non-zero `sole`
column this engine has ever produced — and 9 modules sit behind `gse.win32` +
`gse.win32.environment` alone. Step 2's target is exactly those two, and it can now be
priced on its own.

**The blocker, and it is the owner's call.** Dropping the `gse.win32` edge out of
`gse.time:system_clock` (its body moved to `Time/Win32/SystemClock.cpp`) offered that unit
to the compiler for the first time, and it failed on the *same* site as Timer.cppm. All
four of the engine's wasm32 rejections are one statement: the single `throw format_error`
at `Quantity.cppm:887`, the malformed-spec branch of `std::formatter<quantity>::parse`,
unreachable for every format string that reaches it and rejected by `-fno-exceptions`
anyway. **Eight modules and 70 units are blocked on `gse.time:system_clock`, and
`gse.time` is blocked on that one line.** It is not the charter's "real exceptions" item —
no EH region generation is involved — but every conversion of it changes what a native
build does with a malformed format spec, which the charter reserves. Escalated rather than
decided; step 2 (`gse.win32`) does not depend on the answer and can proceed in parallel.

**Also fixed, because nothing could be measured until it was.** `apply_patches` refused to
start, reporting `0001-wasm-New-backend.patch has changed` and demanding the GCC source
tree be deleted — hours of rebuild. It had not changed: `.gitattributes` pinned
`Tools/gcc-toolchain/recipe/**` to LF and never covered `Tools/gcc-toolchain/wasm/**`, so
`core.autocrlf` had rewritten all seventeen patches to CRLF on checkout. All fifteen
applied stamps match their LF digest and none match their on-disk bytes. `.gitattributes`
extended, working tree normalised, `git diff` over the patch directory empty — no patch
re-cut, no rebuild needed.

**Next:** step 2, the OS seam, now narrowed to `gse.win32` + `gse.win32.environment` and
the three interface units that leak `win32::` (`:process`, `:clipboard`, `:app`). Carry the
`Quantity.cppm:887` decision to the owner first — it does not block step 2, but it does
block every pass criterion, since `scene_reset_cpu` cannot tick without `gse.time`.

### 2026-10-18 — steering: step 1 judged (holds), and the one line that gates the goal escalated

**Judged against the criteria: nothing to pass yet.** (1)-(6) are unstarted, not failed — the task
neither linked nor ran anything, as it reported. The only measurable claims are the sweep counts,
and they were re-run on the day: baseline **121 ok / 3 rejected / 298 host-bound**, after
**123 ok / 4 rejected / 70 blocked / 225 host-bound**, 15 modules translating. Step 1's own claim
is confirmed: `External/Moodycamel.cppm` and `Time/FrameSync.cppm` compile for wasm32 with nothing
stubbed or `#ifdef`-ed out, and `gse.moodycamel` is off the host frontier. `gse.win32` printing
`sole 4` is the measurement step 2 needs.

**The record's step-19 counts (131/2/292) do not reproduce and are retired.** The arithmetic
closes, so nothing regressed; treat the numbers in "Aim" above as historical and the 121/3/298
baseline as the real one. Also noted from the task: `gse.time` does not read `translates` as step 4
of the plan predicted — its win32 edge is gone, but it reads `FAILED 2/9`.

**Verified at source, because the escalation turns on it.** `Math/Units/Quantity.cppm:887` is the
*only* `throw` in the file and the sole cause of all four wasm32 rejections. `parse` is `constexpr`
(`:864`), and the throw sits in the `{`-but-not-`}` branch of the dynamic-unit spec. That matters
for pricing the native-behaviour change the charter reserves: for every `std::format` call with a
literal format string the spec is checked in a constant expression, so a malformed dynamic unit is
**already a compile error today** and would remain one under any of the three conversions. The
behaviour that actually changes is confined to the runtime-spec path — `std::vformat` /
`std::runtime_format` with a spec not known until runtime — which is the narrowest possible blast
radius, but still a native change and still not mine to make.

**Why this is the block and not a detour.** Eight modules and 70 units sit behind
`gse.time:system_clock`, `gse.time` is behind this line, and `scene_reset_cpu` cannot tick without
`gse.time`. No pass criterion is reachable until it is decided. It is *not* the charter's reserved
"real exceptions" item — no EH region generation is involved.

**Verdict: escalate, and do not wait on it.** Step 2 (`gse.win32` + `gse.win32.environment`, and the
three interface units that leak `win32::`: `:process` 63 uses, `:clipboard` 47, `:app` 2) does not
depend on the answer and is the next task either way. Recommendation carried to the owner:
`#if __cpp_exceptions` around the throw with a `std::unreachable()`-free fallback that keeps the
compile-time diagnostic — i.e. the native translation unit is byte-identical because `__cpp_exceptions`
is on there, and only the `-fno-exceptions` wasm arm takes the other path. That makes the native
change *nil* rather than *narrow*, at the cost of two spellings of one branch.

**Also carried forward, unchanged and still open:** distinct `thread_local`-address thread ids are
unproven past `-S` (a step-3 check needing a linked run); `Time/Win32/SystemClock.cpp` is compiled
unconditionally by the CMake glob until step 2 does impl-unit selection; and nothing is committed —
`.gitattributes`, both docs, `Sandbox/docs/` and `Engine/Engine/Source/Time/Win32/` are all
uncommitted or untracked, which remains wholly the owner's call.

### 2026-10-18 — steering, amended: the owner asked why not just remove the throw. Answer: don't, but there is a fourth option with no native change at all

**Removing it is not safe, and the escalation's framing was too narrow.** The throw is not dead
code — it is the diagnostic for an ill-formed spec, unreachable only for well-formed ones. Deleting
it lets the `{`-without-`}` branch fall through to `ctx.next_arg_id()` and
`check_dynamic_spec_string`, so a malformed dynamic unit would be accepted and formatted wrongly
instead of rejected; and because `parse` is `constexpr`, a malformed *literal* spec is a compile
error today, which removal silently downgrades to a wrong answer.

**But libstdc++ has already solved exactly this, and the engine can copy it verbatim.** The
`<format>` header spells no `throw` anywhere either: every sibling error in the same
`std::formatter` protocol routes through `std::__throw_format_error`
(`libstdc++-v3/include/std/format:242`), whose body is `_GLIBCXX_THROW_OR_ABORT` —
`(throw (_EXC))` under `__cpp_exceptions`, `(__builtin_abort(), (void)(_EXC))` without it
(`bits/c++config:260-264`). Replacing the one `throw` at `Quantity.cppm:887` with a call to it gives:

- **native behaviour bit-identical** — same throw of the same `format_error` with the same message,
  because `__cpp_exceptions` is on in the native build and the macro expands to today's statement;
- **the compile-time diagnostic preserved on both arms** — `__builtin_abort()` is no more a constant
  expression than `throw` is, so a malformed literal spec still fails to compile on wasm too;
- **one spelling, no `#if`** — and unqualified lookup resolves, since the formatter is declared as
  `struct std::formatter<...>` (`:856`) and is therefore inside that namespace.

The only cost is depending on a `__`-reserved libstdc++ name. This engine can afford it: GCC trunk
is its only viable frontend, and this is the function the standard library itself uses for the
errors immediately either side of ours in the same protocol.

**This supersedes the three options in the entry above and dissolves the escalation.** The charter
reserves changes to native behaviour; this change has none, so it is not the owner's call on those
grounds and goes back to being an ordinary task. The `#if __cpp_exceptions` option stays on the
table only as the owner's preference, not as a necessity.

**Next task:** the one-line substitution at `Quantity.cppm:887`, then re-run the sweep and judge it
against two things — that the four wasm32 rejections go to zero, and that `gse.time` and the 70
units blocked behind `gse.time:system_clock` begin translating. Native build must be re-run green,
and the native world-state hash must not move (it cannot, but it is the cheap proof that "bit-identical"
is a fact and not an argument). Step 2 (the OS seam) follows and is still independent of this.

### 2026-10-18 — steering: the `Quantity.cppm` fix judged (holds, native nil), and the gate moves to one `try` in the task pool

**Judged against the pass criteria: still nothing to pass.** (1)-(6) are unstarted — nothing was
linked or run in a browser. What is measurable is measurable, and it holds.

**The change is what it claims.** `git diff` over `Math/Units/Quantity.cppm` is exactly four added
lines: `#if __cpp_exceptions` / existing `throw format_error(...)` / `#else` / `__builtin_abort()` /
`#endif`, around the `{`-without-`}` branch at `:887`. No other file touched. Native is therefore
not "nearly" unchanged but *textually* unchanged on its arm, and the reported re-run of
`scene_reset_cpu` reprints `world-state hash 0xe836d31697879e27`. The compile-time diagnostic
survives on both arms because `parse` is `constexpr` and `__builtin_abort()` is not constant-evaluable.

**The preferred spelling is refuted, and the ledger should keep it so no later run retries it.**
`std::__throw_format_error` is *not* reachable through `import std;` — it lives in the global module
fragment of `bits/std.cc` and is not exported. The previous entry's argument for it was sound about
behaviour and wrong about visibility. The two-arm `#if` is the spelling, permanently.

**The totals the task left untallied are in its own log, and they are a large move.** From
`/tmp/wasm_sweep2.log`, which did complete: **ok=152, failed=1, blocked=44, host=225**; modules
`translates=20, FAILED=1, blocked=5, host-bound=37`. Against the 123/4/70/225 baseline that is +29
units translating, rejections 4 → 1, blocked 70 → 44, host-bound flat — the whole gain came out of
the blocked pool, which is what unblocking `gse.time` was supposed to do. `gse.time` reads
`translates 9 units`.

**The single remaining rejection is now the gate on the pass criteria, and it is narrower than the
last one.** `Concurrency/Task.cppm` fails on the `try`/`catch` at `:860-879` in `run_job`, and the
sweep names what sits behind it: `gse.ecs` (blocked 10/15), `gse.http` (2/5), **`gse.scenario`
(1/1)**, **`gse.system_manifest` (1/1)**, `gse.tests.concurrency` (1/1) — all `on
gse.concurrency:task`. `scene_reset_cpu` cannot be named, let alone ticked, without `gse.scenario`
and `gse.ecs`. 44 units behind one `try`.

**Read at source, because it decides the conversion.** The two handlers are a diagnostic boundary,
not an error channel: both arms only `log::println` a stacktrace and fall through, there is no
recovery, no rethrow and no value propagated to a caller. Under `-fno-exceptions` nothing inside
`entry.fn()` can throw, so the handlers are unreachable by construction rather than merely unused —
`#if __cpp_exceptions` around the `try`/`catch` is the honest translation, native byte-identical,
same shape the owner already preferred at `Quantity.cppm`. This is step 5's first site and it is
being decided per-site, as the step requires; it is not a sweep.

**Step 2 (the OS seam) is unchanged and still the larger frontier**, now priced from this sweep:
`gse.win32` **sole 4, reached 24**, `gse.win32.environment` reached 19, 4 modules behind `gse.win32`
alone and 5 behind the pair. It does not gate the scenario the way `Task.cppm` does, so it goes
second.

**Next:** the `#if __cpp_exceptions` conversion of `run_job`'s `try`/`catch`, native build green and
the native hash re-checked, then the sweep re-run and judged on two things — rejections to zero, and
`gse.scenario`/`gse.system_manifest`/`gse.ecs` off the blocked list.

**Still open, carried forward:** distinct `thread_local`-address thread ids unproven past `-S`;
`Time/Win32/SystemClock.cpp` compiled unconditionally by the CMake glob (step 2's job); nothing
committed — `.gitattributes`, both docs, `Sandbox/docs/` and `Engine/Engine/Source/Time/Win32/` all
uncommitted or untracked, the owner's call.

### 2026-10-19 — steering: `run_job` converted and correct, but the unblock did not happen; the gate is a chain, so scope it as one

**Judged against the pass criteria: no movement.** (1)-(6) remain unstarted; nothing was linked or
run in a browser. The sweep is still `ok=152 failed=1 blocked=44 host=225`, identical to the previous
entry's numbers, so the 44 units and five modules (`gse.ecs` 10/15, `gse.http` 2/5, `gse.scenario`
1/1, `gse.system_manifest` 1/1, `gse.tests.concurrency` 1/1) are blocked exactly as before.

**The change itself is verified at source and is what it claims.** `git diff` over
`Concurrency/Task.cppm` is four added preprocessor lines and nothing else: `#if __cpp_exceptions`
before the `try` at `:855`, `#endif`/`#if __cpp_exceptions` bracketing the closing brace, `#endif`
after the `catch (...)` handler at `:884`. Both handlers are still log-and-continue; no statement
moved. Native is textually unchanged on its arm and `scene_reset_cpu` reprints
`world-state hash 0xe836d31697879e27`. Verdict on the site: correct, keep.

**Why it bought nothing, and the lesson for scoping.** The rejection was never *one* `catch`; it was
the first of a chain reachable from one unit. `Task.cppm:821`'s `make_scope_exit` instantiates
`gse::scope_exit<F>::~scope_exit()`, whose body is `try { invoke } catch (...) { std::terminate(); }`
(`Core/ScopeExit.cppm:67-72`) — so with `run_job` converted the same unit simply fails one frame
deeper. Converting one site per task converts one link per round trip, and the sweep only ever shows
the *next* link, never the length of the chain. Also worth keeping: a sweep rejection row names the
importer that triggered the instantiation, not the file holding the `catch`.

**The chain, read ahead so the next task is not scoped blind.** Across `Core`, `Concurrency`, `Ecs`,
`Scenario`, `Save`, `Fs`, `Meta`, `Time` and `Diag` interfaces there are exactly four `catch` sites
left: `Core/ScopeExit.cppm:70` (`std::terminate()` in an already-`noexcept` destructor — the handler
is redundant even natively, which is why `#if` is honest rather than a behaviour change),
`Concurrency/AsyncTask.cppm:383` (`ep = std::current_exception()`, the one that needs real thought
because the whole `exception_ptr` channel disappears on the `-fno-exceptions` arm), and the two in
`run_job` already done. `Meta/Parse.cppm:115-119` exists but is not on this unit's path.

**Next task — the chain, not a link.** Walk it to `failed=0`: convert `ScopeExit.cppm:67-72` with
`#if __cpp_exceptions`, re-run the sweep, and if it names another site on the same path convert that
one too in the same task, each decided and reported on its own merits, stopping to escalate rather
than guessing if a site's handler is a real error channel (`AsyncTask.cppm:383` is the candidate).
Still step 5's per-site rule — per-site *judgement*, one task. Judged on: `failed` to 0, and
`gse.scenario`/`gse.system_manifest`/`gse.ecs` off the blocked list; native build green and
`0xe836d31697879e27` unmoved.

**Carried forward unchanged:** step 2 (the OS seam — `gse.win32` sole 4/reached 24,
`gse.win32.environment` reached 19, and `:process`/`:clipboard`/`:app`) is still the larger frontier
and still independent; `thread_local`-address thread ids unproven past `-S`;
`Time/Win32/SystemClock.cpp` unconditionally globbed; nothing committed, the owner's call.

### 2026-10-19 — the chain was one link: wasm32 rejections and the blocked pool are both zero

**Measured.** One `#if __cpp_exceptions` around the `try`/`catch` in `gse::scope_exit`'s
destructor (`Core/ScopeExit.cppm:67-72`, `#else` arm a plain `std::invoke`) took the sweep from
`ok=152 failed=1 blocked=44 host=225` to **`ok=197 failed=0 blocked=0 host=225`** over 422 units,
`modules: translates=26 host-bound=37`. Both of the step's judging criteria hold: nothing is
rejected, and `gse.scenario` (1/1), `gse.system_manifest` (1/1) and `gse.ecs` (15/15) all read
`translates`, as do `gse.concurrency` (13), `gse.core` (7), `gse.diag` (7), `gse.time` (9),
`gse.http` (5), `gse.tests.concurrency` (1). The 44 blocked units did not move to another edge;
they compiled. Native build green, `scene_reset_cpu` re-run, 240 frames,
`world-state hash 0xe836d31697879e27` unmoved — the `#if` arm is textually today's code.

**The chain the previous entry predicted was shorter than feared, and the reason is worth
keeping.** `Concurrency/AsyncTask.cppm` and `Meta/Parse.cppm` both swept `ok` with their `catch`
sites untouched, so neither was converted. GCC rejects `try`/`catch` under `-fno-exceptions` at
*instantiation*, not at parse: `run_job` was an ordinary function and `~scope_exit` was
instantiated by `make_scope_exit` at `Task.cppm:821`, but `sync_wait` and
`parser<T>::parse`'s floating-point branch are uninstantiated template bodies in every unit the
sweep offered. **So a `catch` in a template that nothing on the path instantiates is not a
blocker**, and the remaining `catch` inventory is not a backlog — only the sites something
actually instantiates are. In particular `AsyncTask.cppm:383`'s `exception_ptr` channel, which the
last two entries queued up as the hard one needing a decision, never had to be decided.

**Still undecided rather than decided quietly:** `sync_wait`'s channel and `Parse.cppm`'s
`stod`/`catch` will need an answer if a later step links a unit that instantiates them (a real
link, not a `-S` sweep). Nothing was pre-emptively converted to get ahead of that.

**Next:** step 2, the OS seam, now the only frontier left. `gse.win32` **sole 4, reached 24**;
`gse.win32.environment` reached 19; 4 modules behind `gse.win32` alone, 5 behind the pair; and
`gse.fs`/`gse.save`/`gse.os`/`gse.runtime` all read `host-bound ... -> gse.win32`. The three
interface units that leak `win32::` (`:process` 63 uses, `:clipboard` 47, `:app` 2) are unchanged.

**Carried forward unchanged:** `thread_local`-address thread ids still unproven past `-S`;
`Time/Win32/SystemClock.cpp` still unconditionally globbed by CMake (step 2's job); nothing
committed — `.gitattributes`, both docs, `Sandbox/docs/` and `Engine/Engine/Source/Time/Win32/`
all uncommitted or untracked, the owner's call.

### 2026-10-19 — steering: zero rejections confirmed at source; the "three interface units" figure is wrong, and the seam has a cheap first arm

**Judged against the pass criteria: still nothing to pass.** (1)-(6) are unstarted — nothing was
linked, no page was loaded, no hash came out of a browser. What is measurable holds, and it was
re-read from the artifacts rather than taken from the report.

**Confirmed.** `git diff` over `Core/ScopeExit.cppm` is four added preprocessor lines and nothing
else: `#if __cpp_exceptions` before the `try`, `#else` with a bare `std::invoke(m_func)`, `#endif`.
No statement moved, so the native arm is textually the old code — the unmoved
`world-state hash 0xe836d31697879e27` is a check on that, not the argument for it. The sweep log
(`/tmp/wasm_sweep_a.log`, 2026-10-19 10:08) reads `host=225 ok=197`, `modules: host-bound=37
translates=26`, with no `failed` or `blocked` rows at all — so rejections 1 → 0 and the blocked pool
44 → 0 are facts, and `gse.ecs` (15), `gse.concurrency` (13), `gse.time` (9), `gse.scenario` (1) and
`gse.system_manifest` (1) all read `translates`. Verdict: step 5's first three sites are correct and
the exceptions frontier is closed.

**The record's step-2 figure does not reproduce, and it understates the work.** "Three interface
units leak `win32::`" is a count of `Os/` only. Across `Engine/Engine/Source/**.cppm` there are
**twelve** importers of `gse.win32` besides `External/Win32{,Environment}.cppm` themselves:
`Os/Process.cppm` 63, `Os/Win32/Clipboard.cppm` 47, `Process/Process.cppm` 46,
`Runtime/Bootstrap.cppm` 10, `Fs/Resolve.cppm` 8, `Sdk/Pack.cppm` 5, `Assets/AssetFormat.cppm` 3,
`Vulkan/Instance.cppm` 3, `Os/App.cppm` 2, `Runtime/AttachedLink.cppm` 2,
`Fs/LayoutStore.cppm` 1, `Runtime/FramePacing.cppm` 1. Treat the table in step 2 as the `Os/`
subset, not the frontier.

**But the frontier decomposes, and the piece nearest the scenario is the cheapest.** The sweep names
four separate host-bound chains, not one: `gse.fs -> gse.win32`, `gse.save -> gse.fs -> gse.win32`,
`gse.os -> gse.win32`, `gse.runtime -> gse.win32`. `gse.fs`'s whole leak is **nine `win32::` uses in
two units**, both inline-in-interface rather than Windows-shaped API: `Resolve.cppm:17-31` is one
`CreateFileW`/`GetFinalPathNameByHandleW`/`CloseHandle` body behind a path-canonicalising function,
and `LayoutStore.cppm:408` is `GetCurrentProcessId()` in a temp-filename suffix. That is the seam's
shape exactly — extract, don't redesign — and it unblocks `gse.fs`, `gse.save` and
`gse.tests.save_registry` while being step 4's (assets/VFS) prerequisite anyway. `gse.os`'s 110 uses
go second, priced on their own.

**Next task:** the `gse.fs` arm of step 2. Move both bodies out of the interface partitions into
impl units with a Win32 arm and a web arm, selected in CMake per the settled `gse.gpu` import
discipline — which is also the occasion to replace the unconditional glob that compiles
`Time/Win32/SystemClock.cpp` today. Judged on: `gse.fs` and `gse.save` reading `translates` in a
re-run sweep with `host=` down and `failed=0` held; native build green and
`0xe836d31697879e27` unmoved.

**Carried forward unchanged:** `AsyncTask.cppm:383` and `Parse.cppm:115` hold unconverted handlers
that the `-S` sweep cannot see, to be re-argued if a real link instantiates them;
`thread_local`-address thread ids still unproven past `-S`; nothing committed — `.gitattributes`,
both documents, `Sandbox/docs/` and `Engine/Engine/Source/Time/Win32/` all uncommitted or untracked,
the owner's call.

### 2026-10-19 — steering: the `gse.fs` arm judged (holds); the cheapest remaining arm is `gse.assets`, 3 uses for 6 modules

**Judged against the pass criteria: still nothing to pass.** (1)-(6) are unstarted — nothing linked,
no page, no hash out of a browser. The measurable claims hold, re-read from the artifacts.

**Confirmed at source.** No `.cppm` under `Fs/` imports `gse.win32` any more. `Resolve.cppm` is the
bare exported declaration; the `CreateFileW`/`GetFinalPathNameByHandleW` body is byte-identical in
new `Fs/Win32/Resolve.cpp` (`module gse.fs:resolve_impl;`), diffed line for line. `LayoutStore.cppm`
swaps `win32::GetCurrentProcessId()` for an internal `temp_suffix()` declared at `:128`, defined as
the pid in `Fs/Win32/LayoutStore.cpp` and as `"web"` in `Fs/Web/LayoutStore.cpp` — native temp name
unchanged. `gse_select_platform_impls()` (`Engine/cmake/GSEEngine.cmake:34-60`) filters `.cpp` only,
by directory name, and is called once from `Engine/CMakeLists.txt:19`; the comment is explicit that
`.cppm` under `Os/Win32` is platform-neutral and must not be dropped. `Time/Win32/SystemClock.cpp`
is now selected rather than unconditionally globbed — an item carried for four entries, closed.

**The sweep is what was reported.** `/tmp/wasm_sweep_fs.log` (2026-10-19 10:52): `host=220 ok=202`,
`modules: host-bound=35 translates=28`, no `failed` or `blocked` rows. `gse.fs` and `gse.save` read
`translates`. `gse.win32` is down to **sole 2, reached 22**. The missed third unblock is correctly
diagnosed: the log reads `gse.tests.save_registry -> gse.process -> gse.win32`, a different chain.
Verdict on the step: correct, keep.

**Priced the whole remaining frontier, because the per-arm cost varies by 20x.** Eleven `.cppm` still
import `gse.win32`: `Os/Process.cppm` 63, `Os/Win32/Clipboard.cppm` 47, `Process/Process.cppm` 46,
`External/Win32Environment.cppm` 13, `Runtime/Bootstrap.cppm` 10, `Sdk/Pack.cppm` 5,
`Vulkan/Instance.cppm` 3, **`Assets/AssetFormat.cppm` 3**, `Runtime/AttachedLink.cppm` 2,
`Os/App.cppm` 2, `Runtime/FramePacing.cppm` 1.

**`gse.assets` is the next arm and it is not close.** Its entire leak is one `MoveFileExW`
atomic-replace at `AssetFormat.cppm:284-289` (three uses, one function, in a 312-line file), and the
sweep shows six modules behind it: `gse` (the umbrella the Sandbox imports), `gse.audio`,
`gse.examples`, `gse.network`, `gse.physics`, plus `gse.assets` itself — all reading
`-> gse.assets -> gse.win32` directly, no intermediate. `gse.physics` and the `gse` umbrella are on
`scene_reset_cpu`'s own path. Three uses for six modules beats `gse.os` (110 uses) and
`gse.process` (46) by an order of magnitude, so those are priced later, separately.

**The known hole is recorded, not forgotten.** `Fs/FileWatcher.cpp` still imports `gse.win32` and is
not under a platform directory, so the selection rule does not cover it; `file_watcher` is a member of
`Assets::AssetState` and will be referenced at a wasm link. ~4 of its 380 lines are Windows. It belongs
with step 4 (assets/VFS) and is deliberately not bundled into the next task. Same for the broader
caveat the task raised and this run endorses: a `translates` row cannot see `.cpp` files, so the sweep
proves the transitive-import blocker is gone, not that the host code is — the first real link is what
will test that, and `Os/Win32/Window.cpp` / `Time/Win32/SystemClock.cpp` will fail there as undefined
symbols rather than as a configure diagnostic.

**Next task:** the `gse.assets` arm of step 2. Move the `MoveFileExW` replace out of
`AssetFormat.cppm` into `Assets/Win32/*.cpp` + `Assets/Web/*.cpp` behind one internal declaration,
same shape as `temp_suffix()`; the web arm is `std::filesystem::rename` with the same
`std::expected` error channel. Judged on: `gse.assets`, `gse`, `gse.physics`, `gse.audio`,
`gse.network` and `gse.examples` reading `translates` in a re-run sweep with `host=` down and
`failed=0` held; native build green and `0xe836d31697879e27` unmoved.

**Carried forward unchanged:** `AsyncTask.cppm:383` and `Parse.cppm:115` hold unconverted handlers the
`-S` sweep cannot see; `thread_local`-address thread ids still unproven past `-S`; `Fs/Web/LayoutStore.cpp`
has never been compiled (no non-Windows configure exists); nothing committed — `.gitattributes`, both
documents, `Sandbox/docs/`, `Time/Win32/` and the new `Fs/Win32/`, `Fs/Web/` all uncommitted or
untracked, the owner's call.

### 2026-10-19 — steering: the `gse.assets` arm judged (holds, buys less than priced); the `gse.gpu` frontier is **one member declaration**, and it is the owner's

**Judged against the pass criteria: still nothing to pass.** (1)-(6) unstarted — nothing linked, no
page, no hash out of a browser.

**Confirmed at source.** `git diff` over `Assets/AssetFormat.cppm` is 36 deletions and nothing else;
the only `gse.win32` import left anywhere under `Assets/` is in `Assets/Win32/AssetFormat.cpp`, a
platform directory the existing `gse_select_platform_impls()` rule already covers, so no CMake change
was needed and none was made. The Win32 arm is the old `MoveFileExW`/`movefile_replace_existing |
movefile_write_through` body with the same `asset_error{io_failure}` channel; the web arm is
`std::filesystem::rename` with `ec.message()` in the same channel. `baked_temporary_id` was not
duplicated. Native green, `world-state hash 0xe836d31697879e27` unmoved.
`/tmp/wasm_sweep_assets.log`: `host=217 ok=205`, `modules: host-bound=35 translates=28`, and zero
lines matching `FAILED|blocked` in the whole file — so `failed=0`/`blocked=0` hold.

**The task's deviation is correct and the record was wrong, not the task.** The previous entry priced
this arm at "six modules unblock" off `module_graph.py`'s `reached` column. `gse.assets` imports
`gse.gpu` (`Assets/AssetState.cppm:9`), so the sweep still reads
`gse.assets -> gse.gpu -> gse.nsight_perf` and no module flipped. **Lesson for pricing every
remaining arm: `reached` counts modules that touch a root, not modules a root *solely* blocks. Price
off `sole`, or off the "what has to be ported together" groups, never off `reached`.** The edit still
stands — it is the settled seam, and step 4 needs `gse.assets` host-free regardless.

**The `gse.gpu` frontier, measured, and it is far narrower than its reservation suggests.** Exactly
one interface unit in the whole engine imports `gse.nsight_perf`: `Gpu/Graph/RenderGraph.cppm:12`.
`gse.aftermath` likewise has one: `Vulkan/Aftermath.cppm`. `gse.directx` is imported only by seven
`Dx12/*.cppm`, which `gse.gpu` does not transitively import (the record's import discipline, holding).
So `gse` and `gse.physics` — both on `scene_reset_cpu`'s path — are blocked on **one `.cppm`**, and
within it on **one line**: `nsight_perf::sample_window samples;` at `RenderGraph.cppm:261`, the only
`nsight_perf::` use in 382 lines.

**Why that is the owner's call and not a seam extraction.** Every leak closed so far was an inline
*implementation* in an interface, movable to a `:*_impl` unit with the exported declaration unchanged
— which is exactly what the charter's seam line authorizes. This is a **data member of an exported
struct**, so it cannot move to an impl unit without changing the struct's layout and its public shape
(pimpl, a variant member, or `#if`). The charter reserves `Gpu/` by path and authorizes the seam by
shape; this is inside the path and outside the shape, so it is escalated rather than decided. It is
also the *whole* remaining question for those two modules — no further `gse.win32` leak can move them.

**Not escalating this run, because in-charter work remains and is on the same path.** `gse.runtime`
is host-bound on `gse.win32` through three units totalling 13 `win32::` uses
(`Runtime/Bootstrap.cppm` 10, `Runtime/AttachedLink.cppm` 2, `Runtime/FramePacing.cppm` 1) — the
engine's boot/host path, which step 3's headless host needs whatever the `gse.gpu` answer is, and the
cheapest arm left by a wide margin (`Os/Process.cppm` 63, `Os/Win32/Clipboard.cppm` 47,
`Process/Process.cppm` 46, `Os/App.cppm` 2). The sweep's groups show 5 modules behind
`gse.win32 + gse.win32.environment` and 2 behind `gse.win32` alone.

**Next:** the `gse.runtime` arm of step 2 — the three units above, same impl-unit shape, priced on
`sole` not `reached`. Then the `RenderGraph.cppm:261` question goes to the owner.

**Carried forward unchanged:** three web arms now exist that nothing has ever compiled
(`Fs/Web/LayoutStore.cpp`, `Assets/Web/AssetFormat.cpp`, and `Fs/Web/`'s sibling) — the count grows
one per leak until a real link exists, and the first link is also what tests `Os/Win32/Window.cpp`
and `Time/Win32/SystemClock.cpp` as undefined symbols rather than as a configure diagnostic;
`AsyncTask.cppm:383` and `Parse.cppm:115` hold unconverted handlers the `-S` sweep cannot see;
`thread_local`-address thread ids unproven past `-S`; `Fs/FileWatcher.cpp` imports `gse.win32`
outside a platform directory, left for step 4; nothing committed, the owner's call.

### 2026-10-19 — steering: the `gse.runtime` arm judged (holds); the frontier is now **four one-unit external leaks**, and the umbrella cannot flip until the owner decides `RenderGraph.cppm:261`

**Judged against the pass criteria: still nothing to pass.** (1)-(6) unstarted — nothing linked, no page,
no hash out of a browser.

**Confirmed at source, not taken from the report.** `grep -rl gse.win32` over
`Engine/Engine/Source/Runtime/` returns only `Win32/{AttachedLink,FramePacing,Platform}.cpp` — all
three under a platform directory the existing `gse_select_platform_impls()` rule covers — plus the
already-recorded `Runtime/Engine.cpp` hole. So the arm's claim ("no `.cppm` under `Runtime/` imports
`gse.win32`") holds exactly as stated. `/tmp/wasm_sweep_runtime.log` reads `host=216 ok=206`,
`modules: host-bound=35 translates=28`, and `grep -cE "FAILED|blocked"` over the whole log is **0**.
Native green, `world-state hash 0xe836d31697879e27` unmoved. Review found no defects. Verdict:
correct, keep. The row not flipping was predicted by the scope and is not a shortfall.

**The sweep's module rows say something the record's step list does not, and it changes the ordering.**
`gse` — the umbrella the Sandbox imports — no longer names `gse.win32` or `gse.gpu` at all. It reads
**`gse -> gse.graphics -> gse.freetype`**. The remaining frontier is not one win32 blob; it is five
roots, and **four of them are each reached through exactly one interface unit**:

| root | sole interface importer | uses |
|---|---|---|
| `gse.nsight_perf` | `Gpu/Graph/RenderGraph.cppm:261` | 1 (a data member — escalated) |
| `gse.freetype` | `Graphics/2D/FontCompiler.cppm:17` | 27 in 302 lines, one `export namespace` |
| `gse.slang` | `GpuRecord/PipelineBuilder.cppm:9` | — |
| `gse.aftermath` | `Vulkan/Aftermath.cppm` | — (path-reserved) |
| `gse.win32` | `gse.os` (110), `gse.process` (46), `gse.sdk` (5) | — |

`gse.freetype`, `gse.slang` and `gse.win32` are vendored/host third-party libraries with no wasm arm;
the leaks are inline implementations in interfaces, i.e. the settled seam shape, extraction not
redesign. **Pricing lesson restated:** `gse` is blocked on `gse.graphics` *today*, but it imports
nearly everything, so closing `FontCompiler.cppm` will simply make it name `gse.gpu ->
gse.nsight_perf` next. The umbrella — and with it `gse.physics`, `gse.assets`, `gse.audio`,
`gse.network`, `gse.examples`, `gse.runtime`, all six on `scene_reset_cpu`'s path — cannot flip until
`RenderGraph.cppm:261` is decided. **That is now the gate, and it is the owner's by path.**

**So the question is being put to the owner this run rather than deferred a third time.** The previous
two entries both deferred it on the grounds that in-charter work remained — and it does (`gse.os` 110
uses, `gse.process` 46, `FontCompiler.cppm` 27, `PipelineBuilder.cppm`) — but that reasoning will hold
again next run and the next, so the decision would never get asked while the thing it gates is every
remaining module on the scenario's path. The owner's pick also directs which arm goes next.

**Carried forward unchanged:** `Runtime/Engine.cpp`, `Fs/FileWatcher.cpp`, `Os/Win32/Window.cpp` and
`Time/Win32/SystemClock.cpp` are `.cpp`-level holes the `-S` sweep cannot see — the first real link is
what tests them; eight `*/Web/*.cpp` arms have never been compiled, two with named-but-unused
parameters; `AsyncTask.cppm:383` and `Parse.cppm:115` hold unconverted handlers nothing instantiates
yet; `thread_local`-address thread ids unproven past `-S`; nothing committed, the owner's call.

### 2026-10-19 — steering (same run, after the owner steered toward a vendor-neutral profiling abstraction): the gate is a *directory test*, and the engine already has the abstraction that pays

The owner declined all five options and asked whether this is the opportunity for a generic GPU-vendor
profiling abstraction. Read the code before answering, and it changes the picture in three ways.

**1. `gse.nsight_perf` is not host-bound because the compiler said so. It is host-bound because of a
`startswith` on its directory.** `Tools/module_graph.py:114`'s `is_host_root` is
`path.startswith("Engine/Engine/Source/External") and module_of(logical) not in PORTED_HOST_ROOTS`.
Nothing compiles it; the sweep short-circuits it into a host-bound row and propagates that to
dependents. The file's own doc comment (`:53-75`) states the rule and its one exception, and it sets a
bar for membership: the wrapper must be portable, *not* portable behind an `#ifdef`, and "nothing
belongs here until a sweep has shown it green".

**2. `NsightPerf.cppm` already meets the spirit of that bar, and the port may need no engine edit at
all.** The 89-line interface imports only `std` and `gse.math`. It exports no vendor-shaped type:
`session_settings`, `session_info`, a `session_status` enum and `sample_window` (two `std::span`s).
It already carries `constexpr bool compiled_in` and a `session_status::not_compiled_in`. Every NVIDIA
header is behind `#ifdef GSE_HAVE_NSIGHT_PERF` in the 782-line `.cpp`, and the `#ifndef` arm is
**complete, not a stub to be written**: `begin_session` returns `not_compiled_in`, `gpu_timestamp`
returns `nullopt`, `decode` returns empty spans, `info`/`end_session` are already neutral. This is
unlike `gse.win32`, which the doc comment correctly excludes — win32 is an empty module on wasm32 that
still gates its callers' `win32::` uses, whereas nsight_perf's callers use only neutral types.
**So `RenderGraph.cppm:261` plausibly needs no change whatsoever**, and the escalated "data member of
an exported struct" problem may not exist. Unmeasured: the sweep has never offered the unit, so green
is a prediction, and the next task must measure it before anything is claimed.

**3. The consumer surface is eight call sites, all already behind the seam.** `Gpu/Device/Device.cpp`
(3), `Gpu/Graph/RenderGraph.cpp` (4) — both impl units — and the one member declaration.
`perf_frame` (`RenderGraph.cppm:260-266`) is itself a neutral aggregate.

**On the owner's actual question, and the answer is not the obvious yes.** The engine *already has* a
generic GPU-vendor timing abstraction and it is not this one:
`gpu::device::create_timestamp_query_pool` (`Gpu/Device/Device.cppm:232`) with real Vulkan and DX12
backends (`DeviceVulkanBackend.cppm:271`, `DeviceDx12Backend.cppm:269`). WebGPU's `timestamp-query`
is a third backend *for that*. `gse.nsight_perf` is a different animal — periodic **counter metric**
sampling (warps-active, SM occupancy) via the NVIDIA periodic sampler. WebGPU exposes no counter
sampling at all, and the AMD/Intel equivalents (RGP, Metrics Discovery) are separate SDKs nothing in
the tree wires. A vendor-neutral *metrics* layer would therefore ship with **one** backend and no
second one in prospect, including in the browser goal that follows this one — which is the shape
`docs/CODE_REVIEW_GUIDE.md` asks us to establish a caller for before building.

Put back to the owner with that framing: the cheap truth (a sweep measurement and a one-tuple change
in a Tools script, no engine edit), the cosmetic promotion (vendor-neutral name, out of `External/`),
the full abstraction now, and the option of a separate goal aimed at the timing abstraction that does
have a second and third backend.

### 2026-10-19 — the owner decided: rename + rehome, the full vendor abstraction, and a separate timing goal

**Decision, not to be relitigated.** The owner chose options 2, 3 and 4 together: promote the module
to a vendor-neutral `gse.gpu_profile` out of `Source/External`, build the real abstraction (neutral
interface + null backend + NVIDIA backend, with per-backend remedy text so the status enum stops
naming nvperf DLLs and the NVIDIA Control Panel), **and** open a separate goal for the generic GPU
*timing* profile over `timestamp_query_pool`. Option 1 (allowlist `gse.nsight_perf` in
`PORTED_HOST_ROOTS`) is not needed and must not be done: moving the module out of
`Source/External` removes the `startswith` misclassification at the root instead of excusing it, which
is strictly better and is why the owner's pick subsumes mine. The `PORTED_HOST_ROOTS` tuple stays at
`("gse.moodycamel",)`.

**The home is settled by existing convention, not invented.** `Source/GpuRecord/` ↔
`gse.gpu_record` ↔ `Import/GpuRecord.cppm` is the engine's pattern for a GPU-adjacent module that is
not `gse.gpu` itself (`GpuBackend/` likewise). So: **`Source/GpuProfile/` ↔ `gse.gpu_profile` ↔
`Import/GpuProfile.cppm`**, with a primary interface that is `export import` of its partitions, exactly
like `Import/GpuRecord.cppm`'s four lines. It must be its own module rather than a `gse.gpu` partition,
or it re-enters `gse.gpu`'s closure and buys nothing.

**Two findings that shape the backend split.** The remedy text has exactly **one** reader in the whole
tree — `NsightPerf.cpp:121`'s `annotation_from_enum(status, status_remedy{})`, inside what becomes the
NVIDIA backend. Since a `[[= status_remedy{...}]]` annotation has to sit on the enum declaration, it
cannot be per-backend; so the neutral enum drops the annotations and the backend owns a plain
`remedy(session_status) -> std::string_view` table. And **backend selection here is not a platform
test**: the NVIDIA arm is selected by `GSE_HAVE_NSIGHT_PERF` (SDK presence, CMake-detected), not by
`/Win32/` or `/Web/`, so `gse_select_platform_impls()` needs a small extension rather than a reuse —
`Engine/cmake/` is charter-authorized for impl-unit selection.

**The risk to watch, and it is the whole review of this task.** Replacing a 782-line file's internal
`#ifdef GSE_HAVE_NSIGHT_PERF` arms with file-level backend selection is a large structural move around
code whose native behaviour must not change. The task proves it rather than argues it: native build
green, `scene_reset_cpu` reprinting `0xe836d31697879e27`, and the `gpu_perf` log lines identical. The
wasm dividend is the measurement that `gse.gpu_profile` translates and that `gse.gpu` and its six
dependents (`gse`, `gse.physics`, `gse.assets`, `gse.audio`, `gse.network`, `gse.runtime`) stop naming
`gse.nsight_perf` — with `RenderGraph.cppm` changing one spelling and `:261`'s member staying
unconditional, as predicted.

**The timing goal is the owner's to open.** Option 4 is a *separate* goal by the owner's own choice, so
it is recorded here and not pursued under this aim: a generic GPU timing profile over
`gpu::device::create_timestamp_query_pool` (Vulkan + DX12 today, WebGPU third in the next goal). It is
surfaced in this run's phase summary so it can be started deliberately rather than absorbed here.

### 2026-10-19 — steering: `gse.gpu_profile` landed and verified structurally; its only measurement was never taken, so the sweep is re-running

**Judged against the pass criteria: still nothing to pass.** (1)-(6) unstarted — nothing linked, no
page, no hash out of a browser.

**Confirmed at source.** `Engine/Engine/Source/GpuProfile/` holds `Backend.cppm`, `Session.cppm`,
`Session.cpp` and two backends (`Null/Backend.cpp`, `Nvidia/Backend.cpp`);
`Engine/Engine/Import/GpuProfile.cppm` is the two-line `export import :session`, matching
`Import/GpuRecord.cppm`'s shape as the owner's decision required. `grep -rl nsight_perf` over
`Engine/Engine/Source` returns **nothing** — the module name, the old `Source/External/NsightPerf.*`
pair and the vendor-named enum remedies are all gone. The native evidence the task reported
(`0xe836d31697879e27` unmoved, `gpu_perf` log line byte-identical) is the right check for a
structural move of this size, and the review found no defects.

**But the change's whole purpose is unverified, and the artifact says so plainly.**
`/tmp/wasm_sweep_gpuprofile.log` is 74KB against ~280KB for every completed sweep, its last write
was 12:29, and at 12:35 no `python`/`cc1plus`/`xg++` process exists. It did not finish and is not
running — it was killed when the task session ended, mid-way through the `engine_meta` probes. There
is **no summary line in it at all** (`grep -cE '^(host=|ok=)'` is 0), so "`gse.gpu_profile`
translates" and "`gse.gpu` plus its six dependents drop their host-bound row" are predictions with
no measurement behind them. The previous entry's reasoning — that moving out of `Source/External`
removes `module_graph.py:114`'s `startswith` misclassification at the root — is sound but is an
argument, and this goal's own rule is that arguments do not count as numbers.

**Lesson worth keeping: a long sweep launched by a task does not survive the task.** Every prior arm
measured because the task waited on its own sweep. This one reported before its evidence existed.
Steering is the phase that can wait (`gse_await`), so the sweep was re-launched here
(`python Tools/build_gcc_wasm.py --sweep` → `/tmp/wasm_sweep_gpuprofile2.log`) and this run waits on
it rather than scoping the next arm blind.

**To be judged on the wake:** `failed=0` and `blocked=0` held; `host=` below 216 (the
`gse.runtime`-arm baseline, `ok=206`); `gse.gpu_profile` reading `translates`; and whether `gse.gpu`,
`gse`, `gse.physics`, `gse.assets`, `gse.audio`, `gse.network` and `gse.runtime` stop naming
`gse.nsight_perf`. Price the *next* arm off `sole`, never `reached` — `gse` will almost certainly
name `gse.graphics -> gse.freetype` (`Graphics/2D/FontCompiler.cppm:17`, 27 uses) the moment the
profiling edge clears, with `gse.slang` (`GpuRecord/PipelineBuilder.cppm:9`), `gse.os` (110) and
`gse.process` (46) behind it.

**New open item from the task's own report, recorded so it is not lost:** the `find_package` hoist in
`Engine/CMakeLists.txt` is order-sensitive — moved back below the source glob it would silently
select the Null backend on a machine that *does* have the Nsight Perf SDK. The no-comments rule
forbids marking it in the file, so it lives here. Also: `Nvidia/Backend.cpp`, the largest moved file,
has had no compiler over it on this machine (`NSIGHT_PERF_SDK` unset) — equally true before the
change, but the move is exactly the kind that a compile would have checked.

**The GPU *timing* goal over `create_timestamp_query_pool` remains the owner's to open**, untouched
here by their own choice.

**Carried forward unchanged:** `Runtime/Engine.cpp`, `Fs/FileWatcher.cpp`, `Os/Win32/Window.cpp` and
`Time/Win32/SystemClock.cpp` are `.cpp`-level holes the `-S` sweep cannot see; eight-plus `*/Web/*.cpp`
arms have never been compiled; `AsyncTask.cppm:383` and `Parse.cppm:115` hold unconverted handlers
nothing instantiates yet; `thread_local`-address thread ids unproven past `-S`; nothing committed,
the owner's call.

### 2026-10-19 — steering, same run: the owner reversed the remedy-table decision. The annotation goes back on the enum, and the engine agrees with them

**The owner read `backend::remedy`'s switch and said it should be an annotation on the enum.** That
reverses one sub-decision of the previous task — and checking the tree, the owner is right and the
task's reasoning was locally sound but parochial.

**Measured.** `annotation_from_enum` has **18+ call sites** across the engine — `Log.cpp:366`,
`Json/Parse.cppm:127`, `Http/Request.cppm:138,144`, `GpuBackend/Image.cppm:268`,
`Ecs/Introspection.cppm:93`, `Gui/Styles.cppm:528`, `Sdk/Pack.cppm:98`, four in `Engine/Code`, and
more. Every one of them attaches a payload struct to an enum declaration and reads it back with a
default. A hand-written 40-line `switch` returning `std::string_view` per enumerator is the *only*
instance of that pattern in the tree, and `status_remedy` — which existed before the move
(`NsightPerf.cpp:121`) — no longer appears anywhere. So the previous task did not choose between two
idioms; it replaced the engine's idiom with a bespoke one and recorded a reason for it.

**The reason it recorded was true and not sufficient.** "An annotation has to sit on the enum
declaration, so it cannot be per-backend" is correct about C++ and wrong about what mattered: there
is one counter-sampling backend, the Null arm can only ever report `not_compiled_in`, and a seam was
added to make a *second* vendor's remedy text selectable when no second vendor exists. That is the
`CODE_REVIEW_GUIDE.md` failure the guide warns about by name — machinery that reads as care, with no
caller whose frequency anyone read. The owner's instruction removes a backend function rather than
adding one.

**Named honestly, because it undoes a stated gain:** putting the text back on `session_status` in the
neutral `:session` partition means the neutral enum again names `nvperf_grfx_host.dll` and the NVIDIA
Control Panel URL — the one thing the owner's earlier option-3 pick called out as wanting fixed. The
owner issued this instruction while looking at those exact strings, so it is an informed reversal and
is not being re-litigated. Keeping the strings verbatim is also the safest option: the `gpu_perf`
log line stays byte-identical, which is the check the move is held to. If the literals should
*themselves* be neutralised (vendor-agnostic phrasing, vendor detail from the backend's own log
line), that is a separate, smaller change and the owner's call to ask for — recorded here so it is
not silently folded in.

**Next task:** restore `status_remedy` as a `[[= status_remedy{...}]]` annotation per enumerator on
`session_status` in `GpuProfile/Session.cppm`, carrying the eight strings exactly as the owner pasted
them (`idle`/`running` keep the default empty); make the remedy lookup
`annotation_from_enum(status, status_remedy{})` at its single reader; and **delete the
`backend::remedy` declaration from `Backend.cppm` and its definition from both `Nvidia/Backend.cpp`
and `Null/Backend.cpp`** — the backend seam drops from five operations to four. Judged on: native
build green, `0xe836d31697879e27` unmoved, and the `gpu_perf` log line for a reachable status
byte-identical to today's.

**Sequencing constraint for that task:** a sweep is already running in this tree
(`/tmp/wasm_sweep_gpuprofile2.log`, started 12:35, rebuilding libstdc++). **Do not start a second
sweep concurrently** — it shares the build directory and module cache. Wait for that one, read its
summary to settle the still-unmeasured `gse.gpu_profile` claim from the entry above, and only then
re-sweep if the annotation change warrants it. `Session.cppm` is a `.cppm` the sweep offers, so the
change does need a sweep eventually; it does not need two.

### 2026-10-19 — steering: the annotation reversal judged (holds); sweep attempt #4, launched detached this time

**Judged against the pass criteria: still nothing to pass.** (1)-(6) unstarted — nothing linked, no
page, no hash out of a browser.

**Confirmed at source.** `GpuProfile/Session.cppm:15` declares `struct status_remedy` and eight
enumerators carry `[[= status_remedy{...}]]` (`not_compiled_in`, `load_failed`, `no_device`,
`unsupported_gpu`, `no_permission`, `no_metrics`, `config_needs_replay`, `start_failed`); `idle` and
`running` carry none. `Session.cpp:28` reads it with `annotation_from_enum(status, status_remedy{})`
into a named local and logs `remedy.text` at `:35`. `grep` for `remedy` over `Backend.cppm`,
`Null/Backend.cpp` and `Nvidia/Backend.cpp` returns **nothing** — the fifth backend operation is
gone, as the owner's instruction required. The `char text[160]` payload is the right call for a
structural annotation and mirrors `http::error_info`. Native green and `0xe836d31697879e27` unmoved
per the task; the warning line was observed under a one-run `--engine-setting` override rather than
argued, which is the correct standard. Verdict: correct, keep.

**The measurement is still missing, and the reason is now diagnosed.**
`/tmp/wasm_sweep_gpuprofile2.log` is 32KB, last written 12:36, and no `python`/`cc1plus`/`xg++`
process exists — it died at the probe stage, the fourth consecutive sweep to be killed with the
session that launched it. Two of the four were launched by *this* phase, so "steering can wait on it"
was not sufficient on its own: the process was still a child of the chat's shell and went down with
it. This attempt is launched **detached** (`nohup ... & disown`, log `/tmp/wasm_sweep_gpuprofile3.log`)
so it outlives the turn, and this run `gse_await`s the log file rather than scoping anything.

**To be judged on the wake, unchanged from the last entry:** `failed=0` and `blocked=0` held;
`host=` below 216 (`ok=206`, the `gse.runtime`-arm baseline); `gse.gpu_profile` reading `translates`;
and whether `gse.gpu`, `gse`, `gse.physics`, `gse.assets`, `gse.audio`, `gse.network` and
`gse.runtime` stop naming `gse.nsight_perf`. Price the next arm off `sole`, never `reached`.

**Carried forward unchanged:** `Nvidia/Backend.cpp` has still had no compiler over it (no Nsight Perf
SDK here) — a whole-function removal with no surviving references, but unbuilt; the `find_package`
hoist in `Engine/CMakeLists.txt` is order-sensitive and unmarkable in-file; the neutral partition
again names `nvperf_grfx_host.dll` and the Control Panel URL, by the owner's informed instruction;
`Runtime/Engine.cpp`, `Fs/FileWatcher.cpp`, `Os/Win32/Window.cpp` and `Time/Win32/SystemClock.cpp`
are `.cpp`-level holes the `-S` sweep cannot see; the `*/Web/*.cpp` arms have never been compiled;
`AsyncTask.cppm:383` and `Parse.cppm:115` hold unconverted handlers nothing instantiates yet;
`thread_local`-address thread ids unproven past `-S`; nothing committed, the owner's call.

### 2026-10-19 — steering: fifth sweep death diagnosed — it is not the sweep, it is `nohup`; relaunched through the harness

**Judged against the pass criteria: still nothing to pass.** (1)-(6) unstarted — nothing linked, no
page, no hash out of a browser. **And the `gse.gpu_profile` measurement is still not taken**, now for
the fifth consecutive attempt, so the claims from three entries ago ("`gse.gpu_profile` translates",
"`gse.gpu` and its six dependents stop naming `gse.nsight_perf`") remain predictions with no number
behind them. Nothing new is scoped this run, because scoping the next arm off an unmeasured tree is
the mistake the last four entries each made in a different costume.

**The cause is now read rather than guessed, and the previous entry's fix was aimed at the wrong
thing.** `/tmp/wasm_sweep_gpuprofile3.log` is 8KB, last written 13:11 — *one minute* after the 13:10
launch. It does not die of duration and it does not die in the sweep: its last lines are
`make all-target-libgcc` reporting `Nothing to be done for 'all'`, i.e. it was still walking the
toolchain rebuild that `--sweep` unconditionally performs before it compiles a single engine unit
(`Tools/build_gcc_wasm.py:1827-1838`: `fetch_source` → `apply_patches` → `check_asm_features` →
`fetch_sysroot` → configure → make, with **no flag to skip any of it** — `--sweep` is documented as
"*after the probes*"). All seventeen patches report `already applied` and the makes are no-ops, so the
work left was libstdc++ and then the probes.

**`nohup ... & disown` was the error.** The previous entry reasoned that four sweeps "died with their
session" and that detaching would fix it; it did not, because the whole process tree goes down at turn
end regardless of the signal `nohup` ignores. The tool for this is the harness's own tracked
background execution, which is specified to keep running across turns and to re-invoke on exit — so
the sweep is relaunched that way (`/tmp/wasm_sweep_gpuprofile4.log`, task `btgxtdsy5`) and this run
waits on it. **Lesson, and it is a general one: do not hand-roll process survival in this tree; the
harness owns the lifetime, and `nohup` buys nothing against it.**

**Recorded as a real cost, for the owner and for whoever next touches the Tools script:** every sweep
re-walks fetch/patch/configure/make before measuring anything, which is why a measurement that should
be minutes has consumed five attempts. A `--sweep-only` arm that trusts an existing `--prefix`
toolchain is squarely in charter (`Tools/build_gcc_wasm.py` is authorized) and is the obvious fix, but
it is a code change and so belongs in a task, not in a steering run. It is not scoped yet because the
*next* measurement is the one that matters; it should be bundled with whatever arm goes next.

**To be judged on the wake, unchanged for the fourth time:** `failed=0` and `blocked=0` held; `host=`
below 216 (`ok=206`, the `gse.runtime`-arm baseline); `gse.gpu_profile` reading `translates`; and
whether `gse.gpu`, `gse`, `gse.physics`, `gse.assets`, `gse.audio`, `gse.network` and `gse.runtime`
stop naming `gse.nsight_perf`. Price the next arm off `sole`, never `reached` — candidates already
priced: `gse.freetype` via `Graphics/2D/FontCompiler.cppm:17` (27 uses), `gse.slang` via
`GpuRecord/PipelineBuilder.cppm:9`, `gse.os` (110), `gse.process` (46).

**Carried forward unchanged:** `Nvidia/Backend.cpp` still has had no compiler over it; the
`find_package` hoist in `Engine/CMakeLists.txt` is order-sensitive and unmarkable in-file; the neutral
partition names `nvperf_grfx_host.dll` and the Control Panel URL by the owner's informed instruction;
`Runtime/Engine.cpp`, `Fs/FileWatcher.cpp`, `Os/Win32/Window.cpp` and `Time/Win32/SystemClock.cpp` are
`.cpp`-level holes the `-S` sweep cannot see; the `*/Web/*.cpp` arms have never been compiled;
`AsyncTask.cppm:383` and `Parse.cppm:115` hold unconverted handlers nothing instantiates yet;
`thread_local`-address thread ids unproven past `-S`; nothing committed, the owner's call.

### 2026-10-19 — steering: sixth sweep death, and the diagnosis finally lands — the toolchain the sweep rebuilds is *already built*, so the rebuild is pure overhead and it is the bug

**Judged against the pass criteria: still nothing to pass.** (1)-(6) unstarted — nothing linked, no
page, no hash out of a browser. **The `gse.gpu_profile` measurement is still not taken**, now for the
sixth consecutive attempt, so the claims from four entries ago ("`gse.gpu_profile` translates",
"`gse.gpu` and its six dependents stop naming `gse.nsight_perf`") remain predictions. Nothing new is
scoped off the unmeasured tree.

**Attempt six died the same way, and the previous entry's two diagnoses were both wrong.**
`/tmp/wasm_sweep_gpuprofile4.log` is 10.8KB, last written 14:13 against a ~13:2x launch, no live
`python`/`cc1plus`/`xg++`. The launch mechanism was *not* the problem: this one went through the
harness's own tracked background execution, exactly as the last entry prescribed, and still did not
survive. And the content is not "make no-ops": the log carries **ten** `make[1]: Leaving directory
.../build/gcc` blocks spanning ~50 minutes of real work. All seventeen patches report `already
applied` and it never reached the probes. So `--sweep` is not dying of its measurement; it is dying
inside the toolchain walk, which is long, and no launch mechanism outlives it.

**Read the tree instead of the log, and the whole walk turns out to be unnecessary.** The sweep's
`Toolchain` (`Tools/build_gcc_wasm.py:1891-1901`) is constructed entirely out of the **build
directory** — `build/gcc`, `build/wasm32-unknown-none/libgcc`,
`build/wasm32-unknown-none/libstdc++-v3` — and every one of those artifacts exists and is complete:
`gcc/cc1plus.exe` (554MB) and `gcc/xg++.exe` both dated **2026-10-17 20:50**, `libgcc.a`,
`libstdc++.a` and `libstdc++convenience.a` all present, `.configure-args` matching the pinned
`gcc-17-20260809` / `wasm32-unknown-none`. **Correcting my own prior note:** the last entry proposed a
`--sweep-only` that "trusts an existing `--prefix` toolchain" — there is no such toolchain.
`.gcc-wasm/` is empty; nothing is ever installed to the prefix, and a `--sweep-only` keyed on it
would find nothing. It must trust the **build dir**.

**So the fix is not to survive the rebuild, it is to not do it**, and it stops being an optional
bundle and becomes the gate. Six attempts, five launch mechanisms and most of a day have gone to
re-walking a toolchain that was finished two days ago. `Tools/build_gcc_wasm.py` is charter-authorized
and this is squarely inside it.

**Next task — `--sweep-only`, then the measurement it unblocks.** Add an arm that skips
`fetch_source` / `apply_patches` / `check_asm_features` / `fetch_sysroot` / configure / all three
`make` invocations, constructs the same `Toolchain` from the existing `--workdir`'s `build` tree, and
goes straight to `sweep(tools)`. It must **fail loudly rather than silently measure a stale or absent
toolchain**: verify `build/gcc/xg++.exe`, `build/gcc/cc1plus.exe`, `libgcc.a` and `libstdc++.a` exist
and that `.configure-args` matches what `configure_args()` would produce for the given snapshot —
a mismatch is the `stale()` failure this script already takes seriously everywhere else, and it is
the one way `--sweep-only` could report a number from the wrong compiler. `fetch_sysroot` is the one
step that may still be needed for the sysroot path; if it is cheap and idempotent keep it, if it
refetches, resolve the path from the workdir instead. `harness.mjs` is written unconditionally and is
cheap — keep it. Do not touch the `--sweep` path's own behaviour.

Then run `--sweep-only` **in the same task** and report its summary, because a task that reports
before its evidence exists is precisely what three of the last four entries did. Judged on the pass
line carried unchanged since the `gse.gpu_profile` move: `failed=0` and `blocked=0` held; `host=`
below 216 (`ok=206`, the `gse.runtime`-arm baseline); `gse.gpu_profile` reading `translates`; and
whether `gse.gpu`, `gse`, `gse.physics`, `gse.assets`, `gse.audio`, `gse.network` and `gse.runtime`
stop naming `gse.nsight_perf`. No engine edit in this task — it is a Tools change plus a measurement,
so the native hash cannot move and is not a check here.

**The next step-2 arm is deliberately not scoped this run.** Pricing it needs the `sole` column from
a sweep that has never completed, and pricing off `reached` has already misled two entries. Candidates
stand as priced: `gse.freetype` via `Graphics/2D/FontCompiler.cppm:17` (27 uses), `gse.slang` via
`GpuRecord/PipelineBuilder.cppm:9`, `gse.os` (110), `gse.process` (46).

**Carried forward unchanged:** `Nvidia/Backend.cpp` still has had no compiler over it; the
`find_package` hoist in `Engine/CMakeLists.txt` is order-sensitive and unmarkable in-file; the neutral
partition names `nvperf_grfx_host.dll` and the Control Panel URL by the owner's informed instruction;
`Runtime/Engine.cpp`, `Fs/FileWatcher.cpp`, `Os/Win32/Window.cpp` and `Time/Win32/SystemClock.cpp` are
`.cpp`-level holes the `-S` sweep cannot see; the `*/Web/*.cpp` arms have never been compiled;
`AsyncTask.cppm:383` and `Parse.cppm:115` hold unconverted handlers nothing instantiates yet;
`thread_local`-address thread ids unproven past `-S`; nothing committed, the owner's call.

### 2026-10-19 — steering: `--sweep-only` judged (holds), the number finally exists, and the frontier is five roots of which three are in charter

**Judged against the pass criteria: still nothing to pass.** (1)-(6) unstarted — nothing linked, no
page, no hash out of a browser.

**Confirmed from the artifact, not the report.** `/tmp/wasm_sweep_only.log` (147KB, 15:07) completed
and carries a summary: **`host=215 ok=209`, `modules: host-bound=34 translates=29`**, and
`grep -cE 'FAILED|blocked'` over the whole file is **0**. `nsight` appears nowhere in it, and the
compiler line names `.gcc-wasm-build/build/gcc/xg++.exe` — the build-dir toolchain the arm was
written to trust. So the claim that has been a prediction for five entries is now a measurement:
`gse.gpu_profile` translates, the profiling edge is gone, and `failed=0`/`blocked=0` hold. `host=`
216 → 215, `ok=` 206 → 209. The diff is one file (`Tools/build_gcc_wasm.py`, +254/−72), no engine
edit, so the native hash cannot have moved and is correctly not a check. Verdict: correct, keep —
and the six-attempt measurement outage is closed.

**The predicted six-module flip did not happen, as the task said. The reason is the useful part:**
with the profiling edge gone those modules simply name the next root. The `sole` column is now almost
all zeros — only `gse.win32` has `sole 2` — so there is no single cheap root left; the frontier is
**five roots in three groups**, read from the log's own "what has to be ported together":

| root | reached | chain the log prints | in charter? |
|---|---|---|---|
| `gse.win32` / `.environment` | 22 / 19 | `gse.os`, `gse.process`, `gse.sdk`, `gse.network`, 4 test modules | **yes** (`Os/**` by path, the rest by seam shape) |
| `gse.freetype` / `gse.msdfgen` | 5 / 5 | `gse.graphics -> gse.freetype`, and `gse`, `gse.devtools`, `gse.examples`, `gse.runtime` behind it | yes, by seam shape (`Graphics/` is not a reserved path) |
| `gse.slang` | 7 | `gse.gpu_record -> gse.slang`, `gse.physics` behind it | yes, by seam shape |
| `gse.directx` | 14 | `gse.gpu -> gse.dx12 -> gse.directx`, `gse.assets`, `gse.audio` behind it | **no — `Dx12/`, owner's by path** |
| `gse.aftermath` | 14 | `gse.vulkan -> gse.aftermath` | **no — `Vulkan/`, owner's by path** |

**Stated plainly so it is not rediscovered a third time: criterion (1) is not reachable without the
owner.** `scene_reset_cpu` needs the `gse` umbrella, the umbrella needs `gse.gpu`, and `gse.gpu` is
host-bound through `gse.dx12 -> gse.directx` — backend partitions *declared* rather than selected,
which is `Gpu/`+`Dx12/` and reserved by path. The import discipline is holding (no consumer imports
the DX12 backend); what taints the row is that `module_graph.py` marks per module and one partition
of `gse.gpu` is host-bound. **Not escalating this run**, because three of the five roots are in
charter and one of them is the record's own step 2 on the scenario's host path — but this is the last
entry that can say that about `gse.gpu` and mean it, and the backend-selection question should go to
the owner as soon as the win32 arm lands.

**Next task: the `gse.os` arm of step 2 — the record's table reproduces exactly.** Three interface
units import `gse.win32`: `Os/Process.cppm` (63 `win32::` uses), `Os/Win32/Clipboard.cppm` (47, and
it also imports `gse.win32.environment`), `Os/App.cppm` (2). Same shape as every arm so far: move the
inline bodies into `:process_impl` / `:clipboard_impl` / `:app_impl` beside the existing
`:window_impl`, Win32 arm under `Os/Win32/`, web arm under `Os/Web/`, selected by the existing
`gse_select_platform_impls()` — no CMake change should be needed, since both directories already
match the rule. The open question from the record stands and must be *answered in the task, not
assumed*: whether `:app`'s exported API (`std::wstring`, `std::span<void* const> handles` for
relaunch handoff, `App.cppm:24-31`) survives the move unchanged; if it is a Windows *concept* rather
than a Windows spelling, the web arm reports unsupported through the caller's existing error channel
and that is reported as a deviation. Judged on: `gse.os` reading `translates` in a re-run
`--sweep-only`, `host=` below 215 with `failed=0`/`blocked=0` held, and the native build green with
`0xe836d31697879e27` unmoved. Price any follow-on arm off `sole` and off the "ported together"
groups, never `reached`.

**New open item:** `--sweep-only` reports no probe verdicts, so the probe regression table has been
unmeasured since the `gse.gpu_profile` move. One full `--sweep` should be run detached at some point;
it is not worth blocking an arm on.

**Carried forward unchanged:** `Nvidia/Backend.cpp` still has had no compiler over it; the
`find_package` hoist in `Engine/CMakeLists.txt` is order-sensitive and unmarkable in-file; the neutral
profiling partition names `nvperf_grfx_host.dll` and the Control Panel URL by the owner's informed
instruction; `Runtime/Engine.cpp`, `Fs/FileWatcher.cpp`, `Os/Win32/Window.cpp` and
`Time/Win32/SystemClock.cpp` are `.cpp`-level holes the `-S` sweep cannot see; the `*/Web/*.cpp` arms
have never been compiled; `AsyncTask.cppm:383` and `Parse.cppm:115` hold unconverted handlers nothing
instantiates yet; `thread_local`-address thread ids unproven past `-S`; nothing committed — including
`Tools/build_gcc_wasm.py` — the owner's call.

### 2026-10-19 — steering: the `gse.os` arm judged (holds, largest arm yet); the one rejection is a real error channel but is unreachable on the wasm arm, so it converts

**Judged against the pass criteria: still nothing to pass.** (1)-(6) unstarted — nothing linked, no page,
no hash out of a browser.

**Confirmed from the artifacts, not the report.** `grep -rl gse.win32` over `Engine/Engine/Source/Os/`
returns only `Win32/{App,Clipboard,Process,Window}.cpp` — four platform-directory impl units the
existing `gse_select_platform_impls()` rule covers, and no `.cppm` at all. `/tmp/wasm_sweep_os.log`
(186KB, 15:35) summary line reads **`blocked=2 failed=1 host=156 ok=265`**, `modules: FAILED=1
host-bound=28 translates=34`, with the single failure row
`gse.graphics FAILED 1/115 units  Engine/Engine/Source/Graphics/3D/Model.cppm`. Against the
`host=215 ok=209`, `translates=29` baseline that is **59 units off the host frontier** — by a wide
margin the largest arm of the port. Native green and `0xe836d31697879e27` unmoved per the task; the
review read all three extractions against the deleted bodies and found nothing. Verdict: correct, keep.

**The deviation is correctly reported and is exposure, not regression.** `Graphics/3D/Model.cppm` sat
behind the `gse.os` edge and had never been offered to the compiler; it instantiates
`load_baked<model::baked>`, whose body carries the `try` at `AssetFormat.cppm:155` with handlers at
`:176` and `:183`. Same instantiation-driven mechanism the ledger records at `Task.cppm` and
`ScopeExit.cppm`: a `catch` is a blocker only once something on the path instantiates it. The two
`blocked` units are downstream of that one rejection.

**Read at source, because it decides the conversion, and the task was right to decline a reflex `#if`
but the answer still comes out the same.** Unlike the three converted sites this *is* a real error
channel: both handlers return `std::unexpected(asset_error{invalid_format, path, ...})`, and
`error.what()` is carried into the detail string. But what the handlers catch is what `*opened & out`
and `opened->valid()` throw — and under `-fno-exceptions` nothing inside the `try` can throw at all,
so on that arm the handlers are unreachable by construction, exactly as `run_job`'s were. The caller's
error channel is **not** lost on wasm either: the explicit `invalid_format` return at `:157-163` for a
truncated or malformed payload is inside the `try` and survives, so a bad asset is still reported
through `asset_result` on both arms; what disappears is only the *exception-sourced* detail text,
which cannot occur. `#if __cpp_exceptions` with the `#else` arm as the straight-line body is therefore
the honest translation and leaves the native arm textually unchanged.

**Next task: walk the chain to `failed=0`, as the `ScopeExit` entry taught.** Convert
`AssetFormat.cppm:155-189`, re-run `--sweep-only`, and if it names another instantiated site on the
same path convert that one too in the same task — each decided on its own merits and reported, and
escalating rather than guessing if a site's handler is a channel that is *reachable* on the
`-fno-exceptions` arm (this one is not). Judged on: `failed=0` and `blocked=0`, `host=` below 156,
`gse.graphics` reading `translates`; native build green and `0xe836d31697879e27` unmoved.

**The frontier after this, priced off the log's groups and `sole`, never `reached`:** `gse.win32`
keeps two interface importers — `gse.process` (46 uses) and `gse.sdk` (5) — with 4 modules behind
`gse.win32`+`.environment` and 2 behind `gse.win32` alone; then `gse.freetype`/`gse.msdfgen` via
`Graphics/2D/FontCompiler.cppm:17` (27 uses) and `gse.slang` via `GpuRecord/PipelineBuilder.cppm:9`.
**And the standing block is unchanged and still the owner's:** `gse.gpu` is host-bound through
`gse.dx12 -> gse.directx` and `gse.vulkan -> gse.aftermath`, both reserved by path, so criterion (1)
is not reachable without a decision on backend partition selection. That question goes to the owner
as soon as the win32 arm lands, as the previous entry said.

**New open items from this arm:** `Os/Web/App.cpp` carries the POSIX `readlink`/`execv` relaunch body
verbatim and will not compile for wasm32 — kept deliberately, since deleting a native path to clear
the wasm build is the owner's call and the first real link is where that decision has facts;
`Os/Win32/Window.cpp` is still the only `:window_impl`, so a wasm link has no window arm at all; the
`*/Web/*.cpp` arms are now ten and none has ever been compiled.

**Carried forward unchanged:** `--sweep-only` reports no probe verdicts, so the probe table has been
unmeasured since the `gse.gpu_profile` move; `Nvidia/Backend.cpp` still has had no compiler over it;
the `find_package` hoist in `Engine/CMakeLists.txt` is order-sensitive and unmarkable in-file;
`Runtime/Engine.cpp` and `Fs/FileWatcher.cpp` are `.cpp`-level holes the `-S` sweep cannot see;
`AsyncTask.cppm:383` and `Parse.cppm:115` hold unconverted handlers nothing instantiates yet;
`thread_local`-address thread ids unproven past `-S`; nothing committed, the owner's call.

### 2026-10-19 — steering: the `AssetFormat` conversion judged (holds); every in-charter arm now leads back to one reserved question, so it goes to the owner

**Judged against the pass criteria: still nothing to pass.** (1)-(6) unstarted — nothing linked, no page,
no hash out of a browser.

**Confirmed from the artifact, not the report.** `git diff` over `Assets/AssetFormat.cppm` adds exactly
three preprocessor lines around the `try`/`catch` in `load_baked` (`#if __cpp_exceptions` at the `try`,
`#endif`/`#if` bracketing the closing brace, `#endif` after the second handler); no statement moved, so
the native arm is textually the old code and `0xe836d31697879e27` is a check on that rather than the
argument for it. `/tmp/wasm_sweep_assetformat.log` (187KB, 15:54) completed and reads
**`host=156 ok=268`, `modules: host-bound=29 translates=34`**, with `grep -cE 'FAILED|blocked'` over the
whole file **0**. `gse.assets` reads `translates 10 units`. Verdict: correct, keep — the exceptions
frontier is closed again, and for the second time the chain was one link.

**The task's reported deviation is confirmed as exposure, not shortfall.** `gse.graphics` no longer
`FAILED`; it reads `gse.graphics -> gse.freetype`, a root the rejection had been masking, and its units
were already inside `host=`, so `host=` did not move. Also endorsed: leaving `Meta/Parse.cppm:115`
alone. Its handler *is* reachable on the no-exceptions arm (`std::stod` aborts), so the fix there is
`std::from_chars`, not an `#if`, and nothing instantiates it yet.

**The frontier, read off this log's own tables, and it is the reason this run escalates.** `sole` is now
`gse.win32` 2, `gse.directx` 1, everything else 0; the "ported together" groups are
`{aftermath, directx, win32, win32.environment}` 6 modules, `{+freetype, msdfgen, slang}` 5 modules,
`{win32, win32.environment}` 4, `{win32}` 2, `{directx}` 1. **Every group that contains the `gse`
umbrella contains `gse.directx` and `gse.aftermath`.** So the three remaining in-charter arms —
`gse.freetype` via `Graphics/2D/FontCompiler.cppm:17` (27 uses), `gse.slang` via
`GpuRecord/PipelineBuilder.cppm:9`, `gse.process` (46) + `gse.sdk` (5) — can each be done and none of
them, nor all three, makes `scene_reset_cpu`'s module closure translate. Criterion (1) is gated on a
reserved path and nothing else will change that.

**And the shape of that gate is now measured rather than assumed.** The import discipline is holding:
`Gpu/Device/DeviceDx12Backend.cppm` is the only `.cppm` under `Gpu/` naming `gse.dx12`, and no consumer
imports it — but it is a **partition of `gse.gpu`**, so the module is host-bound as a whole, and the
existing `gse_select_platform_impls()` filters `.cpp` only, by directory, deliberately leaving `.cppm`
alone. It is therefore not a measurement artifact that a smarter `module_graph.py` could paper over: a
real wasm link has no way to *not* build that partition today. Fixing it means selecting interface
partitions per configuration, which is `Gpu/` + `Dx12/` + `Vulkan/` by path — reserved.

**Escalating, with a recommendation, and not deferring it a fourth time.** The last three entries each
deferred this on the grounds that in-charter work remained; it does, but the tables above show that work
cannot reach the aim, and this chat has five steering runs left before the owner is asked anyway.
Recommended: extend the CMake impl-unit selection to cover backend **interface partitions** the same way
it covers `.cpp` — native keeps compiling both Vulkan and DX12 partitions, so native behaviour and the
`gpu_dispatch` selection at `Device.cpp:52` are untouched, and the wasm configuration simply omits them.
That is the smallest change that keeps the settled seam and does not delete a native path.

**Carried forward unchanged:** `AsyncTask.cppm:383` and `Parse.cppm:115` hold handlers nothing
instantiates, neither a free `#if`; ten `*/Web/*.cpp` arms have never been compiled and `Os/Web/App.cpp`
carries a POSIX `readlink`/`execv` body that cannot build for wasm32; `Runtime/Engine.cpp`,
`Fs/FileWatcher.cpp`, `Os/Win32/Window.cpp` and `Time/Win32/SystemClock.cpp` are `.cpp`-level holes the
`-S` sweep cannot see; `--sweep-only` reports no probe verdicts, so the probe table has been unmeasured
since the `gse.gpu_profile` move; `Nvidia/Backend.cpp` still has had no compiler over it; nothing is
committed, the owner's call.

### 2026-10-19 — the owner granted the reserved permission ("thats fine"): backend interface partitions may be selected per configuration

**Decision, recorded so no later run re-asks.** The escalation asked for permission to select backend
**interface partitions** per configuration in `Engine/cmake/`, touching `Gpu/`, `Dx12/` and `Vulkan/`
by path. The owner approved the recommended option: extend the existing selection rule to cover
backend `.cppm` the way it covers `.cpp`. **Native keeps compiling both the Vulkan and DX12 partitions
unchanged** — the `gpu_dispatch` selection at `Device.cpp:52` is untouched — and only the wasm
configuration omits them. Nothing native is deleted or disabled; that remains reserved.

**The thing the escalation did not say, found while scoping, and it is the whole risk of the task.**
There are *two* enumerators of `.cppm` in this tree and they are independent. CMake builds the link;
`Tools/module_graph.py` builds the sweep's unit list by walking the source tree, and
`Tools/build_gcc_wasm.py` offers every declared `.cppm` to the compiler (`:1897` even *errors* on a
`.cppm` that declares no module). So a CMake-only change moves no sweep number at all:
`DeviceDx12Backend.cppm` would still be offered, `gse.gpu` would still read
`-> gse.dx12 -> gse.directx`, and the task would report a prediction again — the failure mode this
ledger has already recorded five times. The two sides must agree, and from **one** declaration of the
excluded set, not two copies of a directory test — `module_graph.py:108`'s own docstring records that
exact lesson about `is_host_root`.

**Measured starting point:** `Gpu/Device/DeviceDx12Backend.cppm:9` is the sole `.cppm` under `Gpu/`
importing `gse.dx12`, and `Vulkan/Aftermath.cppm:3` the sole importer of `gse.aftermath`. The CMake
side already has the shape to extend: `gse_select_platform_impls()`
(`Engine/cmake/GSEEngine.cmake:42-59`, `.cpp`-only by deliberate comment) and
`gse_select_backend_impls()` beside it, the latter already selecting by a non-platform axis
(`Nvidia`/`Null`, SDK presence) and so the better precedent for an axis that is not `/Win32/` vs
`/Web/`.

**Next task: the backend interface-partition selection, both sides, with the number.** Pick a single
declaration of which interface partitions a non-Windows/wasm configuration omits; have CMake drop them
from the engine target and `module_graph.py`/`build_gcc_wasm.py` drop the same units from the sweep,
reading the same declaration rather than re-implementing the test. Then run `--sweep-only` **in the
same task** and report its summary — a task that reports before its evidence exists is the mistake
five entries here already record. Judged on: `gse.gpu` and `gse.vulkan` no longer naming
`gse.directx`/`gse.aftermath`; `host=` below 156 with `failed=0`/`blocked=0` held; and whether the
`gse` umbrella, `gse.physics`, `gse.assets` and `gse.audio` move off the host frontier. Native is the
other half of the check and is not optional for a change in `Engine/cmake/`: native build green and
`0xe836d31697879e27` unmoved, which is what proves "native compiles both partitions, unchanged".

**Expected to remain after it:** the three in-charter arms already priced — `gse.freetype`
(`Graphics/2D/FontCompiler.cppm:17`, 27 uses), `gse.slang` (`GpuRecord/PipelineBuilder.cppm:9`),
`gse.process` (46) + `gse.sdk` (5) — then step 3's headless host, which is where the `.cpp`-level
holes below stop being invisible.

**Carried forward unchanged:** `AsyncTask.cppm:383` and `Parse.cppm:115` hold handlers nothing
instantiates, and `Parse.cppm`'s fix is `std::from_chars`, not an `#if`; ten `*/Web/*.cpp` arms have
never been compiled and `Os/Web/App.cpp` carries a POSIX `readlink`/`execv` body that cannot build for
wasm32; `Runtime/Engine.cpp`, `Fs/FileWatcher.cpp`, `Os/Win32/Window.cpp` and
`Time/Win32/SystemClock.cpp` are `.cpp`-level holes the `-S` sweep cannot see; `--sweep-only` reports
no probe verdicts, so the probe table has been unmeasured since the `gse.gpu_profile` move;
`Nvidia/Backend.cpp` still has had no compiler over it; the `find_package` hoist in
`Engine/CMakeLists.txt` is order-sensitive and unmarkable in-file; nothing is committed, the owner's
call.

### 2026-10-19 — steering: the backend-interface selection judged (holds, `gse.gpu` translates); the frontier is three arms, one of which is a single unused import line

**Judged against the pass criteria: still nothing to pass.** (1)-(6) unstarted — nothing linked, no page,
no hash out of a browser.

**Confirmed from the artifacts, not the report.** `Engine/cmake/native_gpu_backend_interfaces.txt`
lists the three `Gpu/Device/` partitions (`DeviceVulkanBackend.cppm`, `DeviceDx12Backend.cppm`,
`VideoBackend.cppm`) and is read by both enumerators —
`gse_select_backend_interfaces` (`Engine/cmake/GSEEngine.cmake:96`, called from
`Engine/CMakeLists.txt:16`, right after the glob) and `module_graph.py:86`'s
`EXCLUDED_INTERFACES_LIST` — so the "one declaration, two readers" requirement the previous entry
made the whole risk of the task is met in the form that cannot diverge. The diff is three files plus
the list; no engine source, so the unmoved `0xe836d31697879e27` is consistent by construction rather
than evidence. `/tmp/wasm_sweep_backend_interfaces.log` (186KB, 18:07) completed:
**`host=153 ok=268`, `modules: host-bound=28 translates=35`**, `grep -cE 'FAILED|blocked'` **0**, and
`gse.gpu` reads `translates 19 units`. Against the `host=156 ok=268`, `translates=34` baseline that is
the one intended row and nothing else. Verdict: correct, keep. The two unmet halves of the judging
line are as the task reported — `gse.vulkan -> gse.aftermath` stands because `Vulkan/Device.cppm`
imports it, and `gse.assets`/`gse.audio`/`gse.network` were already translating.

**The frontier, off this log's own tables (`sole` and the groups, never `reached`):** `gse.win32`
sole 2, `gse.slang` sole 2, `gse.directx` sole 1, everything else 0. Groups: 5 modules behind
`{win32, win32.environment}`, 4 behind `{freetype, msdfgen, slang}`, 2 behind `{slang}`, 2 behind
`{win32}`, 1 behind `{freetype, msdfgen, slang, win32, win32.environment}` — that last one is the
`gse` umbrella, which is why no single arm flips it. Three arms, all in charter.

**Priced at source, and the slang arm is nearly free.** `GpuRecord/PipelineBuilder.cppm:9` imports
`gse.slang` and **uses nothing from it** — zero `slang::`, zero `Slang`/`SLANG` identifiers (the one
`grep` hit is the word "Slang" inside a diagnostic string), and the module's 28 real `slang::` uses
are all in `PipelineBuilder.cpp`, already an impl unit. It is the sole importer in the module, so
deleting one line should take `gse.gpu_record` and `gse.physics` to `translates`. The freetype arm is
the ordinary seam shape: `Graphics/2D/FontCompiler.cppm` is the **sole** importer of `gse.freetype`
*and* `gse.msdfgen` in the whole engine, 302 lines with 39 `FT_`/`msdfgen::` uses inline in the
interface — extract to a `:*_impl` unit with a web arm, same as every arm since `gse.fs`.

**Next task: both freetype/msdfgen and slang, one sweep.** Delete the unused `import gse.slang` from
`PipelineBuilder.cppm` (and verify at the compiler, not by grep, that nothing in the interface needed
it); move `FontCompiler.cppm`'s inline FreeType/msdfgen body into an impl unit beside it with a Win32/
native arm and a web arm selected by the existing `gse_select_platform_impls()`, leaving the exported
API unchanged — and report a deviation rather than inventing one if some of that API is a FreeType
*concept* rather than a FreeType spelling. Judged on: `gse.gpu_record`, `gse.physics`,
`gse.graphics`, `gse.devtools`, `gse.examples` and `gse.runtime` reading `translates` in a re-run
`--sweep-only`; `host=` below 153 with `failed=0`/`blocked=0` held; native build green and
`0xe836d31697879e27` unmoved. Run the sweep **in the same task** — six entries here record what
happens otherwise.

**Expected to remain after it:** the `gse.win32` arm (`gse.process` 46 uses, `gse.sdk` 5), which the
umbrella also needs, and then step 3's headless host — where the `.cpp`-level holes stop being
invisible.

**Carried forward unchanged:** the CMake drop path has never executed (only its Windows no-op arm is
measured) and carries two one-assertion gaps the review named — exact-string path matching, and a
comments-only listing being a CMake argument error; `python Tools/module_graph.py` standalone now
omits three units from its native table; `Gpu/Device/VideoEncoder.cpp` joins `Runtime/Engine.cpp`,
`Fs/FileWatcher.cpp`, `Os/Win32/Window.cpp` and `Time/Win32/SystemClock.cpp` as `.cpp`-level holes;
ten `*/Web/*.cpp` arms have never been compiled and `Os/Web/App.cpp` carries a POSIX
`readlink`/`execv` body that cannot build for wasm32; `AsyncTask.cppm:383` and `Parse.cppm:115` hold
handlers nothing instantiates, and `Parse.cppm`'s fix is `std::from_chars`, not an `#if`;
`--sweep-only` reports no probe verdicts, so the probe table is unmeasured since the
`gse.gpu_profile` move; `Nvidia/Backend.cpp` still has had no compiler over it; nothing is committed,
the owner's call.

### 2026-10-19 — steering: the freetype/slang arm judged (holds, 110 units — the largest arm of the port); the last rejection is a real channel, so the float parser goes to the owner

**Judged against the pass criteria: still nothing to pass.** (1)-(6) unstarted — nothing linked, no page,
no hash out of a browser.

**Confirmed from the artifacts, not the report.** `FontCompiler.cppm` is thirteen lines — module
declaration, `import std`, `import :font`, and the exported `bake(const std::filesystem::path&,
font::baked&) -> bool`; the 273-line FreeType/msdfgen body is in new
`Graphics/2D/Native/FontCompiler.cpp` (295 lines) with an 18-line `Graphics/2D/Web/FontCompiler.cpp`
stub beside it, and `Engine/cmake/GSEEngine.cmake` selects `Native` alongside `Win32`. The unused
`import gse.slang` is gone from `GpuRecord/PipelineBuilder.cppm`.
`/tmp/wasm_sweep_fontcompiler.log` (254KB, 20:02) reads **`host=43 ok=362 blocked=15 failed=1`**,
`modules: translates=37 host-bound=22 FAILED=1 blocked=3`. Against the `host=153 ok=268`,
`translates=35` baseline that is **110 units off the host frontier**, and `gse.freetype`,
`gse.msdfgen` and `gse.slang` all read `sole 0 reached 0` — nothing reaches them any more.
`gse.gpu_record` and `gse.physics` translate. Native green, `0xe836d31697879e27` unmoved. Review
found no defects. Verdict: correct, keep; by a wide margin the largest single arm so far.

**The `Native/` directory rather than `Win32/` is the right call and is endorsed**: the body is
portable C++ over two vendored libraries with no Windows in it, so `Win32/` would have misnamed the
axis and handed a future non-Windows native host the web stub.

**The deviation is exposure, for the third time by the same mechanism.** `failed=0` is not held: the
log's row is `gse.graphics FAILED 1/115 units Engine/Engine/Source/Graphics/2D/Gui/Settings.cppm`,
with `gse.devtools` (2/2), `gse.examples` (2/2) and `gse.runtime` (7/12) blocked `on
gse.graphics:settings`. As at `Task.cppm`, `ScopeExit.cppm` and `AssetFormat.cppm`, the rejection row
names the importer that triggered the instantiation, not the file holding the `catch`: the site is
`Meta/Parse.cppm:112-125`, the floating-point arm of `parser<T>::parse`, which was an uninstantiated
template body in every unit the sweep had offered until the FreeType edge cleared.

**Read at source, and this one does not take the `#if` that worked four times.** The handler is
`catch (...) { return false; }` around `std::stod`, and under `-fno-exceptions` libstdc++'s `stod`
aborts rather than being unreachable — so an `#if __cpp_exceptions` that deletes the `try` converts a
rejected settings value into a process abort. The integer arm immediately below it (`:119-127`)
already uses `std::from_chars`, so the repair is obvious in shape; what makes it the owner's is that
plain `from_chars` is **stricter than `stod`** — it rejects leading whitespace, a leading `+`, and
hex-float (`0x1p3`) spellings that `stod` accepts today. The callers are settings values
(`Meta/SettingsAnno.cppm`, `Ecs/Settings.cppm`), `--engine-setting` / command-line arguments
(`Meta/Args.cppm`), JSON reflection (`Json/Reflect.cppm`) and unit/vector parsing
(`Math/Units/Quantity.cppm`, `Math/Vector/VectorParse.cppm`) — so a value in the owner's
`settings.ini` written as `= 1.5` with a leading space is exactly the input whose acceptance would
change. Narrow, but native, and the charter reserves that.

**A third option exists that the task did not name, and it is my recommendation:** trim leading
whitespace and an optional `+`, then `from_chars`, retrying with `chars_format::hex` if the text
starts `0x`/`0X`. One spelling on both arms, no exception, and native acceptance preserved for every
input a settings file or an argument can realistically carry. ~8 lines, replacing 8.

**Put to the owner this run rather than carried**, because the three blocked modules include
`gse.runtime`, which is on `scene_reset_cpu`'s own path, and because a task scoped now would re-sweep
with `failed=1` standing and no clean judging line. The in-charter arm that remains either way is
`gse.win32` — `gse.process` (46 `win32::` uses) and `gse.sdk` (5), 5 modules behind
`{win32, win32.environment}` — and it is what the next task does once the parser answer lands, in the
same task if the answer is small.

**Carried forward unchanged:** eleven `*/Web/*.cpp` arms have never been compiled, and
`Os/Web/App.cpp` carries a POSIX `readlink`/`execv` body that cannot build for wasm32; the CMake drop
path still has no coverage — only its keep-both Windows arm has executed — and carries the two
one-assertion gaps the earlier review named; `Runtime/Engine.cpp`, `Fs/FileWatcher.cpp`,
`Os/Win32/Window.cpp`, `Time/Win32/SystemClock.cpp` and `Gpu/Device/VideoEncoder.cpp` are
`.cpp`-level holes the `-S` sweep cannot see; `AsyncTask.cppm:383` holds a handler nothing
instantiates; `--sweep-only` reports no probe verdicts, so the probe table is unmeasured since the
`gse.gpu_profile` move; `Nvidia/Backend.cpp` still has had no compiler over it; `gse.vulkan` still
names `gse.aftermath` via `Vulkan/Device.cppm`; nothing is committed, the owner's call.

### 2026-10-19 — the owner decided the float parser: trim + `from_chars`, with a hex retry

**Decision, not to be relitigated.** `Meta/Parse.cppm`'s floating-point arm becomes: skip leading
whitespace, consume an optional leading `+`, then `std::from_chars`; if that fails and the remaining
text starts `0x`/`0X`, retry with `std::chars_format::hex`. One spelling on both arms, no `try`, no
`#if`. Plain `from_chars` (option 2) and a two-arm `#if` (option 3) were both declined. The point of
the chosen shape is that native acceptance is preserved for every input a settings file or a
command-line argument can realistically carry, so the native change is nil in practice rather than
merely narrow — which is also what makes it checkable: the native hash cannot move, and that is a
proof rather than an argument.

Worth stating for whoever writes it: `from_chars`'s `general` format already accepts `inf`/`nan` and
the current code tests only `.ec`, so trailing junk is tolerated exactly as `stod` tolerates it
today and the integer arm immediately below tolerates it. The behaviour that genuinely cannot be
reproduced is a locale-dependent decimal separator, which `from_chars` deliberately does not honour
and which no settings file in this tree uses.

### 2026-10-19 — steering: the `gse.win32` arm judged (holds) — **every engine-authored module now translates**, so step 2 is done and the `-S` sweep has nothing left to measure

**Judged against the pass criteria: still nothing to pass.** (1)-(6) unstarted — nothing linked, no page,
no hash out of a browser. But the interface frontier is closed, and that is the end of step 2.

**Confirmed from the artifacts, not the report.** `grep -rl gse.win32` over `Engine/Engine/Source
--include=*.cppm` returns exactly three files: `External/Win32.cppm`, `External/Win32Environment.cppm`
(the roots themselves) and `Vulkan/Instance.cppm` (reserved by path). `/tmp/wasm_sweep_win32arm.log`
(268KB, 21:38) completed and reads **`host=34 ok=387`, `modules: host-bound=15 translates=48`**, with
`grep -cE 'FAILED|blocked'` over the whole file **0**. Against the `host=43 ok=362`, `translates=37`
baseline that is 9 more units and eleven more modules. Verdict: correct, keep.

**The module table is the headline, and it changes what the next step is.** All 15 remaining
host-bound rows are vendored roots or the two GPU backends: `gse.aftermath`, `gse.cxxabi`,
`gse.directx`, `gse.dx12 -> gse.directx`, `gse.freetype`, `gse.libjpeg`, `gse.libpng`,
`gse.miniaudio`, `gse.msdfgen`, `gse.slang`, `gse.sockets`, `gse.vulkan -> gse.aftermath`,
`gse.win32`, `gse.win32.environment`, `gse.winhttp`. **Not one engine-authored module is host-bound.**
`gse` (the umbrella the Sandbox imports), `gse.graphics` (115 units), `gse.physics` (25),
`gse.runtime` (12), `gse.scenario`, `gse.system_manifest`, `gse.ecs`, `gse.os` (13), `gse.process`,
`gse.sdk` — all `translates`. `gse.freetype`, `gse.msdfgen` and `gse.slang` now read `reached 0`;
the "ported together" groups are down to three rows of one module each. **There is no next arm to
price** — the `sole`/`reached` pricing that has driven the last ten entries has run out of input,
because a `-S` sweep cannot see a `.cpp` file and every remaining question is one.

**One thing was not measured and is being measured now rather than argued.** The sweep ran at 21:38;
`Meta/Parse.cppm` was last written at 21:53, after the review's two float-parser fixes. The report said
so honestly. The change is three lines inside an already-translating function body, but this ledger's
rule is that arguments are not numbers, so a `--sweep-only` was launched from this run
(`/tmp/wasm_sweep_parserfinal.log`) and this run waits on it. The parser itself was read at source and
is correct: whitespace trim, one optional sign, double sign rejected, `0x`/`0X` consumed *before*
`from_chars(..., chars_format::hex)` — which is required, since `from_chars` hex does not accept the
prefix — and the sign reapplied after. `inf`/`nan` still parse through `general`, matching `stod`.

**Next task: the first real wasm link, and the capability it needs.** `Tools/build_gcc_wasm.py` has
`--sweep` / `--sweep-only` and nothing that links: `compile_unit` emits `-S` assembly and there is no
`wasm-ld` invocation anywhere in it. So the task is to add a link arm that compiles the module closure
`scene_reset_cpu` actually needs — interfaces **and** the `.cpp` impl units the sweep has never
offered — and links it, reporting the undefined-symbol set as its deliverable. It is judged on facts
produced, not on a binary: a linked module is the win, and a *complete enumerated inventory* of
undefined symbols and unbuildable impl units is an equally acceptable outcome, because that inventory
is the thing ten entries of this ledger have been unable to see. Expect it to name
`Os/Win32/Window.cpp` (the only `:window_impl`), `Time/Win32/SystemClock.cpp`, `Runtime/Engine.cpp`,
`Fs/FileWatcher.cpp`, `Gpu/Device/VideoEncoder.cpp`, `Os/Web/App.cpp`'s POSIX `readlink`/`execv` body,
and the fifteen `*/Web/*.cpp` arms that have never had a compiler over them. Reuse `harness_text()` /
`harness.mjs`, already written unconditionally. No engine behaviour change in the task beyond what a
compile error forces, and anything that would delete or disable a native path is escalated, not done.

**Anticipated and left for the owner when the link measures it, not pre-emptively:** `Gpu/Device/
Device.cpp` imports both backends, so a wasm link will want the `Vulkan/` and `Dx12/` `.cpp` trees
excluded the way their interface partitions already are. The owner's grant covered interface
partitions; whole-directory `.cpp` exclusion is adjacent but larger, and it should be put to them with
the link's actual symbol list in hand rather than as a prediction.

**Carried forward unchanged:** `gse.vulkan -> gse.aftermath` (`Vulkan/Device.cppm`) is the one GPU root
the backend-interface selection did not cover, reserved by path; `Process/Web/Process.cpp` and
`Sdk/Web/Pack.cpp` are honest unsupported reports (child processes with job objects, LZMS, CNG have no
browser answer) — correct for this goal and recorded so nobody reads them as implementations; the CMake
drop path still has no coverage beyond its keep-both Windows arm; `AsyncTask.cppm:383` holds a handler
nothing instantiates; `--sweep-only` reports no probe verdicts, so the probe table is unmeasured since
the `gse.gpu_profile` move; `Nvidia/Backend.cpp` still has had no compiler over it; nothing is
committed, the owner's call.

### 2026-10-19 — steering: the parser change is measured `ok`; and the last entry was wrong — the link pipeline already exists, so the next task is far smaller than it was scoped

**Judged against the pass criteria: still nothing to pass.** (1)-(6) unstarted — nothing linked, no page,
no hash out of a browser.

**The one thing this sweep was launched to measure is measured.**
`/tmp/wasm_sweep_parserfinal.log` reads `[  7/421] ok Engine/Engine/Source/Meta/Parse.cppm`, and
`grep -cE 'FAILED|blocked'` over the log is **0**. So the owner's float-parser decision — trim, one
optional sign, `0x` consumed before `from_chars(..., chars_format::hex)` — compiles for wasm32, which
is what was outstanding. Read at source and it is the decided shape exactly: whitespace trim via
`find_first_not_of`, a single sign with a double sign rejected, prefix stripped before the hex retry,
`general` otherwise. `git diff` is +22/−4 in that one file.

**The sweep itself died again — seventh time — at unit 73/421, 22:08, no live processes**, so the
`host=34 ok=387` recount is *not* confirmed. It is also not worth an eighth attempt: the parser change
adds no import, so no host-bound row can move, and the count it would reprint is not an input to the
next task. Waiting on a job with no use for its result is the thing this phase is told not to do.

**But the survival pattern finally has a signal, and it reverses an earlier entry.** Every
`--sweep-only` that *completed* (`/tmp/wasm_sweep_only.log`, `_os`, `_assetformat`,
`_backend_interfaces`, `_fontcompiler`, `_win32arm`) was launched from a **task** session that then
waited on it. Both that have died were launched from a **steering** run that ended its turn to
`gse_await`. So it is not `nohup` versus the harness, as the sixth-attempt entry concluded: a steering
run's turn-end takes the process tree with it either way. **Rule for later runs: do not launch a sweep
from steering. Scope it into the task that needs it, and have the task wait.**

**A correction to the previous entry, and it shrinks the next task a long way.** That entry asserted
"`Tools/build_gcc_wasm.py` has `--sweep` / `--sweep-only` and nothing that links: `compile_unit` emits
`-S` assembly and there is no `wasm-ld` invocation anywhere in it." **That is false.** `probe_status`
(`:1554-1690`) already runs the full pipeline: `llvm-mc -filetype=obj` per unit, then a `wasm-ld` line
assembled in deliberate dependency order at `:1668` (`crt1-reactor.o`, objects, `-lstdc++`, `-lgcc`,
`-lc -lwasi-emulated-signal`, and `--shared-memory --import-memory --export-memory --max-memory=` for
a threads probe), then `node --no-warnings harness.mjs`. `-S` is where the *sweep* stops, not where
the script stops. Probes link and run today.

**And the closure machinery exists too.** `Probe.closure_of` (`:414`, used at `:1570`) feeds
`graph().closure(module)`, and three probes already link a whole module's closure that way
(`gse.assert`, `gse.meta`, `gse.math`). So a first engine link is a new `Probe` over
`scene_reset_cpu`'s module, not a new link arm.

**The real gap, named precisely:** `module_graph.py:239`'s `closure` returns **interface units only**
(`self.paths(self.units_of(module))`), and `build_gcc_wasm.py` offers only `.cppm`. Nothing in either
enumerator yields a `.cpp`, which is exactly why ten entries of this ledger could not see the
`.cpp`-level holes. An engine link needs the impl units, so the task's one piece of new machinery is
an impl-unit enumeration — and because this tree has twice been bitten by two enumerators disagreeing,
it should come from the same place CMake's selection does, not a second directory test.

**Next task: link `scene_reset_cpu`'s closure, and report the undefined-symbol inventory.** Add a
`.cpp` enumeration beside `closure` (impl units for the modules in the closure, with the platform/
backend selection applied — reuse `EXCLUDED_INTERFACES_LIST`'s one-declaration discipline and
`gse_select_platform_impls()`'s axis rather than restating either), and a probe or arm that takes that
unit set through the existing `llvm-mc` → `wasm-ld` → `harness.mjs` path with `hosted=True`,
`cxx_runtime=True`, `std_module=True`, `threads=True`. **Judged on facts produced, not on a binary:** a
linked module is the win, and a complete enumerated inventory of undefined symbols and unbuildable
impl units is an equally acceptable deliverable, because that inventory is what this ledger has been
blind to throughout. Expect it to name `Os/Win32/Window.cpp` (the only `:window_impl`),
`Time/Win32/SystemClock.cpp`, `Runtime/Engine.cpp`, `Fs/FileWatcher.cpp`,
`Gpu/Device/VideoEncoder.cpp`, `Os/Web/App.cpp`'s POSIX `readlink`/`execv` body, and the fifteen
`*/Web/*.cpp` arms that have never had a compiler over them. **Run it in the task and report its
output** — and if the task also wants a sweep recount, run that in the task too, per the rule above.
No engine behaviour change beyond what a compile error forces; anything that would delete or disable a
native path is escalated, not done.

**Anticipated, left for the owner when the link measures it:** `Gpu/Device/Device.cpp` imports both
backends, so a wasm link will want the `Vulkan/` and `Dx12/` `.cpp` trees excluded the way their
interface partitions already are. The owner's grant covered interface partitions; whole-directory
`.cpp` exclusion is adjacent and larger, and should be put to them with a symbol list in hand.

**Carried forward unchanged:** the `host=34 ok=387` module table stands from the 21:38 sweep and has
not been re-measured since the parser fix (no row can have moved); `gse.vulkan -> gse.aftermath`
(`Vulkan/Device.cppm`) is the one GPU root the backend-interface selection did not cover, reserved by
path; `Process/Web/Process.cpp` and `Sdk/Web/Pack.cpp` are honest unsupported reports, not
implementations; the CMake drop path still has no coverage beyond its keep-both Windows arm;
`AsyncTask.cppm:383` holds a handler nothing instantiates; `--sweep-only` reports no probe verdicts, so
the probe table is unmeasured since the `gse.gpu_profile` move; `Nvidia/Backend.cpp` still has had no
compiler over it; nothing is committed, the owner's call.

### 2026-10-20 — steering: the first engine link judged (holds, and the inventory is real); the frontier is now a named list of 25 files, and the one reserved piece is measured down to ~80 lines

**Judged against the pass criteria: still nothing to pass.** (1)-(6) unstarted — nothing ran in a
browser, and the task said so plainly: the subject was the *engine*, and `scene_reset_cpu` lives in
`sandbox:scenarios`, outside both enumerators' roots. That is a correct statement of scope, not a
shortfall.

**Confirmed from the artifacts, not the report.** The three data files exist with the fatal-on-missing
discipline (`Engine/cmake/platform_impl_arms.txt` → `Win32`/`Native`/`Web`,
`gpu_profile_backend_arms.txt` → `Nvidia`/`Null`, `excluded_impl_units.txt` → `MiniaudioImpl.cpp`),
each documents that the *arm name set* is the datum shared and the *policy* deliberately is not, and
all three are pinned `text eol=lf` in `.gitattributes` (lines 21-23) beside
`native_gpu_backend_interfaces.txt` — the CRLF scar this goal already carries. No engine C++ in
`touched.txt`, so the unmoved `0xe836d31697879e27` is consistent by construction.
`/tmp/link_engine3.log` is the third and only trustworthy run and carries the summary
**`compiled 458 · rejected 25 · assembled 457 · not assembled 1 · undefined 31`**, the 25 rejected
paths and the 31 symbols verbatim. Verdict: correct, keep. The two deviations it reports
(`link_closure` replacing `closure`, because `.cpp` files contribute no import edges and nine
partitions were linking against stale module-cache BMIs; and `--error-limit=0`, after the
single-dash spelling was eaten by `-e`) are both self-caught measurement errors reported rather than
absorbed, which is the standard this ledger has been asking for since the sixth sweep death.

**The pricing regime this ledger has run on for twelve entries is over.** `sole`/`reached` off a
`-S` sweep has no input left — every engine-authored module translates — and the frontier is now a
**named list of 25 files**, which is exactly what eleven entries could not see. Four classes:
17 `.cpp` reaching a host/vendor wrapper with no wasm arm; 5 exception sites a real link
instantiated; `Gpu/Device/Device.cpp`; and `Os/Web/App.cpp`'s POSIX body.

**The in-charter block is large, coherent and is step 3's actual substance.** Ten of the seventeen
are `gse.win32` through spine modules the charter names by module and by path:
`Import/{Assert,Log,Config,Shell,Stacktrace}.cpp`, `Concurrency/Task.cpp`,
`Diag/{AllocTrack,ProfileAggregator}.cpp`, `Fs/FileWatcher.cpp`, `Runtime/Engine.cpp` — and the
undefined-symbol set is their shadow, almost line for line (`gse.log` 5, `gse.assert` 1,
`gse.config` 1, `gse.shell` 1, `gse.concurrency` 1, `gse.os`'s `image::load` 1). So the Web arms and
the missing symbols are one piece of work, not two.

**One class is reserved, and it is now measured rather than predicted — which was the stated
precondition for asking.** `Gpu/Device/Device.cpp` fails on `import :device_dx12_backend`, a partition
the owner's grant already excludes from the wasm configuration; four `gpu::device` methods
(`create_buffer`, two `retire`s, `free_aliased_memory`) are undefined precisely because that one
object is absent. Read at source, the file is **920 lines of which only ~80 are backend-dependent**:
the two imports at `:18-19`, the reflection-built `gpu_dispatch` at `:35-39`, and
`create_device`'s vulkan-then-dx12 arms at `:52-96`. Everything after `:113` forwards through the
dispatch pointer and names no backend. So the previous two entries' framing — "exclude the whole
`Vulkan/`/`Dx12/` `.cpp` trees" — is not the question the link actually asks: those trees are three
files total and were never in the closure. The real question is whether `create_device` becomes a
per-configuration arm the way the backend *interfaces* already are. Put to the owner this run with
that measurement in hand.

**Carried forward, with two corrections to this run's own reading of the report.** The `bit_cast`
finding is real but under-counted: `std::bit_cast<const void*>(handle)` appears at four sites in
`Gpu/Graph/RenderGraph.cpp` (`:630,1024,1049,1699`) and **nine** in
`GpuRecord/RecordingContext.cpp`, not two and two — a 64-bit handle to a 32-bit pointer, ill-formed
on wasm32 and invisible natively. And the two halves sit on opposite sides of the charter:
`GpuRecord/` is not a reserved path, `Gpu/` is. Also outstanding and unchanged: the five exception
sites (`AsyncTask.cpp:320` is the one that is a real `exception_ptr` channel and the only one of the
five this ledger has pre-judged as needing more than an `#if`); `_ZTHN3gse5traceW3gseW4diag3tlsE`
emitted without a `.functype`, which stops `Diag/Trace.cpp` assembling and is the direct cause of
all eight `gse::trace::*` undefined symbols — a patch-0018 candidate, in charter; `__muloti4` absent
from this libgcc, reached from `libc.a(clock_nanosleep.c.obj)`, unexamined;
`Diag/AllocTrackGlobal.cpp` and `Dx12/AgilitySdk.cpp` declare no module so no link enumerates them,
and the first replaces global `operator new`; `--max-memory` still the harness's 16 pages; nothing
committed, including the three new data files.

### 2026-10-20 — the owner decided `Device.cpp`: split it, the seam shape one level down. And the split has a hard part the question did not name

**Decision, not to be relitigated.** The ~80 backend-dependent lines of `Gpu/Device/Device.cpp` become
a per-configuration `create_device` arm — native keeps today's vulkan-then-dx12 body verbatim, wasm
gets a null/headless arm — and the other ~840 backend-neutral lines stay where they are. Whole-file
exclusion and further deferral were both declined. Native must come out textually unchanged on its
arm, which is also the check: `0xe836d31697879e27` cannot move.

**Read further while scoping, and the hard part is not `create_device` — it is `gpu_dispatch`.**
`Device.cpp:34-36` completes the dispatch type with
`consteval { std::meta::define_aggregate(^^gpu_dispatch, meta::dispatch_specs(... ^^vulkan_device_backend ...)) }`,
so the *shape* of the table every one of the 840 neutral lines dereferences through `m_vt->` is
derived from `vulkan_device_backend` — a partition the wasm configuration excludes. A
`define_aggregate` happens once in a program, so the neutral half cannot simply keep it: whichever
arm is compiled has to complete that type, and the wasm arm has no Vulkan backend to derive it from.
**So the null arm is not a stub that returns failure; it needs a null backend type with the same
operation set to define the aggregate from** — the shape the engine already has a slot for, since
`Engine/cmake/native_gpu_backend_interfaces.txt` is precisely the list of backend interface
partitions a non-native configuration drops, and a `:device_null_backend` beside it is the same
mechanism used in the other direction.

**Second decision point inside the owner's pick, for the task to settle and report rather than
assume:** `backend_kind` (`GpuBackend/Enums.cppm:15`) has exactly two enumerators, `vulkan` and
`dx12`, and `device::create` takes it by reference and writes the one it chose. A null arm has no
honest value to write. Adding a third enumerator is compile-visible everywhere it is switched on and
`GpuBackend/` is not a reserved path, so it is in charter — but it is a change to a native-facing
enum and belongs in the task's report, not in its margins.

**Next task: the split, plus the in-charter half of the inventory, one link.** Two pieces, both
judged by the same `--link-engine` run:
1. **`Device.cpp`'s split**, as decided: a `create_device` arm per configuration, a
   `:device_null_backend` partition for the wasm arm to complete `gpu_dispatch` from, selected
   through the existing one-declaration arm lists rather than a second directory test. Native
   textually unchanged.
2. **The ten `gse.win32` spine impl units**, which are the same work as the 31 undefined symbols:
   `Import/{Assert,Log,Config,Shell,Stacktrace}.cpp`, `Concurrency/Task.cpp`,
   `Diag/{AllocTrack,ProfileAggregator}.cpp`, `Fs/FileWatcher.cpp`, `Runtime/Engine.cpp` — Web arms
   under the established `*/Web/` discipline, each one a decision between a real browser answer and
   an honest unsupported report, decided per unit and reported per unit, never swept.

Judged on: `rejected` falling from 25 toward the classes left unaddressed (the five exception sites,
the `bit_cast` sites, `Os/Web/App.cpp`), the `gse.log`/`gse.assert`/`gse.config`/`gse.shell`/
`gse.concurrency`/`gse.os` undefined symbols going to zero, and the four `gpu::device` methods
defined. Native build green and `0xe836d31697879e27` unmoved. **Run `--link-engine` in the task and
report its summary** — the one rule this ledger has paid for repeatedly.

**Deliberately not in it:** the five exception sites and the `bit_cast` sites (per-site decisions,
and `Gpu/Graph/RenderGraph.cpp`'s four are a reserved path); the `.functype` patch 0018; `__muloti4`;
the Sandbox link, which is the subject after the engine's own gaps close and is where criterion (1)
first becomes reachable.

### 2026-10-20 — steering: the `Device.cpp` split and the spine seam judged (both hold); the 16 rejections now sort into exactly four classes, and three of them are small

**Judged against the pass criteria: still nothing to pass.** (1)-(6) unstarted — nothing ran in a
browser. `scene_reset_cpu` lives in `sandbox:scenarios`, outside both enumerators' roots, so the
engine link cannot reach it yet; the task said so, and that is scope, not shortfall.

**Confirmed from the artifacts, not the report.** `Gpu/Device/Device.cpp` imports no backend —
its import list is `:backend_state`, `:command_dispatch`, `:device`, `:device_contract` and neutral
modules — and `Gpu/Device/{Native,Web}/{DeviceCreate,DeviceVideo}.cpp` both exist, so
`make_video_encoder` was armed alongside `create` as reported. `native_gpu_backend_units.txt` is the
renamed single declaration and now carries `VideoEncoder.cpp` beside the three partitions, with the
comment stating why the `.cpp` walk and the `.cppm` walk read the same file. `/tmp/link_engine5.log`
(09:41) is the run and it is what was claimed: **`offered 491 · compiled 475 · rejected 16 ·
assembled 474 · not assembled 1 · undefined 24`** against the previous `458/25/31`. The five named
modules are at zero in the symbol list — no `gse::log`, `gse::assert`, `gse::config`,
`gse::shell::reveal` (present in run 4, gone in run 5) or `task::apply_priority`. Verdict: correct,
keep. All three reported deviations check out, including the reverted `import gse.win32` deletions —
`using namespace` use is invisible to a grep, and that is now the second time this port has been
misled by one.

**The useful product of this run: all 16 rejections classified by their actual first diagnostic, so
the next scoping is not a guess.** Four classes, no overlap:

| class | files | first error |
|---|---|---|
| **exceptions, 5 sites** | `Concurrency/AsyncTask.cpp:320`; `Ecs/Scheduler.cpp:318`; `Assets/AssetCatalog.cppm:126` (via `Gui/Gui.cpp`, `Gui/Scale.cpp`); `Runtime/Bootstrap.cpp:45`; `Assets/AssetSystem.cppm:176` (via `Runtime/Engine.cpp`) | `exception handling disabled` |
| **`bit_cast<const void*>(handle<T>)`** | `Gpu/Device/Device.cpp:736`, `Gpu/Graph/RenderGraph.cpp:630`, `GpuRecord/RecordingContext.cpp:211` | `no matching function` |
| **vendored root, no wasm module** | `gse.libpng` (`Os/STB/ImageLoader.cpp`), `gse.slang` (`GpuRecord/PipelineBuilder.cpp`), `gse.sockets` (`Network/Socket.cpp`), `gse.miniaudio` (`Audio/Audio.cpp`), `gse.winhttp` (`Http/Client.cpp`) | `failed to read compiled module` |
| **one-offs** | `Import/Stacktrace.cpp:219` (`std::stacktrace` absent from the sysroot), `Os/Web/App.cpp:18` (`execv`) | — |

**And the 24 undefined symbols are entirely the shadow of that table plus one assembler bug** — 8
`gse::trace::*` from the single unit that does not assemble (the `.functype`-less
`_ZTH...diag3tlsE`, patch-0018 candidate), 7 `network::udp_socket` from `Socket.cpp`, 4 `gpu::device`
from `Device.cpp`, 2 `gse::*stacktrace*`, `image::load` from `ImageLoader.cpp`, and `__muloti4` from
libgcc. There is no symbol in the list that is not explained by a rejected or unassembled unit. So
closing rejections is the whole of the remaining engine work; there is no separate linker frontier.

**The five exception sites were read at source, and they are not one decision.** Three are the shape
this ledger has converted four times — `Ecs/Scheduler.cpp`'s two handlers only `assert(false, ...)`,
`Runtime/Bootstrap.cpp`'s is a `std::terminate` handler rethrowing to print a type name, and
`AsyncTask.cpp`'s `when_all_helper` stores into an `exception_ptr` nothing can fill on the
`-fno-exceptions` arm — all unreachable by construction there, so `#if __cpp_exceptions` is honest
and native is textually unchanged. **The other two are a new class and are the `Parse.cppm` lesson
again:** both catch `std::filesystem::filesystem_error` around *throwing* `std::filesystem`
overloads (`exists`, `last_write_time`, directory walks), which under `-fno-exceptions` **abort**
rather than being unreachable. An `#if` there converts a missing-file I/O error into a process abort.
The fix is the `std::error_code` overloads, returning the same `asset_error{io_failure}` the handler
already returns; the one native-observable difference is the `detail` wording, since `error.what()`
is replaced by the `error_code`'s message. That is squarely the charter's "a decision between an
error channel the caller already has and a genuinely fatal condition", so it is the task's to make
and to report — but it must be reported, not absorbed.

**Next task: the five exception sites, per-site, plus the one in-charter `bit_cast` file.** Convert
`Ecs/Scheduler.cpp:318`, `Runtime/Bootstrap.cpp:45` and `AsyncTask.cpp:320` with
`#if __cpp_exceptions`; rewrite `Assets/AssetCatalog.cppm:126` and `Assets/AssetSystem.cppm:176` onto
the `error_code` overloads with the same `io_failure` channel on both arms, reporting the `detail`
wording change — and escalating rather than guessing if either body turns out to need a native
behaviour change beyond that. Then fix `GpuRecord/RecordingContext.cpp`'s `bit_cast<const void*>` of
a 64-bit handle (`GpuRecord/` is not a reserved path; the two in `Gpu/` are, and are left alone).
Judged on: `rejected` falling from 16 to the classes left unaddressed — the two `Gpu/` `bit_cast`
files, the five vendored roots, `Stacktrace.cpp`, `Os/Web/App.cpp` — with the `network`-unrelated
exception units (`Gui.cpp`, `Scale.cpp`, `Runtime/Engine.cpp`, `Runtime/Bootstrap.cpp`,
`Ecs/Scheduler.cpp`, `AsyncTask.cpp`) all compiling; native build green and `0xe836d31697879e27`
unmoved. **Run `--link-engine` in the task and report its summary.**

**To the owner next run, now that the class is measured rather than predicted:** the two `Gpu/`
`bit_cast` sites (`Device.cpp:736`, `RenderGraph.cpp:630` and three siblings) are a reserved path and
are all that stand between the split and the four undefined `gpu::device` methods; and the five
vendored roots need a per-root verdict (a wasm arm, or an honest unsupported report) that
`gse.libpng` at least cannot avoid, since `image::load` is reached from the neutral `gse.os`.

**Carried forward unchanged:** `video_encoder`'s ctor/dtor/moves have no wasm definition and the
first unit that destroys an `std::optional<video_encoder>` will surface it; `"web"` is hardcoded as
the process-id answer in three modules with no shared home; an untracked `Runtime/Web/Platform.cpp`
was overwritten and whatever else it held is gone; `Diag/AllocTrackGlobal.cpp` and
`Dx12/AgilitySdk.cpp` declare no module so no link enumerates them; `__muloti4` is absent from this
libgcc, reached from `libc.a(clock_nanosleep.c.obj)`, still unexamined; the editor returned "failed
without a parseable diagnostic" for roughly eight ordinary GCC errors in this task, which is a
tooling gap worth its own report; nothing is committed, the owner's call.

### 2026-10-20 — steering: the thirteen exception sites judged (hold); rejections 16 → 10, and the only thing left on the `gpu::device` symbols is a reserved-path type change

**Judged against the pass criteria: still nothing to pass.** (1)-(6) unstarted — nothing ran in a
browser. `scene_reset_cpu` lives in `sandbox:scenarios`, outside both enumerators' roots, so the
engine link still cannot reach it.

**Confirmed from the artifacts, not the report.** `/tmp/link_engine8.log` (10:58) reads
`rejected 10 · assembled 480 · not assembled 1` against the previous `16/474/1`, and **no rejection
in it is an exception site** — the ten are `Import/Stacktrace.cpp`, `Os/Web/App.cpp`, the five
vendored roots (`Os/STB/ImageLoader.cpp`, `GpuRecord/PipelineBuilder.cpp`, `Network/Socket.cpp`,
`Audio/Audio.cpp`, `Http/Client.cpp`) and the three `bit_cast` files
(`Gpu/Device/Device.cpp`, `Gpu/Graph/RenderGraph.cpp`, `GpuRecord/RecordingContext.cpp`). The
eleven `#if __cpp_exceptions` guards were read at source in `Ecs/Scheduler.cpp` (the two
`assert(false, ...)` handlers), `Runtime/Bootstrap.cpp` and `Concurrency/AsyncTask.cpp` (both
`when_all_helper` and `sync_wait`, the `#else` arm a bare `co_await`) — native textually unchanged,
which the unmoved `0xe836d31697879e27` checks. `AssetCatalog.cppm`'s `catalog_for` is the real
rewrite and is correct in shape: a local `io_failure` lambda, `exists(p, ec)`,
`recursive_directory_iterator(root, directory_options::none, ec)`, `it.increment(ec)` and
`is_regular_file(ec)` each checked, same `asset_error{io_failure}` through the same `std::expected`
on both arms. Verdict: correct, keep. The eight unlisted sites being done in-task rather than
deferred is the right call and is the lesson four earlier entries paid for.

**The one native-observable difference is named and accepted:** `asset_error::detail` is now the
`error_code`'s message rather than `filesystem_error::what()`, and `catalog_for`'s
`asset_error::path` is the path in hand at the failing call. Codes and callers unchanged; this is
exactly the charter's "an error channel the caller already has", and it was reported rather than
absorbed.

**The `bit_cast` half was deliberately not done and is measured rather than predicted.**
`resource_ref::ptr` (`GpuBackend/Pipeline.cppm:35`) is a `const void*` **identity key** holding a
64-bit `gpu::handle` — 17 sites, **8 in `Gpu/`** (`Device.cpp:736,747,785,797`;
`RenderGraph.cpp:630,1024,1049,1699`) and **9 in `GpuRecord/RecordingContext.cpp`**. Grepped every
consumer: it is only ever compared or used as a map key (`RenderGraph.cpp:646`'s
`target_of.find(u.resource.ptr)`), never dereferenced. So changing `ptr` to a `std::uint64_t` id is
bit-for-bit the same value natively and ill-formed nowhere — but it rewrites eight call sites under
`Gpu/`, reserved to the owner by path. The `GpuRecord/`-only variant would truncate a 64-bit handle
to 32 bits on wasm32 and close nothing, so it is not a cheaper subset; it is wrong. This is all that
stands between the finished `Device.cpp` split and the four undefined `gpu::device` methods
(`create_buffer`, both `retire`s, `free_aliased_memory`).

**Escalating, with that measurement in hand.** Recommended: `resource_ref::ptr` becomes
`std::uint64_t id` and the 17 `std::bit_cast<const void*>(x.handle())` become
`std::bit_cast<std::uint64_t>(...)` (or the handle's own value), `GpuBackend/Pipeline.cppm`'s
declaration with them. `host_buffer` is a genuine host pointer and is **not** touched.

**In charter and next either way, whichever the answer is:** `Os/Web/App.cpp`'s POSIX
`readlink`/`execv` body and `Os/STB/ImageLoader.cpp` (both `Os/**`, and the latter is the sole
source of the undefined `image::load` that the *neutral* `gse.os` reaches), plus
`Import/Stacktrace.cpp` (`<stacktrace>` absent from the sysroot; two undefined `gse.stacktrace`
symbols). The other four vendored roots — slang, sockets, miniaudio, winhttp — each need a per-root
verdict and none is named by the charter.

**Measurement lesson, from the task:** three `--link-engine` runs were killed before finishing;
only a *foreground* run promoted to background after its timeout survived. A fresh background launch
does not. Combined with the earlier sweep rule: run the measurement in the task, in the foreground.

**Carried forward unchanged:** the 24 undefined symbols are the same set — `__muloti4` from libgcc,
eight `gse::trace::*` from the one unit that does not assemble (missing `.functype`, a patch-0018
candidate), seven `network::udp_socket`, four `gpu::device`, two stacktrace, `image::load`;
`video_encoder`'s ctor/dtor/moves have no wasm definition; `"web"` is hardcoded as the process-id
answer in three modules; `Diag/AllocTrackGlobal.cpp` and `Dx12/AgilitySdk.cpp` declare no module so
no link enumerates them; the CMake drop path has no coverage beyond its keep-both Windows arm;
`gse.vulkan -> gse.aftermath` (`Vulkan/Device.cppm`) is reserved by path; nothing is committed —
`.gitattributes`, both documents and the `*/Web/` arms — the owner's call.

### 2026-10-20 — the owner approved the `uint64_t` id, and asked why not just delete every try/catch. Measured answer: the wasm arm already has none

**Decision, not to be relitigated.** `resource_ref::ptr` (`GpuBackend/Pipeline.cppm:35`) becomes a
`std::uint64_t` id and the 17 `std::bit_cast<const void*>(x.handle())` sites become
`std::bit_cast<std::uint64_t>(...)`, including the 8 under the reserved `Gpu/` path. Natively the
value is bit-for-bit what it is today, so `0xe836d31697879e27` cannot move and that is the check.
`host_buffer` is a genuine host pointer and is **not** touched.

**On "remove all try/catch and propagate errors instead" — that is already the wasm arm, and the
guards are how it was achieved without touching native.** Measured across
`Engine/Engine/{Source,Import}`: **26 `catch` sites in 9 files, and every one of those files carries
an `__cpp_exceptions` guard** (`AssetRegistry.cppm` 10, `AssetFormat.cppm` 4, `AsyncTask.cpp` 2,
`Task.cppm` 2, `AssetSystem.cppm`, `ScopeExit.cppm`, `Scheduler.cpp`, `Bootstrap.cpp`). So on the
`-fno-exceptions` arm the engine compiles with no handler at all; the native arm keeps today's code
textually. Deleting the sites outright instead would change native behaviour — which the charter
reserves — and would buy the wasm build nothing it does not already have.

**And the reason deletion would not be enough even if native were free: the throws are not ours.**
The engine contains exactly **five** `throw` statements — four `std::bad_alloc` in
`Diag/AllocTrackGlobal.cpp` and the guarded `format_error` at `Quantity.cppm:888`. Everything the
handlers ever caught came out of libstdc++: `std::filesystem`'s throwing overloads, `std::stod`,
`operator new`, `std::format`. Under `-fno-exceptions` libstdc++ does not propagate those — it
**aborts** (`_GLIBCXX_THROW_OR_ABORT`). So removing a handler converts a recoverable failure into a
process abort rather than into an error return. That is precisely why two sites this goal touched
could not take a guard and had to be rewritten: `Meta/Parse.cppm`'s `stod` → `from_chars`, and
`Assets/`'s `filesystem_error` catches → the `error_code` overloads. Propagating errors is the right
model and the engine already does it with `std::expected`; the work is per-site plumbing onto
non-throwing APIs, not a deletion sweep.

**Next task:** the `uint64_t id` change as decided, plus the three in-charter rejections the last
entry named — `Os/Web/App.cpp` (POSIX `readlink`/`execv`), `Os/STB/ImageLoader.cpp` (sole source of
the undefined `image::load`, reached from the neutral `gse.os`) and `Import/Stacktrace.cpp`
(`<stacktrace>` absent from the sysroot). Judged on: `rejected` falling from 10 to the four vendored
roots that remain unverdicted (slang, sockets, miniaudio, winhttp); the four `gpu::device` symbols,
`image::load` and the two `gse.stacktrace` symbols all defined; native build green and
`0xe836d31697879e27` unmoved. Run `--link-engine` **in the task, in the foreground** — three
background runs died, and only a foreground run promoted after its timeout survived.

### 2026-10-20 — steering: the `uint64_t` id and the three in-charter rejections judged (hold); the engine's own frontier is now **four vendored roots, one unassembled unit and one libgcc symbol**, and nothing else

**Judged against the pass criteria: still nothing to pass.** (1)-(6) unstarted — nothing ran in a
browser. `scene_reset_cpu` lives in `sandbox:scenarios`, outside both enumerators' roots, so the
engine link cannot reach it; that is scope, not shortfall.

**Confirmed from the artifacts, not the report.** `GpuBackend/Pipeline.cppm:35` reads
`std::uint64_t id = 0;` with `host_buffer` still a `const void*` two lines below, and
`grep -rn 'bit_cast<const void\*>' Engine/Engine/Source/` returns **nothing** — all 17 sites moved,
including the eight under the reserved `Gpu/` path the owner authorised. `Os/STB/` is gone;
`Os/ImageLoader.cppm`, `Os/ImageLoader.cpp`, `Os/Native/ImageLoader.cpp` and
`Os/Web/ImageLoader.cpp` are the three-way split as described, and `Import/{Win32,Web}/Stacktrace.cpp`
now sit beside their four `Assert`/`Config`/`Log`/`Shell` siblings in both arms.
`/tmp/link_engine9.log` (11:35) carries the summary **`offered 493 · compiled 489 · rejected 4 ·
assembled 488 · not assembled 1 · undefined 17 · other errors 0`** against the previous
`491/475/16/474/1/24`, and its rejection list is exactly the four vendored roots —
`Http/Client.cpp`, `GpuRecord/PipelineBuilder.cpp`, `Network/Socket.cpp`, `Audio/Audio.cpp`. The
full judging line holds: no `gpu::device`, `image::load` or `gse.stacktrace` symbol appears in the
undefined list, which is now 8 `gse::trace::*`, 8 `gse::network::*` and `__muloti4` and nothing
else. Review found no defects. Verdict: correct, keep.

**The remaining engine frontier is three items and is fully enumerated for the first time.** There
is no longer a class of unknown breakage: every undefined symbol is the shadow of a named unit.
1. **Four vendored roots with no wasm module**, each rejected on `failed to read compiled module`:
   `gse.winhttp` (`Http/Client.cpp`, 396 lines), `gse.slang`
   (`GpuRecord/PipelineBuilder.cpp`, 1016), `gse.sockets` (`Network/Socket.cpp`, 281),
   `gse.miniaudio` (`Audio/Audio.cpp`, 227). The 8 `gse::network::*` symbols are `Socket.cpp`'s
   shadow; the other three are reached but their symbols are not yet demanded.
2. **`Diag/Trace.cpp` does not assemble** — GCC emits `_ZTH…diag3tlsE` with no `.functype` — and is
   the sole cause of the 8 `gse::trace::*` symbols. A patch-0018 candidate, in charter.
3. **`__muloti4` absent from this libgcc**, reached from `libc.a(clock_nanosleep.c.obj)`, still
   unexamined.

**On charter, stated so the next task does not stall on it.** `Network/`, `Audio/`, `Http/` and
`GpuRecord/` are not reserved paths, and arming a `.cpp` with a `Web/` sibling changes no native
behaviour — it is the settled seam shape this port has now applied eight times (`Fs/`, `Assets/`,
`Runtime/`, `Os/`, `Process/`, `Sdk/`, `Graphics/2D/`, `Import/`). The four `External/*.cppm` wrapper
interfaces themselves are **not** touched by that work and stay as they are. So this is in charter
and is an ordinary task, not an escalation.

**Two shape warnings for it, read at source rather than assumed.** `Audio/Audio.cpp` declares
`module gse.audio;` — the *primary* module's impl unit, not a partition, so arming it is not the
same two-file move the others are and may want a partition extracted first.
And `GpuRecord/PipelineBuilder.cpp` is 1016 lines: arming the whole file means a Web arm that
redefines its entire exported surface, so the cheaper and more honest move is to hoist only the
`slang::`-using bodies into a sub-impl partition and arm *that*, leaving the neutral majority
compiled by every configuration. A per-root verdict is the deliverable; a root that turns out to
need more than an unsupported report should be reported, not forced.

**Next task: the four vendored roots, one `--link-engine`.** A Web arm per root reporting unsupported
through the error channel each caller already has — `std::expected`, a `false` return, an
`assert(false, …)` where the native path itself asserts — never a silent success, and never a
deletion of the native body. Judged on: `rejected` falling from 4 to 0, the 8 `gse::network::*`
symbols defined or gone, `undefined` down to the 8 `gse::trace::*` plus `__muloti4`; native build
green and `0xe836d31697879e27` unmoved. Run `--link-engine` **in the task, in the foreground** — the
only runs that have ever survived.

**Then, in order:** the `.functype` patch 0018 for `Diag/Trace.cpp` (a toolchain rebuild, expected
hours, in charter); `__muloti4`; and only then the **Sandbox** link, which is where criterion (1)
first becomes reachable and where the eleven never-executed `*/Web/*.cpp` arms are first observed
rather than merely compiled.

**Carried forward unchanged:** `Os/Web/ImageLoader.cpp` asserts rather than failing at link time if
the headless scenario turns out to call `image::load` — unsettleable before the Sandbox link;
`Diag/AllocTrackGlobal.cpp` and `Dx12/AgilitySdk.cpp` declare no module so no link enumerates them,
and the first replaces global `operator new`; `video_encoder`'s ctor/dtor/moves have no wasm
definition; `"web"` is hardcoded as the process-id answer in three modules with no shared home; the
CMake drop path has no coverage beyond its keep-both Windows arm; `gse.vulkan -> gse.aftermath`
(`Vulkan/Device.cppm`) is reserved by path; `--sweep-only` reports no probe verdicts, so the probe
table is unmeasured since the `gse.gpu_profile` move; `Nvidia/Backend.cpp` still has had no compiler
over it; nothing is committed, including the renamed and new files under `Os/` and `Import/` — the
owner's call.

### 2026-10-20 — steering: the four vendored roots judged (hold) — **zero rejected units**, and the engine's whole remaining frontier is two toolchain symbols

**Judged against the pass criteria: still nothing to pass.** (1)-(6) unstarted — nothing ran in a
browser. `scene_reset_cpu` lives in `sandbox:scenarios`, outside both enumerators' roots.

**Confirmed from the artifacts, not the report.** `/tmp/link_engine11.log` (12:17) carries the
summary **`offered 497 · compiled 497 · rejected 0 · assembled 496 · not assembled 1 · undefined 9 ·
other errors 0`** against the previous `493/489/4/488/1/17`. The rejection list is empty — the first
time in this port — and the 9 undefined symbols are exactly 8 `gse::trace::*` from the single
unassembled unit plus `__muloti4`. `Audio/{Native,Web}/Audio.cpp`, `Network/{Native,Web}/Socket.cpp`
and `GpuRecord/{ShaderCompile.cppm,Native/,Web/}` all exist as described; native green and
`0xe836d31697879e27` unmoved. Review found nothing. Verdict: correct, keep.

**So the frontier is now two items and neither is engine C++.** Both are read at source:
1. `link-0389.s:11568` — `call _ZTHN3gse5traceW3gseW4diag3tlsE` with **no `.functype`** for that
   symbol, so `llvm-mc` refuses the unit. GCC's wasm backend declares a `.functype` for ordinary
   external calls but not for the TLS-init wrapper it synthesises for a `thread_local` defined in
   another unit. That is patch **0018**, squarely in charter (new patches 0018+, long rebuilds
   expected), and it is the sole cause of all eight `gse::trace::*` symbols.
2. `__muloti4`, reached from `libc.a(clock_nanosleep.c.obj)` — a **TImode** (128-bit) libgcc
   routine, and `Tools/build_gcc_wasm.py:1162-1174` already records that the `wasm*-*-*` arm of
   `libgcc/config.host` leaves some primitives undeclared because wasm32 has no 128-bit support.
   Measure before changing anything: it may be reachable only from a `clock_nanosleep` path a
   headless scenario never takes, in which case the honest answer is a definition supplied at link
   (or the routine enabled in libgcc), not a hunt through wasi-libc.

**Both want the same hours-long rebuild, so they are one task, in that order.** A patch that is
re-cut after being applied forces the source tree to be deleted, so 0018 must be right before it is
stamped; `__muloti4` should be settled first by reading, and only its *fix* bundled into the same
rebuild if it needs one.

**Next task:** cut patch 0018 so the wasm backend emits a `.functype` for the TLS-init wrapper it
calls, rebuild the toolchain, and settle `__muloti4`. Judged on: `not assembled 0`, the eight
`gse::trace::*` symbols defined, and `undefined` reaching 0 — a **linked wasm module for the engine
closure** is the win. Run `--link-engine` **in the task, in the foreground**; three background runs
have died in this ledger and only a foreground run promoted after its timeout has ever survived.
Native build green and `0xe836d31697879e27` unmoved (no engine C++ should change at all).

**Then the Sandbox link**, which is where criterion (1) first becomes reachable and where the Web
arms are observed rather than merely compiled.

**Carried forward unchanged:** the Web arms compile but have never run, and `GpuRecord/Web/
ShaderCompile.cpp`'s empty compile result would surface at device pipeline creation — reported, not
papered over, and a question for the headless host once something runs; `Diag/AllocTrackGlobal.cpp`
and `Dx12/AgilitySdk.cpp` declare no module so no link enumerates them, and the first replaces global
`operator new`; `video_encoder`'s ctor/dtor/moves have no wasm definition; `"web"` is hardcoded as
the process-id answer in three modules; the CMake drop path has no coverage beyond its keep-both
Windows arm; `gse.vulkan -> gse.aftermath` (`Vulkan/Device.cppm`) is reserved by path; `--sweep-only`
reports no probe verdicts, so the probe table is unmeasured since the `gse.gpu_profile` move;
`Nvidia/Backend.cpp` still has had no compiler over it; nothing is committed — the owner's call.

### 2026-10-20 — steering: **the engine closure links** — patch 0018 judged (holds), zero rejected, zero undefined; the subject is now the Sandbox and the first run

**Judged against the pass criteria: still nothing to pass, and for the first time that is about to
change.** (1)-(6) unstarted — nothing ran, in a browser or anywhere, because `engine.wasm` has no
entry point and `scene_reset_cpu` is not in the link.

**Confirmed from the artifacts, not the report.** `/tmp/link_engine13.log` (13:10) carries the
inventory **`units offered 497 · compiled 497 · rejected 0 · assembled 497 · not assembled 0 ·
undefined 0 · other errors 0 · wasm-ld exit 0`**, `grep -c 'undefined symbol'` over the whole log is
**0**, and `.gcc-wasm-build/build/gcc/engine.wasm` exists at 11,371,823 bytes dated 13:10. Against
the previous `497/497/0/496/1/9` that is the last unassembled unit and the last nine undefined
symbols — `gse::trace::*` and `__muloti4` — gone. `Tools/gcc-toolchain/wasm/0018-local-called-function-decls-and-muloti4.patch`
exists and is named in `build_gcc_wasm.py:1403`'s `PATCH_NAMES`. `touched.txt` lists only two GCC
source files, `t-wasm`, the Tools script and the patch README — **no engine C++ at all**, so the
unmoved `0xe836d31697879e27` is consistent by construction rather than evidence, and nothing native
can have changed. Verdict: correct, keep. The backend fix (declare any function named in the
instruction stream, mirroring the existing data-reference site) rather than a front-end
`DECL_EXTERNAL` change is the right call and closes the class; `__muloti4`'s 256-bit product was
differentially checked against `__int128` over 4,000,361 cases.

**The review's finding is the load-bearing part of this task and is worth restating:** 0018 was cut
without the snapshot and series records the README mandates, and the same omission predated it at
`0005`, `0007` and `0010` — one documented re-cut command would have swallowed five patches. All
four commands are now deleted rather than repointed (a visibly wrong command beats a quietly wrong
one), `baseline-0017/` exists, and the README gained a copy-pasteable ownership check that
re-derives file ownership from the patches. Given this ledger's standing rule that a re-cut patch
forces the GCC source tree to be deleted, that check is the cheapest insurance in the toolchain.

**New, unexplained, and recorded rather than chased:** the link now emits
`wasm-ld: warning: function signature mismatch: strtold` — `(i32,i32)->f64` in
`libstdc++.a(c++locale.o)` against `(i32,i32,i32)->void` in `libc.a(strtod.c.obj)`. A warning, not
an error; the link succeeds. It matters only if something in the scenario's path calls `strtold`,
which the float parser deliberately no longer does. Left as an observation for the first run to
settle empirically.

**Next task: the Sandbox link, and the first execution.** This is where criteria (1)-(2) first become
reachable, and it is two things in one task because a link with no run proves nothing here:
1. **Bring `sandbox` into the enumerators.** `Tools/module_graph.py:50`'s
   `INTERFACE_ROOTS = ("Engine/Engine/Source", "Engine/Engine/Import")` is why
   `Sandbox/Sandbox/Source/Sandbox/Scenarios.cppm` has never been offered. Extend it — one
   declaration, both walks, the discipline this port has now used four times — and link the closure
   of the Sandbox's own module alongside `gse`/`gse.scenario`.
2. **Give it an entry point and run it.** The existing `probe_status` path already does
   `llvm-mc` → `wasm-ld` → `node --no-warnings harness.mjs` with `crt1-reactor.o`; a scenario run
   needs a `main` (or an exported entry the harness calls) that names `scene_reset_cpu` and lets the
   fixed-step clock drive its own 240 frames to exit. Reuse `harness_text()`/`harness.mjs` rather
   than inventing an output format, and reuse `parity_gate.py:11`'s regex contract for the hash line.

**Judged on facts produced, with a deliberately graded line** — this is the first run of engine code
and a complete inventory is a real deliverable:
- **best:** node runs the module, the console carries `world-state hash 0x<16 hex>` and the p50 row,
  and the process exits 0. If it does, record the hash — it is not expected to equal
  `0xe836d31697879e27` (that is native AVX2; criterion (3) compares against a *portable-SIMD* native
  build, which is still unverified as configurable);
- **good:** it links and runs but traps or exits non-zero — then the deliverable is the trap site and
  the enumerated reason, since eleven `*/Web/*.cpp` arms have compiled and none has ever executed;
- **acceptable:** it does not link — then the deliverable is the enumerated undefined-symbol and
  rejection set for the Sandbox's own units, which is information this ledger has never had.

Run the link **in the task, in the foreground** — every background measurement in this ledger has
died. No engine behaviour change beyond what a compile error forces; anything that would delete or
disable a native path is escalated, not done. Native build green and `0xe836d31697879e27` unmoved.

**Expected to surface on the first run, named so they are diagnosed rather than rediscovered:**
`Os/Web/ImageLoader.cpp` and `GpuRecord/Web/ShaderCompile.cpp` assert or return empty, and a
`headless = true` scenario may or may not reach them — this run is what settles the record's open
assumption about what `headless` still demands of `gse.gpu`; `Diag/AllocTrackGlobal.cpp` declares no
module so no link enumerates it, and it replaces global `operator new`, which a run will notice if
the link does not; `video_encoder`'s ctor/dtor/moves have no wasm definition; `--max-memory` is still
the harness's 16 pages, which 11MB of module plus a world is unlikely to fit.

**Carried forward unchanged:** the probe suite has not run since the `gse.gpu_profile` move, so the
four probes exercising exactly 0018's declaration paths (`funcptr`, `comdat_tu`, `alias`,
`weak_undef`) are unmeasured against it, and `0017`'s "a called weak reference still has no
signature" may be closed by 0018 but nothing exercises it — one full `--sweep`, run from a task,
closes both; seven patch boundaries still have no baseline snapshot and cannot be re-cut without
replaying onto a fresh extract; `"web"` is hardcoded as the process-id answer in three modules;
the CMake drop path has no coverage beyond its keep-both Windows arm; `gse.vulkan -> gse.aftermath`
(`Vulkan/Device.cppm`) is reserved by path; `Nvidia/Backend.cpp` still has had no compiler over it;
nothing is committed — patch 0018, the README, `baseline-0017/`, both documents and every `*/Web/`
arm — the owner's call.

### 2026-10-20 — steering: the Sandbox links and the undefined set is enumerated (the "acceptable" tier, met); the last three arms are all in charter

**Judged against the pass criteria: still nothing to pass.** (1)-(6) unstarted — nothing executed, so no
hash, no p50, no exit 0. The previous entry's graded line is met at its **acceptable** tier and not
above it: the deliverable is the enumerated undefined-symbol set, which this ledger has never had for
the Sandbox.

**Confirmed from the artifacts, not the report.** `/tmp/run_scenario2.log` (14:22) carries
`units offered 549 · compiled 549 · rejected 0 · assembled 549 · not assembled 0 · undefined 22 ·
other errors 0 · wasm-ld exit 1`, and the node stage reports `did not instantiate: ENOENT ...
scenario.wasm` — i.e. no module was produced, so `world-state hash (none printed)` is a consequence of
the link, not of the harness. `grep -oE 'undefined symbol: .*' | sort -u` yields exactly 22 and they
sort into three groups, not two as the report counted: **10** `gse::window::*` (`tick`, `show`,
`shutdown`, `viewport`, `minimized`, `wait_events`, `post_wake`, `find_surface`,
`frame_buffer_resized`, `install_resize_pump`), **11** `gse::gpu::video_encoder::*` (ctor, dtor,
`operator=`, `begin_capture`, `set_bitrate`, `submit_ready`, `take_bitstream`, `stream_header`,
`codec`, `extent`, `valid`), and **1** `gse::system_clock::from_query_performance_counter`. Native
green and `0xe836d31697879e27` unmoved per the task. `touched.txt` lists only the three Tools files,
so the `Engine/Server` extraction and the CMake changes are in the working tree but outside the
editor's touched set — worth knowing when reading a later build-state line, and `git status` confirms
`Engine/Server/{Win32,Web}/`, `Engine/cmake/GSEEngine.cmake`, `Engine/CMakeLists.txt` and
`Sandbox/CMakeLists.txt` are all modified or untracked. Verdict: correct, keep.

**Two review findings are the load-bearing part of the task and both are recorded rather than
summarised away.** A module-partition `.cpp` handed to `target_sources(... PRIVATE)` configures and
scans cleanly and then fails collation in a `cmake -E` step — producing a native build failure with
*no* diagnostic anywhere, which is also what the recorded capability gap describes. And the module is
a reactor, so buffered stdout was never flushed: `__wasm_call_dtors` is now exported and called after
the entry returns. That second one is directly load-bearing on criterion (2) and is still
**unexercised**, because nothing has reached the hash.

**All three remaining arms were read at source and all three are in charter**, so this is an ordinary
task and not an escalation:
- **`Os/Web/Window.cpp` does not exist** — `Os/Web/` holds `App`, `Clipboard`, `ImageLoader`,
  `Process` and no window arm, while `Os/Win32/Window.cpp` is the sole `:window_impl`.
  `Os/Win32/Window.cppm` is the platform-neutral interface the selection rule deliberately keeps. The
  10 symbols are a headless window arm, and `Os/**` is charter-authorized by path. **This is the
  measurement the record's step 3 wanted:** `headless = true` still demands `window::tick`,
  `wait_events`, `viewport` and `minimized`, so the runtime's loop references them unconditionally.
- **`Gpu/Device/VideoEncoder.cpp` is in `native_gpu_backend_units.txt`** and is dropped from the wasm
  configuration, which is why `video_encoder`'s own ctor/dtor/accessors have no definition —
  `Gpu/Device/Web/DeviceVideo.cpp` already returns `std::nullopt` from `make_video_encoder` but
  defines none of the class's members. A `Gpu/Device/Web/VideoEncoder.cpp` arm is exactly the shape
  the owner granted for `Gpu/Device/`, so no second escalation on the reserved path is needed.
- **`Time/Web/` does not exist**; `Time/Win32/SystemClock.cpp` is the only definition of
  `from_query_performance_counter`, reached from `Gpu/Graph/RenderGraph.cpp:422`. `gse.time` is named
  by the charter.

**Next task: the three arms, then run it.** `Os/Web/Window.cpp` (a headless window: no surface, no
events, a viewport the scenario's settings imply, `wait_events`/`post_wake` as a no-op or an honest
unsupported report through whatever channel each caller already has), `Gpu/Device/Web/VideoEncoder.cpp`
(an invalid encoder — `valid()` false, accessors returning the neutral empty answer, never a silent
success), and `Time/Web/SystemClock.cpp` (a QPC-tick conversion with no QPC; if there is no honest
answer, say so in the report rather than inventing a timebase, since this value feeds GPU/host
calibration a headless run may never consult). Each arm decided and reported on its own merits, never
swept. Judged on the same graded line, and now the top tier is genuinely in reach: **best** — node
runs the module, the console carries `world-state hash 0x<16 hex>` and the p50 row, exit 0; record the
hash and do **not** expect `0xe836d31697879e27` (that is native AVX2; criterion (3) compares against a
portable-SIMD native build, still unverified as configurable); **good** — it links and runs but traps
or exits non-zero, deliverable the trap site and the enumerated reason, which is the first observation
of any of the twelve-plus `*/Web/*.cpp` arms actually executing; **acceptable** — a new enumerated
undefined set. Run `--run-scenario scene_reset_cpu` **in the task, in the foreground** — every
background measurement in this ledger has died.

**Expected to surface on the first execution, named so it is diagnosed rather than rediscovered:**
`Os/Web/ImageLoader.cpp` asserts and `GpuRecord/Web/ShaderCompile.cpp` returns empty, and a headless
scenario may or may not reach either; `Diag/AllocTrackGlobal.cpp`, `Dx12/AgilitySdk.cpp` and
`Sandbox/Main.cpp` declare no module, so no link enumerates them and the first replaces global
`operator new` — the log names all three explicitly now; the asset preopen root is unmeasured because
nothing has yet asked the filesystem for a scene; and the `strtold` signature-mismatch warning from the
engine link is still unexplained and settleable only by a run.

**Carried forward unchanged:** sweep unit counts are no longer comparable with the `host=34 ok=387`
baseline — the three new `INTERFACE_ROOTS` changed the measured population, so a future sweep is a new
baseline, not a regression check; the probe suite has not run since the `gse.gpu_profile` move, so
0018's four declaration-path probes are unmeasured; seven patch boundaries have no baseline snapshot;
`"web"` is hardcoded as the process-id answer in three modules; the CMake drop path has no coverage
beyond its keep-both Windows arm; `gse.vulkan -> gse.aftermath` is reserved by path;
`Nvidia/Backend.cpp` still has had no compiler over it; nothing is committed — the owner's call.

### 2026-10-20 — steering: **engine code runs** — the Sandbox module links clean and executes; the first stop is `gse.config`'s manifest search, and it is a path, not a filesystem

**Judged against the pass criteria: still nothing to pass, but for the first time the obstacle is
inside a running program.** (1)-(6) unstarted — no hash line, no p50 row, no exit 0.

**Confirmed from the artifacts, not the report.** `/tmp/run_scenario3.log` (15:02) carries
`units offered 552 · compiled 552 · rejected 0 · assembled 552 · not assembled 0 · undefined 0 ·
other errors 0 · wasm-ld exit 0`, `grep -c 'undefined symbol'` over the whole log is **0**, and
`.gcc-wasm-build/build/gcc/scenario.wasm` exists at 77,405,484 bytes. `touched.txt` is the four Web/
neutral arms plus `Tools/build_gcc_wasm.py` and the two Window units; `Os/Window.cpp` is the only
edit reaching native, and `0xe836d31697879e27` is reported unmoved. Review found nothing. Verdict:
correct, keep — **the "good" tier of the graded line is met and the first engine execution has
happened.**

**The one claim the report could not evidence is now evidenced, by reading the right file.** That
same log's node stage still reads `did not instantiate: ... memory import has 2 pages`, because it
predates the harness fix; `harness.mjs` (15:03) now reads `INITIAL_PAGES = 256, MAX_PAGES = 8192`,
and `/tmp/node_scenario1.log` is the post-fix node run:

```
gse.config: no gse.manifest found within 8 directories above the executable; run from a build tree or an install image
gse.config: cannot resolve engine paths; exiting.
trapped: undefined
```

Three lines of engine output. So the module instantiates, static constructors run, `entry()` is
entered, `gse::log` reaches stdout through the WASI arm, and the program declines for a stated
reason rather than trapping blind. Every one of those was unproven an hour ago.

**And the blocker is narrower than "assets".** The record's step 4 anticipated a read-only VFS over
preloaded bytes; that is not what this is. `Engine/Engine/Import/Web/Config.cpp:6` returns a
**fabricated** executable path, `/gse/engine.wasm`, with `/gse/roaming|home|local` beside it, while
the harness preopens the *repo root* (`harness.mjs:21`'s `ROOT`, `argv[5]`, passed as the repo path).
Nothing named `/gse` exists in the mounted tree, so the 8-level walk above the executable cannot
find a manifest and never touches the filesystem in anger. The three `gse.manifest` files in the
tree are `out/build/x64-mingw-gcc-Release/`, `dist/engine-sdk/...` and `dist/game/...` — i.e. the
native build dir is already a valid search root *inside the preopen*, so the first move is to make
the reported executable path a real path in the mounted tree, not to write a VFS. Whether what the
manifest then points at resolves under WASI is the next thing the same task measures, and that is
where a VFS may or may not become necessary.

**Next task: make the wasm run resolve its engine paths, then run it.** Give `Import/Web/Config.cpp`
an executable path inside the preopened tree rather than an invented one — the harness already knows
the root, so passing it in (an `--engine-setting`-style argument, a WASI environment variable, or a
preopen chosen to put the manifest within 8 levels) is preferable to hardcoding a build-directory
name in engine source, and the choice should be stated in the report. Then follow the run wherever it
goes next: the writable `/gse/roaming|home|local` paths have the same problem and `gse.save` /
`layout_store` will ask for them, so decide per path whether a browser run wants a real mount, a
scratch directory under the preopen, or an honest unsupported report through the channel the caller
already has. In charter: `gse.fs`, `gse.save` and the spine siblings are named, and `Import/Web/` is
the seam this port has already established.

**Judged on the same graded line, top tier now genuinely in reach:** **best** — the console carries
`world-state hash 0x<16 hex>` and the p50 row and the process exits 0; record the hash and do **not**
expect `0xe836d31697879e27` (that is native AVX2; criterion (3) compares against a portable-SIMD
native build, still unverified as configurable). **good** — it runs further and stops for a new,
named reason, with the stop site and the enumerated cause as the deliverable. **acceptable** — a new
undefined set. Run `--run-scenario scene_reset_cpu` **in the task, in the foreground**, and report
the full inventory, which also closes the one gap this entry had to work around: the memory-page fix
has never been through a full pipeline run.

**Expected next, named so they are diagnosed rather than rediscovered:** `Os/Web/ImageLoader.cpp`
asserts and `GpuRecord/Web/ShaderCompile.cpp` returns empty, and a `headless = true` scenario may or
may not reach either — this run is what finally settles the record's open assumption about what
`headless` demands of `gse.gpu`; the stdout flush at exit (`__wasm_call_dtors`, exported but
unexercised) is the first suspect if a run ever exits 0 with no hash line; and `Sandbox/Main.cpp`
declares no module, so no link enumerates it — the `entry()` export is what stands in for it.

**Carried forward unchanged:** the `strtold` signature-mismatch warning is still unexplained and
still only a warning, and the float parser deliberately no longer calls it; sweep unit counts are no
longer comparable with the `host=34 ok=387` baseline since `INTERFACE_ROOTS` grew; the probe suite
has not run since the `gse.gpu_profile` move, so 0018's four declaration-path probes are unmeasured;
seven patch boundaries have no baseline snapshot; `"web"` is hardcoded as the process-id answer in
three modules; the CMake drop path has no coverage beyond its keep-both Windows arm;
`gse.vulkan -> gse.aftermath` is reserved by path; `Nvidia/Backend.cpp` still has had no compiler
over it; nothing is committed, including these four arms — the owner's call.

### 2026-10-20 — steering: the env/path arm and `fd_readdir` judged (hold); the reported stop-3 had no artifact, so it was re-run here and is confirmed — a **cross-module function-pointer** trap, and the probe suite has no row for that shape

**Judged against the pass criteria: still nothing to pass.** (1)-(6) unstarted — no hash line, no p50
row, no exit 0. The graded line is met at **good**: the run goes further and stops for a new, named
reason.

**Confirmed at source.** `Engine/Engine/Import/Web/Config.cpp` reads `GSE_EXECUTABLE_PATH` and
returns `{}` from `known_folder`, with the fabricated `/gse/...` paths gone; `Import/Config.cpp:176`
carries `env_path("GSE_ROOT_DIR")` beside the four existing overrides, and `:152` spells
`generic_string<std::filesystem::path::value_type>()`; `harness.mjs` carries `fd_readdir` at `:129`,
`INITIAL_PAGES = 256`, and `e.stack` on trap. `touched.txt` is three files and the only engine edit
reaching native is `Import/Config.cpp`, so `0xe836d31697879e27` unmoved is the right check.

**But the report's stop-3 was unevidenced, and this run fixed that rather than accepting it.** The
on-disk `run-scenario.log` is the **15:48** run and stops at `Engine.cpp:374`, *"Asset discovery
failed: Function not implemented"* — the ENOSYS the task then fixed. `harness.mjs` is dated **15:56**,
*after* that link and that run, so no `--run-scenario` had ever exercised `fd_readdir`, and the
claimed `run_hook` trap was in no artifact at all. Since `scenario.wasm` (15:48) already contains the
`generic()` fix and the only later change is the harness, the node stage was re-run here, unchanged
command line, against the current harness (`/tmp/steer_node_run1.log`). **Both claims reproduce:**
the ENOSYS line is gone — asset discovery passes, so the harness `fd_readdir` works in the engine and
not only in isolation — and the run reaches ECS init and traps exactly as reported.

**Lesson, a variant of one this ledger already carries:** the rule has been "run the measurement in
the task". It is not enough on its own — the task ran it, then changed a load-bearing file afterwards
and reported a state no artifact held. The measurement must be the **last** thing a task does.

**The trap, read at source and narrowed.** V8's stack names it precisely:
`scheduler::advance_one_init_system` → `when_all` → `task::start` → `run_hook` → `wrap_run_task`,
`null function or function signature mismatch`. `Ecs/Scheduler.cpp:314`'s `run_hook` makes one
indirect call, `invoke(ctx, node.data.get())`, through
`auto (*)(context&, void*) -> async::task<>`; the pointer is `node.invoke_init_fn`, set at
`Ecs/SystemManifest.cppm:366` to `&invoke_annotated_fn<FnInfo, State>` and guarded non-null at
`Scheduler.cpp:782`. So it is not a null C++ pointer — it is a wasm `call_indirect` whose table slot
is empty or whose declared type disagrees, i.e. a codegen/relocation class, not an engine-logic one.
It is also the first fault of this port that is neither a syscall, a path, nor a missing definition.

**And the probe suite has no row for its shape, which is the lead.** `funcptr`
(`Tools/build_gcc_wasm.py:796`) takes the address of two `extern "C"` functions **defined in the same
TU** — so the definition's own signature wins and a table index is emitted locally. The engine case is
a pointer to a *template instantiation* in another module's interface, whose address is taken in one
TU and called in another, returning a coroutine by value. That is exactly the gap `extern_varargs`
exists to cover for signatures (its comment says so: `varargs` cannot catch it because there the
definition is in the same TU). Patch 0018 taught the backend to declare functions *named in the
instruction stream*; a function whose **address is taken** but which is never called in that TU is the
adjacent case and may have the same omission. In charter either way — new patches 0018+ are authorized,
and `Ecs/` is a named spine module.

**Next task: diagnose the indirect call, then fix it where the measurement points.** Do not patch the
toolchain on the strength of the paragraph above. Order:
1. **Reproduce minimally** through the existing probe machinery — a `Probe` with `second=` where one
   TU defines a function template instantiation, another takes its address and a third site calls it
   through the pointer, ideally returning a class by value. Add the row whatever the verdict, because
   this is the shape the suite is missing.
2. **Read the artifact, not the guess.** The `.s` for `Ecs/Scheduler.cpp`'s unit and for whichever
   unit instantiates `invoke_annotated_fn` are in `.gcc-wasm-build/build/gcc/link-*.s`; compare the
   `.functype` for `invoke_annotated_fn` against the `call_indirect` type at the `run_hook` site, and
   check whether the pointer is emitted as a table index (`R_WASM_TABLE_INDEX_*`) at all. An empty slot
   and a type disagreement are different bugs with different fixes.
3. **Fix at the layer the measurement names** — a patch 0019 if it is a backend omission, the engine if
   the C++ genuinely asks for something wasm cannot express. Escalate rather than guess if the fix
   would change native behaviour or delete a native path.

Judged on: the minimal probe added and its verdict recorded; the diagnosis stated as an artifact
citation rather than a theory; and, if a fix lands, the run reaching past ECS init. **Best** remains
`world-state hash 0x<16 hex>` plus the p50 row and exit 0 — and do not expect `0xe836d31697879e27`
(native AVX2; criterion (3) compares against a portable-SIMD native build, still unverified as
configurable). Run `--run-scenario scene_reset_cpu` **in the task, in the foreground, as the last
action**, and note that the node stage alone is ~30 seconds against ~25 minutes for a full relink —
if nothing in the link changed, re-running node directly is the cheap check.

**Also worth the next task's time, cheap and adjacent:** the full probe suite has not run since the
`gse.gpu_profile` move, so 0018's four declaration-path probes are unmeasured against it; a
`--sweep` would close that and the new row at once.

**Carried forward unchanged:** `fd_readdir` omits `.` and `..` and reads as end-of-directory rather
than truncation if one entry exceeded the whole buffer (needs a ~1000-character name, unreachable
here); the harness assumes every directory fd descends from a preopen via `path_open`, else `EBADF`;
`Os/Process.cppm:122,125` still call `path::wstring()`, the same null-codecvt trap the `generic()` fix
closed, reachable only if something spawns a process; `Os/Web/ImageLoader.cpp` asserts and
`GpuRecord/Web/ShaderCompile.cpp` returns empty, neither reached yet; the `strtold` link warning is
still unexplained and still only a warning; `Diag/AllocTrackGlobal.cpp`, `Dx12/AgilitySdk.cpp` and
`Sandbox/Main.cpp` declare no module so no link enumerates them; seven patch boundaries have no
baseline snapshot; `"web"` is hardcoded as the process-id answer in three modules; the CMake drop path
has no coverage beyond its keep-both Windows arm; `gse.vulkan -> gse.aftermath` is reserved by path;
`Nvidia/Backend.cpp` still has had no compiler over it; nothing is committed, including
`Engine/Engine/Import/Web/` — the owner's call.

### 2026-10-20 — steering: patch 0019 judged (holds) — **the wasm run reaches `gpu::context::init` and declines for a stated reason**; and the headless-device question has an answer already in the engine, so it is not the owner's after all

**Judged against the pass criteria: still nothing to pass.** (1)-(6) unstarted — no hash line, no p50
row, no exit 0. The graded line is met at **good**: the run goes further and stops for a new, named
reason.

**Confirmed from the artifacts, not the report.**
`Tools/gcc-toolchain/wasm/0019-local-noop-coroutine-frame-signature.patch` exists and is named in
`build_gcc_wasm.py:1713`'s series list. `touched.txt` is two files — the Tools script and the patch
README — so **no engine C++ changed at all** and the unmoved native hash is consistent by
construction rather than evidence. `/tmp/wasm_full_0019.log` (16:31) is a *full* `--sweep`, the first
since the `gse.gpu_profile` move, and its probe table shows `funcptr_tu linked RAN` and
`coroutine_resume linked RAN` — both new rows, both green — with every other row unchanged save one.
`/tmp/run_scenario_0019.log` (16:48) links clean and its node stage is the measurement: V8's stack now
runs `coroutine_handle::resume` → `wrap_run_task` → `run_hook` → `invoke_annotated_fn` →
`gpu::context::init` → `unique_ptr<gpu::device>::operator*` → `__glibcxx_assert_fail`. The
`call_indirect` signature mismatch that stopped the previous run is gone; the coroutine machinery
resumes. Verdict: correct, keep.

**The diagnosis the task reported is the right one and its shape is worth keeping:** the trap was
never the engine's function pointer — `std::noop_coroutine()`'s static frame declares its
resume/destroy slots as `void (*)()` while generated code calls them as `void (*)(void*)`. Every
target with a C calling convention ignores the extra argument; wasm type-checks at run time. Fixing
the libstdc++ header rather than teaching GCC a `__builtin_coro_noop`, and fixing the toolchain
rather than routing the engine away from a correct use of `noop_coroutine()`, are both the right call
under this charter. The `funcptr_tu` row is a **true negative** deliberately kept — it is the shape
the old `funcptr` row could not ask (address taken in one TU, definition a template instantiation in
another module's interface), and it cleared the original suspect.

**What the run actually says, read line by line, because it answers an open assumption this record
has carried since it was written.** Before the trap the engine prints:
`system 'gse::window::data' not registered: disabled in this mode`,
`system 'gse::audio::data' not registered: disabled in this mode`, and then
`error/render: no gpu backend is compiled into this configuration; device creation is unsupported` —
i.e. the Web `create_device` arm reports honestly, `gse::log` reaches stdout through the WASI arm,
and `Gpu/Context.cpp:19` then dereferences the null `unique_ptr` it was handed. So the record's
assumption "**`headless = true` may still construct a GPU device**" is settled: **it does**, and a
null backend behind `backend_kind` is *not* needed.

**And the fix is not in `Gpu/` at all, which is why this is not an escalation.** `engine_config`
already has a first-class `bool gpu = true` (`Runtime/Engine.cppm:68`), and
`Runtime/Engine.cpp:280-283` does exactly the right thing with `gpu = false`: it inserts
`id_of<gpu::context::data>()` into `disabled_roots` and logs *"gpu disabled by config: no device will
be created, graphics assets load CPU-side only, and every system that requires a device is dropped"*.
That is the same `resolve_activation` mechanism that printed the two "disabled in this mode" lines
above, so `gpu::context::init` would never run. `scenario::info` has `headless` and `gpu_solver` and
no `gpu` field, and `Main.cpp:114`'s `apply_scenario` sets `create_window = false` and
`render = false` for a headless scenario but leaves `gpu` alone — so today's native headless runs do
build a device, and **changing that would be a native behaviour change the charter reserves.**
Measured caveat, stated rather than assumed: `grep` finds **no caller anywhere** that sets
`gpu = false`, so the path exists and has never been exercised. It may well surface more work;
that is the measurement, not a reason to avoid it.

**Next task: run the wasm scenario with `gpu = false`, from the harness argv and nowhere else.**
`SCENARIO_ENTRY` (`Tools/build_gcc_wasm.py:725`) builds `{"sandbox", "--engine-bench-scenario",
"<name>"}` and calls `__main_argc_argv`, deliberately reusing `apply_scenario` so the configuration
*is* the native one. Add the engine flag that clears `engine_config::gpu` to that argv — verify at the
compiler/run that `gse::parse_args` really derives `--no-engine-gpu` from the field rather than
assuming the spelling, and report the spelling it accepted. `Tools/build_gcc_wasm.py` is
charter-authorized and **no engine source should change**, which is also the check: the native hash
cannot move because nothing native was touched. If the flag is not derivable and the only route is a
`scenario::info` field or an `apply_scenario` change, **stop and escalate** rather than altering what
a native headless run does.

Then follow the run wherever it goes: with `gpu::context::data` disabled, expect the next stop to be
a system that required the device and was dropped, or the asset path loading graphics assets CPU-side
(`Os/Web/ImageLoader.cpp` asserts, `GpuRecord/Web/ShaderCompile.cpp` returns empty — neither reached
yet). Judged on the same graded line: **best** — `world-state hash 0x<16 hex>` plus the p50 row and
exit 0, and do **not** expect `0xe836d31697879e27` (native AVX2; criterion (3) compares against a
portable-SIMD native build, still unverified as configurable); **good** — a new named stop with the
site and enumerated cause; **acceptable** — a new undefined set. Run `--run-scenario scene_reset_cpu`
**in the task, in the foreground, as the last action** — the node stage alone is ~30s against ~25min
for a relink, so if nothing in the link changed, re-running node directly is the cheap check.

**New open item, from this sweep and pre-existing:** the `libcall` probe regressed from `RAN` to
`FAILED: assemble failed` — `__powidf2`'s declared signature disagrees with its call site, the same
class patch 0018 addressed, and the full suite line reads `probes failed: libcall/linked`. Nothing in
the engine link references it, so it is not on this goal's path; it wants its own patch and rebuild.

**Also recorded:** the review re-derived 0019's two load-bearing claims at source (nothing else in
libstdc++ reads those slots; no other patch touches that file, so it needs no baseline snapshot). The
patch was **not** proven red without itself — un-stamping an applied patch forces deleting the GCC
tree and a multi-hour rebuild — so the evidence is direct instead: the trapping instruction, the two
disagreeing signatures in one unit, and the `.functype` flipping after the patch. That is an honest
substitute and is the right trade here. The README's `0019` bullet has no detail section below it
unlike `0002`-`0018`; convention only, the rationale is in the bullet and the preamble.

**Carried forward unchanged:** sweep counts (`host=33 ok=422`, no failed or blocked units) are a new
baseline, not comparable with `host=34 ok=387` — the interface roots grew when the Sandbox came in;
`fd_readdir` omits `.` and `..`; `Os/Process.cppm:122,125` still call `path::wstring()`, reachable
only if something spawns a process; the `strtold` link warning is still unexplained and still only a
warning; `Diag/AllocTrackGlobal.cpp`, `Dx12/AgilitySdk.cpp` and `Sandbox/Main.cpp` declare no module
so no link enumerates them; seven patch boundaries have no baseline snapshot; `"web"` is hardcoded as
the process-id answer in three modules; the CMake drop path has no coverage beyond its keep-both
Windows arm; `gse.vulkan -> gse.aftermath` is reserved by path; `Nvidia/Backend.cpp` still has had no
compiler over it; nothing is committed — 0019, the README, the Tools work and every `*/Web/` arm —
the owner's call.

### 2026-10-20 — steering: `--no-engine-gpu` judged (holds, the "good" tier); **the spin is in the init-phase scheduler wait, and the engine's own stall dump cannot fire there — on any platform**

**Judged against the pass criteria: still nothing to pass.** (1)-(6) unstarted — no hash line, no p50
row, no exit 0. The graded line is met at **good**: a new named stop, not a passing scenario.

**Confirmed from the artifacts, not the report.** `touched.txt` is `Tools/build_gcc_wasm.py` alone,
and `build_gcc_wasm.py:741` reads
`{"sandbox", "--no-engine-gpu", "--engine-bench-scenario", "%(scenario)s"}` with the rationale
comment above it — so **no engine C++ changed** and the unmoved `0xe836d31697879e27` is consistent by
construction as well as measured. `/tmp/run_scenario_nogpu.log` carries
`units offered 552 · compiled 552 · rejected 0 · assembled 552 · not assembled 0 · undefined 0 ·
other errors 0 · wasm-ld exit 0`, then the ten console lines: `gpu disabled by config: …`, the three
`not registered: disabled in this mode` rows (`window`, **`gpu::context`**, `audio`), and
`no gpu context: textures, models and skinned models will load without device resources`
(`Assets/AssetState.cppm:59`) at +0.18 s — then nothing for 900 s, `node exit 124`,
`world-state hash (none printed)`. Verdict: correct, keep. The 75-second node-only localisation was
the right spend and its conclusion (one core saturated, 13 threads alive, no spawn failure → a
busy-wait, not a blocked join) stands.

**The region is now a function, read at source, and the reading also explains the silence.** The
main thread's busy-wait is `scheduler::sync_wait_or_dump` (`Ecs/Scheduler.cpp:1392`): it starts a
`when_all` wrapper and then loops `task::try_run_one()` / `std::this_thread::yield()` until
`done_flag` — exactly a saturated core with live workers and no progress. That loop is reached from
`advance_inits` (`:899`, `wait_phase::init`), i.e. inside `engine::initialize`. **And the loop's own
diagnostic cannot fire there:** it prints `log_stall_state` only when `watchdog::dump_pulse()` moves,
and `watchdog::start()` is called at `Runtime/Bootstrap.cpp:104` — *after* `e.initialize(setup)`. So
during init the monitor thread does not exist, `dump_pulse` is frozen at 0, and an init-phase stall
is silent **on Windows too**. That is why 900 s produced no `WATCHDOG STALL` line, and it is not a
wasm defect.

**Next task: name the stalling init system, then decide where the bug is.** Two routes, cheapest
first, and the deliverable is the name — not necessarily a fix:
1. **No engine change, no relink: profile the running module.** `scenario.wasm` is already linked and
   the node stage is ~30 s. Run it under `node --cpu-prof` (or `--prof`) from the harness and read
   the wasm frames in the spin: the profile names the function the main thread burns in and, more
   usefully, what `try_run_one` is polling. `Tools/build_gcc_wasm.py` and `harness.mjs` are
   charter-authorized, so this costs a minute and zero engine risk. Do this before touching C++.
2. **If the profile is not conclusive, make the existing stall dump reachable in the init phase.**
   `log_stall_state(wait_phase::init, …)` already enumerates per-node `init_launched`/`init_done`,
   which is precisely the answer wanted. Getting it to fire means either starting the watchdog before
   `initialize` or giving the init wait its own elapsed-time trigger — **both change what a native
   run does**, so decide it explicitly and report it; if the only honest shape is a native behaviour
   change beyond a diagnostic that is dormant unless a budget is exceeded, **escalate rather than
   deciding it**. `gse.ecs` and `gse.diag` are charter-named, so the edit itself is in charter; the
   native-behaviour question is what is not.

Also still testable without a relink and still the standing hypothesis: `concurrentqueue.h`'s
`thread_local`-address thread ids, unproven past `-S` since step 1. Colliding ids would make a
publish invisible to the consumer spinning for it. Run it as a probe through the existing machinery;
it is cheap, and a green result removes it from this ledger for good.

Judged on: the stalled init system (or the spun-on condition) named from an artifact, with the
artifact cited. **Best** remains `world-state hash 0x<16 hex>` plus the p50 row and exit 0 — and do
**not** expect `0xe836d31697879e27` (native AVX2; criterion (3) compares against a portable-SIMD
native build, still unverified as configurable). Run the measurement **in the task, in the
foreground, as the last action**.

**New, and it must not be lost before the parity gate is written:** the wasm run now also carries
`gpu = false` and drops `gpu::context::data`, which a native `scene_reset_cpu` does not. **The native
side of criteria (3)/(4) must pass `--no-engine-gpu` too**, or the gate measures the SIMD arm *and*
the dropped GPU context and attributes both to SIMD. Recorded here because it is written down nowhere
else.

**Carried forward unchanged:** `Os/Web/ImageLoader.cpp` asserts and `GpuRecord/Web/ShaderCompile.cpp`
returns empty, neither reached; `__wasm_call_dtors` / the stdout flush at exit is unexercised, so a
run that exits 0 with no hash line should suspect it first; `Time/Web/SystemClock.cpp`'s
`from_query_performance_counter` just returns `now()`, an honest non-answer that nothing has yet
consulted; the `strtold` signature-mismatch link warning and the `libcall`/`__powidf2` probe
regression are both pre-existing and untouched; `fd_readdir` omits `.` and `..`;
`Os/Process.cppm:122,125` still call `path::wstring()`; `Diag/AllocTrackGlobal.cpp`,
`Dx12/AgilitySdk.cpp` and `Sandbox/Main.cpp` declare no module so no link enumerates them; seven patch
boundaries have no baseline snapshot; `"web"` is hardcoded as the process-id answer in three modules;
the CMake drop path has no coverage beyond its keep-both Windows arm; `gse.vulkan -> gse.aftermath` is
reserved by path; `Nvidia/Backend.cpp` still has had no compiler over it; nothing is committed — the
owner's call.

### 2026-10-20 — steering: the init stall dump judged (holds); the window narrows from "two systems never launch" to **`w.start()` plus one symmetric transfer, synchronous, on the main thread**

**Judged against the pass criteria: still nothing to pass.** (1)-(6) unstarted — no hash line, no p50 row,
no exit 0. The graded line is met at **good**: a new named stop, with the stop site identified.

**Confirmed from the artifact, not the report.** `.gcc-wasm-build/build/gcc/run-scenario.log` carries 54
`STALL` lines in exactly three rounds at `elapsed=5008.262 / 10008.681 / 15008.826 ms`, `dump=#1/#2/#3`
and no `#4`, so the interval trigger and the cap are both as described. The two
`init() not complete (launched=false in_flight=false waiting on deps: none)` rows are
`gse::physics::shadow_step::data` and `gse::stateless_system<^^sandbox::client_system>`, byte-identical
across all three dumps; the other sixteen rows per dump are the pre-existing `stuck inside run()` block.
`touched.txt` is five files; the engine edits are the `engine_config` field, one setter call, the setter and
member, and the `sync_wait_or_dump` trigger — and `0xe836d31697879e27` was reprinted with the feature off
*and* with it on, which is the stronger of the two checks and the right one to have run. Review found no
defects. Verdict: correct, keep.

**And the diagnostic earns its keep immediately, because reading the wait function with its answer in hand
collapses the search space.** `sync_wait_or_dump` (`Ecs/Scheduler.cpp:1397-1404`) builds the `when_all`
wrapper and calls **`w.start()` synchronously, before the poll loop is entered**.
`suspend_and_capture::await_suspend` (`Concurrency/AsyncTask.cpp:180-220`) posts `helpers[1..]` through
`task::post_range` and **returns `helpers[0]`'s handle** — a symmetric transfer the compiler resumes on the
calling thread. `advance_one_init_system` (`Scheduler.cpp:1326-1329`) sets `init_in_flight = true` as its
very first statement. So on a correct target, by the time that `while` loop reaches its first iteration,
`init_in_flight` for the first of the two nodes is **necessarily** true. It is false in all three dumps.

**Therefore the failure is before any concurrency.** Either `w.start()` does not execute the wrapper body,
or the handle returned from `await_suspend` is not resumed. No worker, no queue and no memory model is
involved in either. That also *explains* the task's two side measurements rather than leaving them as
coincidences: if `await_suspend` was never reached, `post_range` was never called, nothing released
`work_available`, and the twelve workers park in `semaphore::acquire` exactly as the profile showed — while
the main thread spins in `try_run_one`/`yield` because `done_flag` and `w.done()` are both false forever.
One cause, three symptoms. **It supersedes the handoff's framing** ("why neither the inline nor the posted
helper entered its body"): the posted helper is downstream of the inline one and is not an independent
question.

**The constraint that makes this a codegen question rather than a logic one:** this same
`start()`/`when_all`/symmetric-transfer path demonstrably works earlier in the same run — the settle loop
at `:860-876` has already taken other systems to `init_done`, or all of them would appear in the first
block rather than two. The shape is not uniformly broken, so whatever distinguishes these two nodes is the
lead. The handoff's guess (both are `m_deferred_nodes`, registered at `Engine.cpp:315` immediately before
`boot_immediately`) is still unconfirmed and is now cheap to settle, because it predicts something
checkable about *which* round they are in.

**One prediction to test first, because it costs nothing:** if `await_suspend` is never reached then
`trace::begin_async` for `async::when_all::resume` never fires in the stalling round. The engine's own
trace channel can say so with no relink, through `--run-args`. A relink is ~25 minutes; a flag is ~30
seconds, and this goal has repeatedly spent hours on measurements a flag would have answered.

**Next task: establish which of the two statements above is false, then fix at the layer the measurement
names.** Cheapest first:
1. **No relink.** Re-run with trace/async-event output enabled via `--run-args` and look for
   `async::when_all::resume` and the `run_hook` spans in the stalling round. Present → `await_suspend` ran
   and the fault is downstream of it; absent → the fault is `w.start()` or the `co_await` itself, and
   nothing concurrent is implicated.
2. **One relink, carrying everything.** If the trace is not conclusive, add the narrowest instrumentation
   that separates the two: a log after `w.start()` returns, one in `await_suspend` before the `post_range`
   branch, and `w.done()`/`done_flag` in the existing stall block. Keep it behind the same
   `init_stall_dump` gate so native stays dormant at its default, and report the wording change as the
   last task did.
3. **Read the artifact, not the guess.** `.gcc-wasm-build/build/gcc/link-*.s` holds the units for
   `Scheduler.cpp` and `AsyncTask.cpp`. If the trace says `await_suspend` never ran, compare the
   resume-slot `call_indirect` type at the `co_await` site against the `.functype` of the awaited frame's
   resume slot — patch 0019's class one level out (a *real* frame returned from `await_suspend`, rather
   than `noop_coroutine()`'s static frame). A patch **0020** is in charter, as is the engine side if the
   C++ genuinely asks for something wasm cannot express. Escalate rather than guess if the only fix would
   change native behaviour or delete a native path.

Judged on: the false statement named from an artifact, with the artifact cited — that is the deliverable,
and a fix is the bonus. **Best** remains `world-state hash 0x<16 hex>` plus the p50 row and exit 0, and do
**not** expect `0xe836d31697879e27` (native AVX2; criterion (3) compares against a portable-SIMD native
build, still unverified as configurable). Run the measurement **in the task, in the foreground, as the last
action** — the handoff before this one reported a state no artifact held because it changed a load-bearing
file after its last run.

**Carried forward from this task, both disclosed rather than found:** the wasm module was linked from the
pre-cleanup text of `Scheduler.cpp` (behaviourally identical, and the cleaned text is what both native
builds compiled), so step 2 above re-links from the current text anyway; and `init_launched`/
`init_in_flight` are plain unsynchronised `bool`s, an unfenced read the diagnosis leans on — immaterial
here, because the conclusion is that nothing ran on any thread, which no race can manufacture. Also: only
the `concurrency` tag ran under wasm, not `--engine-test-all`, now cheap via `--run-args`.

**And the parity-gate note, restated because it is still written down nowhere else:** the wasm run carries
`--no-engine-gpu` and drops `gpu::context::data`, which a native `scene_reset_cpu` does not. The native
side of criteria (3)/(4) must pass `--no-engine-gpu` too, or the gate measures the SIMD arm *and* the
dropped GPU context and attributes both to SIMD.

**Carried forward unchanged:** `Os/Web/ImageLoader.cpp` asserts and `GpuRecord/Web/ShaderCompile.cpp`
returns empty, neither reached; `__wasm_call_dtors` / the stdout flush at exit is unexercised, so a run
that exits 0 with no hash line should suspect it first; `Time/Web/SystemClock.cpp`'s
`from_query_performance_counter` just returns `now()`; the `strtold` link warning and the
`libcall`/`__powidf2` probe regression are both pre-existing and untouched; `fd_readdir` omits `.` and
`..`; `Os/Process.cppm:122,125` still call `path::wstring()`; `Diag/AllocTrackGlobal.cpp`,
`Dx12/AgilitySdk.cpp` and `Sandbox/Main.cpp` declare no module so no link enumerates them; seven patch
boundaries have no baseline snapshot; `"web"` is hardcoded as the process-id answer in three modules; the
CMake drop path has no coverage beyond its keep-both Windows arm; `gse.vulkan -> gse.aftermath` is
reserved by path; `Nvidia/Backend.cpp` still has had no compiler over it; nothing is committed — the
owner's call.

### 2026-10-21 — steering: the init-wait instrumentation judged (holds, 4-of-6 confirmed); the deque and the cross-thread channels are now *measured* green under wasm, so the fault is per-helper delivery, and a new abort site surfaced for free

**Judged against the pass criteria: still nothing to pass.** (1)-(6) unstarted — no hash line, no p50
row, no exit 0. The graded line is met at **good**: a new named diagnosis, no fix, by the scope's own
design.

**Confirmed from the artifacts, not the report.** `touched.txt` is `Ecs/Scheduler.cpp` alone.
`/tmp/run_scenario_waitsize.log` carries exactly one `init wait: starting 6 task(s)` (line 1693),
`start() returned for 6 task(s) (done_flag=false wrapper_done=false)` (1694), and then three dump
rounds at `elapsed=5001.7 / 10015.8 / 15017.5 ms` naming the *same two* nodes each time —
`gse::physics::shadow_step::data` and `gse::stateless_system<^^sandbox::client_system>`, both
`launched=false in_flight=false`. Re-derived at source: `advance_inits` (`Scheduler.cpp:879-884`)
skips `init_done || init_in_flight`, so all six were incomplete at collection, and
`advance_one_init_system` (`:1327`) sets `init_in_flight = true` as its **first statement** — so
`launched=false in_flight=false` means those two coroutine bodies were never resumed at all, while
four reached `init_done`. **The 4-of-6 conclusion holds, and so does the refutation of both predicted
causes.** Declining patch 0020 and the `.s` read was right: a `call_indirect` type mismatch traps
deterministically and cannot produce 4-of-6. Verdict: correct, keep.

**This run spent its time narrowing the surviving hypothesis instead of scoping blind, and two of
three candidates are now dead — measured, not argued.** `scenario.wasm` is already linked and carries
`gse.tests.concurrency`, so the engine's own tests were run through `--run-args`-style env with **no
relink** (~2 s each, node directly on the existing module):
- `work_stealing_pop_and_steal_never_duplicate` **passes** — 20,000 items, 3 stealer threads, and the
  assertion is total accountability (`all.size() == count`, no duplicates, 0..count-1 complete). So
  the Chase-Lev deque does not *lose* entries under wasm atomics. Caveat recorded honestly: it
  completed in ~3 ms, which is consistent with the main thread draining before the stealers wound up,
  so it is strong evidence against loss and weak evidence about steal throughput.
- `spsc_preserves_order`, `mpsc_push_publishes_a_complete_value` and
  `mpsc_rejects_a_full_buffer` all **pass**, so cross-thread publication and spawn/join work.
This retires the ledger's long-carried "`thread_local`-address thread ids unproven past `-S`" as the
leading suspect and also retires "a job is stranded behind parked workers": `try_pop_local`
(`Task.cppm:679`) drains `remote_entries` as well as the deque, `try_pop_or_steal` (`:717`) sweeps
every queue, and the main thread calls it each loop iteration.

**So the surviving statement is narrow: for two of the six helpers, the posted job never executed its
`fn`, or executed it without resuming the child.** `when_all`'s `await_suspend` posts `helpers[1..]`
via `post_range` (`Task.cppm:552`) and returns `helpers[0]` for symmetric transfer, so inline-1 +
3-of-5-posted = the four that completed. Nothing in `post_range` can silently drop a job — a null
`fn` logs, and the report measured zero such lines — which leaves the per-job path between
`push_to_queue` and `run_job` as the only unmeasured span. That span has **no counter anyone prints**:
`in_flight` is suspected off by two and nothing logs it.

**New, found while probing, and recorded because nothing else records it:** `--engine-test-all` under
wasm **aborts** at `gse::process::temporary_path` →
`std::filesystem::temp_directory_path()` → `__cxa_throw` → `abort` (V8 stack captured). A sixth
throwing-`std::filesystem` call, in a module this port has already armed for the web; WASI has no
temp directory and nothing sets `TMPDIR`. It is not on `scene_reset_cpu`'s path, so it does not gate
the aim, and it is the same class as `AssetCatalog`/`AssetSystem`: the `error_code` overload, or an
honest unsupported report. Left unfixed deliberately.

**Next task: instrument the job path, name the two helpers, then fix if the measurement names a fix.**
All in charter (`gse.concurrency`, `gse.ecs`, both named; `Tools/` authorized), all behind the
existing `--engine-init-stall-dump` gate or an equivalent so the native default stays dormant:
1. **Per-helper identity.** One gated log in `when_all_helper`/`await_suspend` carrying the helper
   index — on entry to the helper body and immediately before `co_await child`. Six "posted #i",
   four "entered #i" names the missing pair and settles whether `helpers[0]` is among them.
2. **Per-job accounting.** A gated count of jobs pushed vs `run_job`-entered vs completed, plus
   `in_flight` and each queue's `approximate_size()`/`remote_entries.size()` printed at every stall
   dump. If pushed == 6 and entered == 4 with every queue empty, a job was claimed and dropped
   between `try_pop_*` and `run_job`; if two queues still hold an entry, the sweep is not sweeping
   what it thinks it is. Those are different bugs with different fixes, and today nothing
   distinguishes them.
3. **Fix at the layer the measurement names.** Escalate rather than guess if the only fix would change
   native behaviour or delete a native path.

Judged on: the missing pair named by helper index from an artifact, with the artifact cited, and the
pushed/entered/queue-contents triple printed. **Best** remains `world-state hash 0x<16 hex>` plus the
p50 row and exit 0 — and do **not** expect `0xe836d31697879e27` (native AVX2; criterion (3) compares
against a portable-SIMD native build, still unverified as configurable). Native build green and the
hash reprinted with the gate both off and on, as the last task did.

**Cheap-measurement rule, now demonstrated rather than asserted:** the linked `scenario.wasm` can be
re-run under node in ~2 s with different argv via the harness env, against a ~25-minute relink. Any
question answerable by argv alone should be answered that way first — this run answered three.

**Carried forward unchanged:** the parity gate's native side must pass `--no-engine-gpu`, or it
attributes the dropped GPU context to SIMD; `Os/Web/ImageLoader.cpp` asserts and
`GpuRecord/Web/ShaderCompile.cpp` returns empty, neither reached; `__wasm_call_dtors`/the stdout
flush at exit is unexercised, so a run that exits 0 with no hash line should suspect it first;
`Time/Web/SystemClock.cpp`'s `from_query_performance_counter` returns `now()`; the `strtold` link
warning and the `libcall`/`__powidf2` probe regression are pre-existing and untouched; `fd_readdir`
omits `.` and `..`; `Os/Process.cppm:122,125` still call `path::wstring()`;
`Diag/AllocTrackGlobal.cpp`, `Dx12/AgilitySdk.cpp` and `Sandbox/Main.cpp` declare no module so no
link enumerates them; seven patch boundaries have no baseline snapshot; `"web"` is hardcoded as the
process-id answer in three modules; the CMake drop path has no coverage beyond its keep-both Windows
arm; `gse.vulkan -> gse.aftermath` is reserved by path; `Nvidia/Backend.cpp` still has had no
compiler over it; nothing is committed — the owner's call.

### 2026-10-21 — steering: the job census judged (holds); and the artifact carries a lead the report did not read — **not one non-main thread has logged a single line in the entire wasm run**

**Judged against the pass criteria: still nothing to pass.** (1)-(6) unstarted — no hash line, no p50
row, no exit 0. The graded line is met at its middle tier: a measurement that names a region, no fix,
by the scope's own design.

**Confirmed from the artifacts, not the report.** `touched.txt` is three files
(`Concurrency/Task.cppm`, `Concurrency/AsyncTask.cpp`, `Ecs/Scheduler.cpp`).
`/tmp/run_scenario_helperids.log` carries one `init wait: starting 6 task(s)` (line 1693), `posted
helper #1..#5` plus `symmetric transfer to helper #0 (empty=false)`, `job #1..#4 entered` each with
`resume returned (resumed=true)`, **no `job #5` line of any kind**, and three dumps at 5/10/15 s all
reading `pushed=5 claimed=5 entered=5 completed=4 in_flight=1 queues[0:local=0 remote=0 1:local=0
remote=0]`. Both review fixes are in the tree as described: `claimed(job_entry&&)`
(`Task.cppm:745`) performs the `std::optional` construction *and* the increment, and all four success
returns of `try_pop_local`/`try_steal_from` go through it with no increment left in
`try_pop_or_steal`; `set_job_diagnostics(true)` (`:698-706`) zeroes all four counters with relaxed
stores before the release store of the flag. Verdict: correct, keep — and hypothesis (A), a job lost
between the queue and `run_job`, which three handoffs carried, is dead.

**The new lead, read off that same log and off a native one beside it.** Every one of the **92** log
lines in the whole wasm run is tagged `main`. Natively, the same gate-on `scene_reset_cpu` runs print
freely from workers (`Sandbox.18228...`: `worker-1` x2, `worker-2` x1; `Sandbox.18820...`: `worker-1`
x25 across five worker indices). And the tag is not noise: `log::thread_display()`
(`Import/Log.cpp:208-216`) is pure `thread_local` — an unnamed thread reads
`thread_role::unknown` and prints `T<16 hex>`, never `main`. So the asymmetry is real and it sits
exactly where the stuck job is: jobs #1-#4 all ran on `main` (the spinning `try_run_one`), main is
demonstrably alive through three dumps, so the fifth `run_job` is on a **worker** — and no worker has
produced a line of output in this port, ever.

**Two readings, mutually exclusive, both load-bearing, and the log cannot separate them:**
- **(A) no worker has ever reached a `log::println`** — i.e. the worker hangs at or before its first
  log call, which is also why the gated `job #5 entered` probe is silent;
- **(B) `thread_local` storage is not per-thread under wasm** — a worker reading main's TLS would
  print `main` rather than `T...`. That would reach far past logging: `t_is_main_thread`,
  `t_worker_index` (queue selection), `gse::trace`'s TLS (the very `_ZTH...diag3tlsE` wrapper patch
  0018 exists for) and `concurrentqueue`'s `thread_local`-address thread ids — the item this ledger
  has carried as "unproven past `-S`" since step 1, which would finally be proven *wrong*.

**A scoping constraint that follows, and it is the reason the next task must not be probed the
obvious way:** under reading (A), **every `log::println`-based probe inside that region is blind** —
the probe is the hang. The next measurement has to be breadcrumbs a worker writes without logging
(atomics) and the **main** thread prints at the stall dump, which is the one thread known to reach
stdout.

**Next task: separate (A) from (B), then isolate the statement.** All in charter — `gse.concurrency`,
`gse.diag` and `gse.ecs` are charter-named and the gate already exists:
1. **Settle TLS first, because it is cheap and it reframes everything else.** At worker entry
   (`worker_loop`, `Task.cppm:971`), have each worker store into a per-index atomic slot: its
   `t_is_main_thread`, its `t_worker_index`, and `std::bit_cast<std::uint64_t>(&t_worker_index)` —
   the address, which is the direct test. Print the table from `log_job_census` on the main thread.
   **Distinct addresses and `is_main=false` means (B) is dead** and TLS is per-thread; **any two
   workers sharing an address, or a worker reporting `is_main=true`, is the bug**, and it is a
   toolchain finding (a patch 0020 candidate, in charter) rather than an engine one.
2. **Breadcrumb `run_job`'s prologue with atomics, not logs.** A gated per-stage counter or a single
   `last_stage` atomic bumped at each of: entry, after `make_scope_exit`, after the
   `entry.async_trace`/`end_async` branch, after the null-`fn` check, after the `trace::scope_guard`
   constructor, and immediately before `entry.fn()`. Main prints it at each dump. That names the
   statement outright, and it works under either reading.
3. **Fix at the layer the measurement names.** Escalate rather than guess if the only fix would
   change native behaviour or delete a native path.

Also worth two minutes in that task, and recorded because it was *asserted* rather than shown: the
previous handoff eliminated trace via `--no-engine-trace`, but nothing establishes that the flag
makes `trace::end_async` and `trace::scope_guard`'s constructor early-return rather than merely
suppressing output. Read those two bodies; if the flag only gates emission, trace is **not**
eliminated and step 2's breadcrumb is what settles it.

Judged on: (A) vs (B) decided from an artifact, with the artifact cited, and the prologue stage named.
**Best** remains `world-state hash 0x<16 hex>` plus the p50 row and exit 0 — and do **not** expect
`0xe836d31697879e27` (native AVX2; criterion (3) compares against a portable-SIMD native build, still
unverified as configurable). Native build green and the hash reprinted with the gate both off and on.
Run the measurement **in the task, in the foreground, as the last action**; node on the already-linked
module is ~2 s against ~25 minutes for a relink, so answer by argv first wherever argv can answer.

**Carried forward from this task:** `completed` can exceed a round's own `entered` by a few in a
multi-round native wait (a worker still in `run_job`'s scope-exit when the next round arms the gate) —
cannot under-count, cannot affect the pushed/claimed/entered comparison, single-round under wasm;
helper index to system is still unmapped, which is why one stuck job sits against two unlaunched
systems (`gse::physics::shadow_step::data`, `gse::stateless_system<^^sandbox::client_system>`);
`entered`/`completed` count io and background lane jobs while `pushed`/`claimed` cover only the worker
queues, so the triple is not a balanced ledger outside an init wait; the wasm triple has not been
re-measured since the two counter fixes.

**Carried forward unchanged:** the parity gate's native side must pass `--no-engine-gpu`, or it
attributes the dropped GPU context to SIMD; `--engine-test-all` under wasm aborts at
`process::temporary_path` then `std::filesystem::temp_directory_path()`, a sixth
throwing-`filesystem` call, off this goal's path; `Os/Web/ImageLoader.cpp` asserts and
`GpuRecord/Web/ShaderCompile.cpp` returns empty, neither reached; `__wasm_call_dtors` and the stdout
flush at exit are unexercised, so a run that exits 0 with no hash line should suspect them first;
`Time/Web/SystemClock.cpp`'s `from_query_performance_counter` returns `now()`; the `strtold` link
warning and the `libcall`/`__powidf2` probe regression are pre-existing and untouched; `fd_readdir`
omits the dot and dot-dot entries; `Os/Process.cppm:122,125` still call `path::wstring()`;
`Diag/AllocTrackGlobal.cpp`, `Dx12/AgilitySdk.cpp` and `Sandbox/Main.cpp` declare no module so no
link enumerates them; seven patch boundaries have no baseline snapshot; `"web"` is hardcoded as the
process-id answer in three modules; the CMake drop path has no coverage beyond its keep-both Windows
arm; `gse.vulkan -> gse.aftermath` is reserved by path; `Nvidia/Backend.cpp` still has had no
compiler over it; nothing is committed — the owner's call.

### 2026-10-21 — steering: **the engine ticks 240 frames off Windows and prints a stable hash**; the week-long "worker stall" was an unbound WASI in the harness. Criteria still need a browser, and this chat is out of steering runs

**Judged against the pass criteria: (1)-(6) remain unmet as written, and every one of them is unmet for the
same single reason — this ran under node, not in a browser page.** No criterion is *failed*; what each was
waiting on, an engine that ticks a named headless scenario to its own exit outside Windows, now exists.

**Confirmed from the artifacts, not the report.** `/tmp/apply_fix2.log` (diagnostics gate off) carries
`bench: fixed-step clock engaged, 0 warmup frames then 240 measured frames`, seven `thread 2..8 spawned`
and all seven `returned`, `worker-0` log lines mid-run (so non-main threads reach stdout — the asymmetry
the previous entry built two hypotheses on is gone), `scenario: scene reset rebuilt 79 bodies from 79`,
then `bench: 240 frames measured, p50 188.417 ms p95 300.975 ms p99 377.843 ms, world-state hash
0x2aeb133bbb68d214`, `bench: PASSED` and the harness's own `ok 0`. `/tmp/apply_fix1.log` is the second run:
**same hash**, p50 190.193 ms, `ok 0`. `/tmp/apply_1a.log:42-44` carries the cause verbatim —
`thread 1 trapped: wasi.start() has not been called`, V8 frames `__wasi_clock_time_get` → `gettimeofday`
→ `steady_clock::now`. `touched.txt` is `Tools/build_gcc_wasm.py` alone, so no engine C++ changed and the
native hash cannot have moved. Verdict: correct, keep — the **best** tier of the graded line is met for the
first time in this goal.

**The diagnosis retires four handoffs' worth of hypotheses, and the lesson is bigger than the bug.** The
harness constructed each worker's `node:wasi` object and never called `initialize` on it; node throws on
every syscall until it is bound. The first WASI call any worker made — a clock read inside
`trace::end_async`, two statements into `run_job` — threw, killed the worker mid-job, and left the main
thread spinning on a job that could never complete. So **(A) a job lost between queue and `run_job`, (B)
TLS not being per-thread, and the standing "`thread_local`-address thread ids unproven past `-S`" item
are all dead** — the ids were fine, the deque was fine, the ECS scheduler was fine, and every engine-side
measurement this ledger commissioned since 2026-10-20 18:49 was measuring a harness defect. The reason it
survived three investigations is recorded by the fix itself: a worker's `console.log` and a Worker `error`
event both need the **parent's event loop**, which a synchronously spinning main thread never returns to,
so the trap was structurally invisible. `writeSync(1, …)` is the only channel that works, and the worker
spawn/return/trap lines are now unconditional rather than gated — correct, and cheap at three writes.
**General rule for this port: when the engine's own instrumentation says "nothing ran on any thread",
suspect the host before instrumenting the guest further.**

**Two corrections to this record, from the task and endorsed:** `--no-engine-trace` is applied *after*
`initialize`, so the 2026-10-20 entry's claim to have eliminated trace from the init-phase stall was
measuring nothing — and trace was in fact on the failing path, as the trap stack shows. Also
`ids[0] = 1`: a zero-filled shared thread-id buffer was handing the first spawned thread the initial
thread's own id, duplicating libc per-thread state.

**On the hash: `0x2aeb133bbb68d214` is the wasm number and is not expected to equal
`0xe836d31697879e27`.** That is native AVX2; criterion (3) compares against a *portable-SIMD* native build,
and the record's long-standing caveat stands — **it is still unverified that `GSE_SIMD_X86` can be turned
off in a native build without editing the engine.** Settling that is now the single largest risk to the
`require` level of (3), and it is cheap to check. Carried with it, and still written down nowhere else:
**the native side of (3)/(4) must pass `--no-engine-gpu`**, or the gate attributes the dropped
`gpu::context::data` to the SIMD arm.

**Escalating, because this chat's steering allowance is spent (0 runs left) and the remaining work is a
coherent, in-charter block that wants the owner's go-ahead rather than a guess.** What is left, in order:
1. **The browser page** — criteria (1)/(2) name one. `Tools/`'s harness is charter-authorized ("a new
   browser/node harness beside them"), `--import-memory`/`--export-memory` and the Worker-backed
   `wasi.thread-spawn` carry over, and the `writeSync(1)` discipline becomes a page-side equivalent. This
   is the aim's last structural step.
2. **The parity gate and the report artifact** — criteria (3)-(6): a portable-SIMD native build (verify
   `GSE_SIMD_X86` is configurable *first*; if it is not, (3) drops to `report` as the record already
   provides for), `--no-engine-gpu` on both sides, `--engine-bench-state-dump-out` through
   `parity_gate.py` at `0.002`, three consecutive runs, hash stable across page reloads, and the report
   written under `Tools/` output.
3. **Hygiene the owner may want folded in or dropped:** the per-helper and job-census instrumentation from
   the last three tasks is now obsolete machinery with no remaining question —
   `--engine-init-stall-dump` reads as a keeper, the rest does not. It is prior tasks' uncommitted work,
   so removing it is the owner's call. And **nothing in this goal has ever been committed** — patches
   0018/0019, the toolchain README, `baseline-0017/`, `.gitattributes`, both documents, every `*/Web/`
   arm, the three `Engine/cmake/` data files and this Tools change — which is a large uncommitted surface
   to be carrying into a browser bring-up.

**New and unexamined:** an `update`-phase stall now appears during the clean run —
`gse::physics::data` stuck inside `run()`, watchdog firing at ~0.6-1.8 s, ~190 ms per frame against a
native budget orders of magnitude smaller. The run completes and the hash is stable across two runs, so it
reads as slowness rather than a hang, and **no pass criterion mentions wall-clock time** — (2) asks only
for the p50 row, not a p50 value. Recorded so it is not mistaken for a regression later; a 240-frame run
at ~190 ms/frame is ~45 s, which a browser page will tolerate.

**Carried forward unchanged:** `Os/Web/ImageLoader.cpp` asserts and `GpuRecord/Web/ShaderCompile.cpp`
returns empty, neither reached; `__wasm_call_dtors` and the stdout flush at exit are now *exercised* — the
hash line and `=== Log ended ===` both appear before `ok 0`, so that open item is closed;
`Time/Web/SystemClock.cpp`'s `from_query_performance_counter` returns `now()`;
`--engine-test-all` under wasm aborts at `process::temporary_path` → `std::filesystem::temp_directory_path()`,
off this goal's path; the `strtold` link warning and the `libcall`/`__powidf2` probe regression are
pre-existing; `fd_readdir` omits the dot and dot-dot entries; `Os/Process.cppm:122,125` still call
`path::wstring()`; `Diag/AllocTrackGlobal.cpp`, `Dx12/AgilitySdk.cpp` and `Sandbox/Main.cpp` declare no
module so no link enumerates them; seven patch boundaries have no baseline snapshot; `"web"` is hardcoded
as the process-id answer in three modules; the CMake drop path has no coverage beyond its keep-both
Windows arm; `gse.vulkan -> gse.aftermath` is reserved by path; `Nvidia/Backend.cpp` still has had no
compiler over it.

### 2026-10-22 — the owner resolved the escalation: remove the obsolete instrumentation, next steps agreed, **commit authorized (no push)**

**Decisions, recorded so no later run re-asks.** The owner agreed the next steps as escalated — the browser
page, then the parity gate and report artifact — directed that the redundant code come out, and
**authorized committing this goal's work, explicitly without pushing**, noting API tweaking still to come
at the end. That lifts, for this work, the charter's "any commit or push is the owner's call" on the commit
half only; **push remains reserved** and nothing here changes that.

**The removal has a precise referent, located rather than guessed, with a keeper sitting next to it.**
Obsolete — commissioned to find a stalled job, and the answer turned out to be an unbound WASI in the
harness, so there is no remaining question for it to answer:
- `Concurrency/Task.cppm` — `set_job_diagnostics`/`job_diagnostics`/`log_job_census` (declarations
  `:121-127`, state `:276`, bodies `:698-734`) and the counter sites at `:746`, `:817`, `:897`, including
  the `pushed`/`claimed`/`entered`/`completed` counters and `claimed(job_entry&&)` itself;
- `Concurrency/AsyncTask.cpp` — the per-helper logs at `:186-246` and `:354` (`posted helper #`,
  `job # entered`, `job # resume returned`, `symmetric transfer to helper #0`);
- `Ecs/Scheduler.cpp` — the three call sites at `:1401`, `:1405` and `:1614-1615`.

**Keeper, and it must survive the sweep:** `--engine-init-stall-dump` — `scheduler::set_init_stall_dump`
and `m_init_stall_dump` (`Scheduler.cpp:1397,1491`, `Scheduler.cppm:152,234`), `log_stall_state`
(`Scheduler.cpp:1495`, `Scheduler.cppm:208`) and `engine_config::init_stall_dump`
(`Engine.cppm:92`, wired at `Engine.cpp:236`). It is a real diagnostic, dormant at its native default, and
the new `update`-phase `gse::physics::data` stall is exactly what it exists to name. The two scaffolding
lines *inside* its gate — `init wait: starting N task(s)` (`:1420`) and `start() returned …` (`:1432`) —
were cut for one specific diagnosis and read as removable; that is the task's call to make and to report,
not mine to assume.

**Also not redundant, stated because a cleanup pass would otherwise strip it:** the worker
spawn/return/trap lines in `Tools/build_gcc_wasm.py`. The previous task made them unconditional on
purpose, and this ledger's own history vindicates that — thread lifetime being unobservable is what made
one harness bug cost four handoffs. They stay.

**Next task: the removal, proved on both arms, then the commit.** In that order, so the commit contains
verified-green code rather than a cleanup nobody has run:
1. Remove the obsolete set above, keeping the init-stall dump intact. Establish that nothing outside the
   census reads the counters before deleting them — `claimed()` performs the `std::optional` construction
   as well as the increment, so it is a refactor at four return sites, not a line deletion.
2. **Native build green and `0xe836d31697879e27` unmoved** — three of the touched files are spine
   concurrency and scheduler code, so this is a real check, not a formality.
3. **Relink and re-run the wasm scenario**, and confirm the hash is still `0x2aeb133bbb68d214` with
   `ok 0`. ~25 minutes for the relink, and worth it: it is the proof the removal is behaviour-neutral on
   the arm that matters, and it re-measures the goal's headline result. Run it **in the task, in the
   foreground, as the last action before the commit** — this ledger records what happens otherwise.
4. **Commit, do not push.** One commit of this goal's whole surface: patches 0018/0019 and the toolchain
   README and `baseline-0017/`, `.gitattributes`, `docs/wasm_pipeline_plan.md` and this record, every
   `*/Web/` and `*/Native/` arm, the three `Engine/cmake/` data files, the `Tools/` work and the engine
   edits. **No Claude or AI attribution in the message** — the owner's standing rule, which overrides the
   harness's default attribution guidance.

**Then, as agreed:** the browser page (criteria (1)/(2)), bundling the `GSE_SIMD_X86`-configurability
check, since whether a portable-SIMD native build is reachable without editing the engine is what decides
if criterion (3) is `require` or `report`; then the parity gate with `--no-engine-gpu` on **both** sides,
three consecutive runs, reload stability, and the report artifact under `Tools/` output.
