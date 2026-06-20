# Logger Improvement Backlog (ranked)

Ranked list of features the current logger lacks relative to production loggers
(spdlog, quill, glog, Boost.Log), tailored to this engine. Pick the top unstarted
item, implement, check it off. Each entry is self-contained so a cold session can
act on it without re-deriving context.

Source: [Log.cppm](../Engine/Engine/Import/Log.cppm) (public API + templates),
[Log.cpp](../Engine/Engine/Import/Log.cpp) (`logger` impl).
Used at ~300 call sites across ~68 files, so **additive, backward-compatible
changes are strongly preferred** over anything that touches the call API.

Date drafted: 2026-06-19.

## Status — branch `logger-improvements` (2026-06-20)

- **#1 Runtime level filtering** — done. `set_level` (global + per-category),
  `level_of`, `enabled` in `gse.log`. Default stays `debug` (nothing filtered)
  until you raise it; setting the global threshold broadcasts to all categories.
  Gate sits in the two leaf `println` templates, so `make_format_args` is skipped
  when disabled.
- **#2 Throttle / every-N** — partial. `log::sampler` (every-N + every-T,
  lock-free, caller-owned `static`) done. Automatic "repeated N×" dedup still
  deferred — it changes global logging semantics, so it wants its own opt-in.
- **#4 Named threads** — done. Reflected `thread_role` enum (`unknown, main,
  worker`) rendered via the same `std::formatter` / `enum_to_string` path as
  `level` / `category`; `name_thread(role)` + `name_thread(role, index)`.
  `write_line` shows `main` / `worker-3`, falls back to the hash when `unknown`.
  Wired at `task::pool_start` (main) and the worker lambda. Kept independent of
  the `trace` registry on purpose (no lock on the log hot path, no `gse.diag`
  dependency in the universal logger). Dedicated watchdog / capture / network
  threads exist too — one line each to name now that roles are cheap to add.
  Task-origin labeling for the pool (the source_location idea) is the deliberate
  follow-up — see #12.
- **#5 Sink abstraction** — done. `write_line` fans each `record` out to a list
  of `sink`s, each with its own `min_level` (a second filter under the global /
  per-category one). Exports `sink` (write / write_raw / flush), `record`,
  `format_line`, `add_sink(unique_ptr<sink>) -> sink*`; `console_sink` +
  `file_sink` are the internal auto-registered defaults. `msvc_debug_sink`
  deferred (Win32 `OutputDebugStringA` — keep the universal logger portable for
  the gcc-trunk CI; trivial `#ifdef _WIN32` add later). Formatting centralized in
  `format_line`; the message is now built once as a string — the hook #2-dedup
  and #6 ring-buffer both need.
- **#3 Async logging** — done, opt-in, lock-free hot path. `set_async(true)` starts
  one background thread that drains the queue and performs all sink writes; producers
  only enqueue a pre-formatted `queued_record` and never touch I/O. Queue is
  `moodycamel::ConcurrentQueue` + a `std::counting_semaphore` for consumer blocking
  (only `concurrentqueue.h` is vendored, so the blocking wait is hand-rolled). The
  hot path is lock-free (enqueue + `release`); `m_sink_mutex` only guards the actual
  writes (consumer-only) and `add_sink`; flush coordination uses a separate
  `m_flush_mutex` off the hot path. `flush()` posts a flush control message and waits
  a token barrier; a `terminate` message + drain handles stop. Default stays
  synchronous. Trade-offs vs a locked queue: the on/off toggle isn't fully race-free
  (no lock pairs the `m_async` flip with enqueues — toggle at quiescent points;
  stragglers are caught by the stop/dtor drain), and `flush()` is a strong *practical*
  barrier rather than a strict global one (moodycamel has no cross-producer FIFO).
  Uncommitted pending a build/test.
- **Architecture (interface slimming):** `logger` / `queued_record` / `instance()`
  moved out of the interface into `Log.cpp` behind a non-template `write_line` seam,
  so the ~68 importers no longer parse the logger's internals (build-time win) and
  moodycamel stays fully contained in the `.cpp` — no PIMPL needed.
- **#6 Ring-buffer backtrace** — done. `enable_backtrace(n)` keeps the last N
  *sub-threshold* records (the ones the filter would normally drop) in an in-memory
  ring; on a passing `error`, the ring is force-dumped to all sinks (bracketed,
  bypassing per-sink levels) right before the error, then cleared. `dump_backtrace()`
  and `disable_backtrace()` for manual control. The template gate passes sub-threshold
  records through only when backtrace is active; `write_line` / `run` funnel through a
  new `process()` (ring the `!pass` records, dispatch the `pass` ones). Uncommitted,
  unbuilt.
- **#9 `fatal` level + Assert** — done. `fatal` is the new top level; `write_line`
  handles it specially: flush (drains the async queue), dump the backtrace ring,
  force-write the fatal line to every sink, flush, `std::terminate()`. A fatal log
  gives full recent context on the way down. `Assert` routes `assert_fail` through
  `log::println(fatal, …)` (now `[[noreturn]]`), inheriting the ring dump for free.
  Uncommitted.
- **#7 Log rotation** — done. `file_sink` rotates on open (keeps the last N runs:
  `log.txt` + `log.1.txt`..`log.{N-1}.txt`, default N=5 via `log_files_kept`) instead
  of truncating each run, so prior runs survive for post-crash diagnosis. Per-run
  rotation; size-based rolling within a single long run is a separate follow-up.
  Uncommitted.
- **#10 Color output** — done. `console_sink` wraps each line in an ANSI SGR color
  by level; the color is an **annotation on the `level` enum** (`[[= ansi_sgr{N} ]]`)
  read back via reflection (`level_sgr` → `first_annotation_of_type` + `constant_of`),
  not a hand-written switch. `set_color(bool)` toggles it (default on). The SGR code is
  an `int` (structural) because `std::string_view` can't be an annotation value.
- **#13 Static-destruction safety** — done. The logger is a plain global now, so a log
  during static teardown would hit a destroyed object. A trivially-destructed
  `logger_alive` atomic (true at end of ctor, false at start of dtor) guards the free
  `write_line`/`flush`; when dead, `write_line` falls back to `fputs(stderr)`. Also
  covers use-before-construction.
- **#2 Auto-dedup** — done. `dispatch` collapses consecutive identical records (same
  level/category/message) into "(previous message repeated N times)", flushed when a
  different record arrives or on shutdown. The ring dump and fatal force-write bypass it.
- **Next ranked:** #8 compile-time stripping, #11 pattern/JSON sink, #12 scoped
  context (task-origin labeling).

Everything below is the original plan, unchanged.

## Current baseline (do not regress)

- `std::format`-typed API with an overload ladder: `(fmt)`, `(level, fmt)`,
  `(level, loc, fmt)`, `(category, fmt)`, `(level, category, fmt)`,
  `(level, category, loc, fmt)`.
- Levels: `debug, info, warning, error`. Categories: `general, runtime, render,
  network, vulkan, vulkan_validation, vulkan_memory, assets, task, save_system,
  physics`.
- `level`/`category` stringify via the p2996 reflection formatters (Meta/Enum,
  Meta/Format) — anything new added to those enums prints for free.
- Thread-safe: single `std::mutex` in `logger`, held across formatting + I/O.
- Fan-out to console (`cout`, or `cerr` when flushing) and a file.
- UTC timestamps with ms precision; per-line thread tag; opt-in `source_location`.
- Flush policy: flush on `error` only (`should_flush`, [Log.cpp:38](../Engine/Engine/Import/Log.cpp)).
- File opened with `trunc` ([Log.cpp:45](../Engine/Engine/Import/Log.cpp)) — wipes prior run.
- Singleton via function-local `static logger` (`instance()`, [Log.cpp:101](../Engine/Engine/Import/Log.cpp)).

## Ranking at a glance

| # | Feature | Impact | Effort | Back-compat | Depends on |
|---|---------|--------|--------|-------------|------------|
| 1 | Runtime level filtering (global + per-category) | High | Low | Yes | — |
| 2 | Throttle / dedup / every-N | High | Low–Med | Yes | — |
| 3 | Async / background-thread logging | High | Med–High | Yes (API unchanged) | benefits from #5 |
| 4 | Named threads (vs hashed id) | Med–High | Low | Yes | thread registry |
| 5 | Sink abstraction / multiple sinks | Med (High as enabler) | Med | Yes | — |
| 6 | Ring-buffer backtrace (dump-on-error) | Med–High | Med | Yes | #5 (light) |
| 7 | Log rotation / retention | Med | Med | Yes | #5 (light) |
| 8 | Compile-time stripping in release | Med–High | Med + macro tension | New call form | — |
| 9 | `fatal` level + Assert integration | Med | Low–Med | Yes (additive) | Assert.cppm |
| 10 | Color console output by level | Low–Med | Low | Yes | #5 (light) |
| 11 | Configurable pattern + structured/JSON sink | Low–Med | Med | Yes | #5 |
| 12 | Scoped / thread-local context | Low–Med | Med | Yes (additive) | — |
| 13 | Static-destruction safety | Low (correctness) | Low | Yes | — |

## Sequencing notes

- **#1 and #2 are the quick, high-value wins** — both drop into `write_line`,
  both touch zero call sites. Do them first regardless of anything else.
- **#5 (sink abstraction) is an enabler.** #6, #7, #10, #11 all layer on it
  cleanly. If you intend to do two or more of those, pull #5 forward and build
  them on top instead of retrofitting. If you only ever want one, skip #5.
- **#4 (named threads) is an afternoon's work** and improves every single log
  line's readability — pull it earlier than its rank if you want momentum, or
  fold it into #3 since async output benefits most from readable thread labels.
- **#3 (async) is the one with real design risk** (ordering, flush-on-crash,
  shutdown drain). Don't start it on a tired evening.

---

## 1. Runtime level filtering (global + per-category)

**What.** A minimum-level threshold, checked before any formatting work, plus a
per-category override table.

**Why.** Biggest functional gap. `debug` exists but is never filtered — every
debug line formats and writes unconditionally. No way to say "vulkan at warning,
physics at debug." Cuts noise and the bulk of logging cost in one change.

**Effort.** Low. **Back-compat.** Full — purely additive.

**Sketch.** In `gse::log`, add:

```
auto set_level(level min) -> void;
auto set_level(category cat, level min) -> void;
auto enabled(level lvl, category cat) -> bool;
```

Back them with a `std::atomic<level>` global and a
`std::array<std::atomic<level>, category_count>` (default each to the global).
First line of `write_line`: `if (!enabled(lvl, cat)) return;`. For the cheapest
possible early-out, also gate inside the `println` templates so
`std::make_format_args` is skipped when disabled.

## 2. Throttle / dedup / every-N

**What.** glog-style rate limiting: `every_n`, `first_n`, `every_t`, plus
"last message repeated N×" collapsing for identical consecutive lines.

**Why.** A per-frame loop spams identical errors every frame; this is the single
biggest readability win for a real-time engine. Currently nothing prevents it.

**Effort.** Low–Med. **Back-compat.** Additive (new overloads or a small policy
arg/struct; leave existing calls untouched).

**Sketch.** Keyed by `source_location` (file+line) so each call site has its own
counter — store a `std::unordered_map<std::uintptr_t, count_state>` guarded by the
existing mutex, or a small fixed cache. For consecutive-dedup, hash the formatted
string and the (level, category); on match increment a counter and suppress, on
change flush the "repeated N×" summary then print the new line.

## 3. Async / background-thread logging

**What.** Calling thread enqueues a log record; a dedicated thread formats and
writes.

**Why.** Today everything — timestamp, two `vprint` calls, file I/O — runs on the
calling thread under one global mutex. Render and physics threads log per-frame
and serialize on that mutex + block on disk. Async removes I/O from the hot path.

**Effort.** Med–High (the risky one). **Back-compat.** Full — public API is
unchanged; this is an internal swap of `write_line`'s tail.

**Sketch.** [moodycamel concurrentqueue](../Engine/External/moodycamel/concurrentqueue.h)
is already vendored. Enqueue a record `{ level, category, timestamp, thread tag,
prefix, pre-formatted string }` — **format on the producer**, not the consumer, so
captured arguments don't have to outlive the call (avoids lifetime hazards with
`std::format_args`). Consumer thread drains and writes. Design carefully:
- **Flush-on-error / crash:** synchronous path or block-until-drained for `error`/
  `fatal`, so a crash doesn't lose the last lines.
- **Shutdown:** drain the queue in the consumer-join before `~logger` closes the
  file.
- **Overflow policy:** bounded queue with either block or drop-oldest + a dropped
  counter; decide explicitly.

## 4. Named threads (vs hashed id)

**What.** Replace `[T{:016x}]` (a `hash<thread::id>`, `current_thread_tag`,
[Log.cpp:34](../Engine/Engine/Import/Log.cpp)) with human names: `render`,
`physics`, `main`, `task-3`.

**Why.** The hashed tag is unreadable. The engine already has a task/frame
scheduler that owns its threads, so naming is natural and makes every line
scannable.

**Effort.** Low. **Back-compat.** Full.

**Sketch.** `thread_local std::string t_name;` + `auto name_thread(std::string)`.
Call it where worker threads start (FrameScheduler, task pool). Fall back to the
short hash when unnamed. Pad/truncate to a fixed width for column alignment.

## 5. Sink abstraction / multiple sinks

**What.** A `sink` interface; the logger fans a record out to a list of sinks,
each with its own level filter and (eventually) formatter.

**Why.** Output is hardcoded to one `ofstream` + cout/cerr in `write_line`
([Log.cpp:66](../Engine/Engine/Import/Log.cpp)). No way to add an in-game console
overlay, a `OutputDebugStringA` sink (logs appear in the VS debugger), or a
network sink. This is the **enabler** for #6, #7, #10, #11.

**Effort.** Med. **Back-compat.** Full (refactor behind the existing API).

**Sketch.**

```
struct sink {
    virtual ~sink() = default;
    virtual auto write(const record&) -> void = 0;
    virtual auto flush() -> void = 0;
    std::atomic<level> min_level;
};
```

Ship `console_sink`, `file_sink`, `msvc_debug_sink` initially. `logger` holds a
`std::vector<std::unique_ptr<sink>>`; `add_sink` / `remove_sink`. Move the current
console+file logic into the first two sinks.

## 6. Ring-buffer backtrace (dump-on-error)

**What.** Keep the last N `debug`-level records in an in-memory ring buffer,
normally discarded; on an `error`/`fatal`, flush the ring to the sinks first.

**Why.** Best feature spdlog has for shipping builds: no debug spam in normal
operation, but full context preserved when something actually breaks.

**Effort.** Med. **Back-compat.** Full (additive `enable_backtrace(n)` /
`dump_backtrace()`).

**Sketch.** Fixed-size ring of pre-formatted records guarded by the mutex (or its
own lock). On a triggering level, emit the ring in order then the triggering line.
Cleaner once #5 exists (dump targets the sink list).

## 7. Log rotation / retention

**What.** Stop wiping history; roll files by size or date and keep the last N.

**Why.** File opens with `std::ios::trunc` ([Log.cpp:45](../Engine/Engine/Import/Log.cpp)),
so every run destroys the previous log — bad for diagnosing a crash after the fact.

**Effort.** Med. **Back-compat.** Full (internal to the file sink).

**Sketch.** As a `rotating_file_sink` (rides on #5): on open, rename
`log.txt -> log.1.txt -> log.2.txt ...` up to N, or timestamp the filename per run.
Optional size cap that rolls mid-run.

## 8. Compile-time stripping in release

**What.** A path where sub-threshold `debug`/`trace` calls compile to nothing, so
arguments aren't even evaluated.

**Why.** Even with #1, `std::make_format_args(args...)` still evaluates every
argument on a filtered call. Truly zero-cost debug logging needs the call elided
at compile time.

**Effort.** Med, plus **aesthetic tension**: the only robust way is a macro
(`if constexpr` can skip the body but not argument evaluation at the call site).
Conflicts with the codebase's macro-averse style — decide whether the perf is
worth a `GSE_LOG_DEBUG(...)`-style entry point that compiles out below a
`compile_time_min_level` constant. **Lower priority because #1 already removes the
runtime cost for most cases.**

**Back-compat.** Introduces a new optional call form; existing calls keep working.

## 9. `fatal` level + Assert integration

**What.** A level above `error` that flushes everything, logs, then breaks/aborts.
Wire it into the existing [Assert.cppm](../Engine/Engine/Import/Assert.cppm).

**Why.** `error` is currently the ceiling; there's no log level that participates
in a hard stop, and asserts/log are separate paths.

**Effort.** Low–Med. **Back-compat.** Additive (new enum value — the reflection
formatters pick up the string automatically; audit any exhaustive `switch` on
`level`, though there are essentially none today).

**Sketch.** Add `fatal` to `level`; `should_flush` returns true for it; after
writing, `std::abort()` / `__debugbreak()` (debug). Have the assert macro route
its message through `log::println(level::fatal, ...)`.

## 10. Color console output by level

**What.** ANSI color per level on the console sink (red error, yellow warning,
etc.); plain text to the file.

**Why.** Cheap dev-experience win; console is currently uncolored plain text.

**Effort.** Low. **Back-compat.** Full.

**Sketch.** Color belongs in the console sink (#5). Enable VT processing on the
Windows console once at startup (`SetConsoleMode` +
`ENABLE_VIRTUAL_TERMINAL_PROCESSING`). Gate on `isatty` / a config flag so piped
output stays clean.

## 11. Configurable pattern + structured/JSON sink

**What.** A per-sink pattern string (e.g. `%t [%l][%c] %v`) instead of the
hardcoded layout, and an optional JSON-lines sink for machine parsing.

**Why.** Format is hardcoded in `write_line`. A JSON sink makes logs ingestible by
tooling. Lower near-term value for a solo engine, hence the rank.

**Effort.** Med. **Back-compat.** Full. **Depends on** #5.

## 12. Scoped / thread-local context

**What.** A scoped guard that attaches contextual fields (frame number, system
name, entity id) to every line emitted within its scope, instead of threading them
through each call.

**Why.** Removes repetitive context-passing and makes per-frame/per-system logs
correlatable.

**Effort.** Med. **Back-compat.** Additive.

**Sketch.** `thread_local` stack of small key/value frames; a RAII
`log::scope("frame", n)` pushes/pops; `write_line` appends the current stack to
the prefix. Composes with #4 (both are thread-local context).

## 13. Static-destruction safety

**What.** Guard against logging after the function-local `static logger`
(`instance()`, [Log.cpp:101](../Engine/Engine/Import/Log.cpp)) is destroyed.

**Why.** Any `log::println` from a static destructor running after `s_logger` is
torn down is UB. Low probability today, but a real footgun.

**Effort.** Low. **Back-compat.** Full.

**Sketch.** A `std::atomic<bool> g_alive` flag set false in `~logger`; `write_line`
falls back to a direct `cerr` write (or no-ops) when not alive. Or adopt a
leak-on-exit strategy (never destroy the singleton) and just flush at exit.
