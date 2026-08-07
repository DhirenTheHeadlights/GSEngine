# Boot — stop gating the loading screen on asset compilation

**Status: implemented 2026-08-04.** This document is now the record of what was done and why the
design changed from the first draft.

The loading screen waited for `compile_non_boot_critical()` even though nothing is annotated
`boot_critical` except `font`. A single malformed `.gsmdl` turned that wait into 55–60 s on every
launch. The overrun bug was fixed first (`source_reader` latch + `bake()` bounds checks); after
that, boot dropped to 3.6 s with `compile_non_boot_critical` taking 44 ms. This change removes the
structural hazard so no future asset can reintroduce the stall.

---

## What already existed

Almost none of the "per-asset readiness" machinery needed building.

**Loading is fully async and per-asset.** `resource_slot` carries
`unloaded / queued / loading / loaded / reloading / failed`. `asset::run` calls `loader->flush()`
every frame; `flush()` promotes every `queued` slot and launches `launch_load` as a coroutine on a
worker. `handle::valid()`, `state()` and `error()` are public and correct.

**Every asset already compiles itself on demand.** `register_loaders()` installs a pre-load hook on
every loadable type:

```
set_pre_load_fn<T>(*m_data, [](const std::filesystem::path& baked_path) {
    return recompile_if_stale<T>(baked_path);
});
```

`recompile_if_stale<T>` resolves the source from the baked path, checks `needs_recompile<T>`, and
bakes if stale — on the loading worker, immediately before `load()`. `compile_non_boot_critical()`
was therefore redundant for correctness.

**The eager pass was single-threaded.** `compile<T>()` iterates its catalog and calls
`bake_to_disk` inline; the `(try_one<Ts>(), ...)` fold runs types in sequence. Nothing about it was
parallel. The per-asset pre-load hook bakes each asset on its own worker, so removing the eager
pass is *faster*, not just simpler. This is why the first draft's "keep the eager pass, it
parallelises compilation" reasoning was wrong and was dropped.

---

## The audit — 91 `acquire()` / `resolve()` sites

Enumerated across Engine, Sandbox and Editor. Result: the guard idiom was already near-universal.

| Class | Count | Verdict |
|---|---|---|
| Semaphores, not resource handles (`Log.cpp`, `Task.cppm`) | 2 | false positives |
| `ctx.fonts.text` / `.code` in GUI + Editor draw code | 80 | safe by structural gate |
| Model / clip / texture sites | 8 | already guarded |
| `UiRenderer::add_text_quads` | 1 | **genuinely unguarded — fixed** |

**The structural gate for the 80 font sites:** `gui::init` is a `system_init` coroutine that does
not complete until both fonts and the blank texture are loaded. ECS `init` completes before `run`,
and every one of those 80 sites is in draw code reached from `run`. They are safe — but they are
safe *because of that gate*, which is worth knowing before anyone makes `gui::init` complete early.

**The 8 model sites** all open with `if (!handle.valid()) continue;` — `ClipPlayer` ×2,
`Ragdoll` (guarded at the top of the request loop, 19 lines above the `resolve()`),
`GeometryCollector` ×3, `SkinRenderer`, `RuntimeSpawns::spawn_character`. `GeometryCollector` and
`SkinRenderer` add a second `uploads_ready()` check for GPU residency.

**The one real gap:** `add_text_quads` called `cmd.font.resolve()` with no check, reached from the
draw loop for any non-sprite command. Latent today — swapping the UI font in settings hands
`d.fonts.text` a `queued` handle via `reload_font`, and `resolve()` asserts on an unpopulated
generation. Fixed with an early-out.

---

## What was implemented

**1. `get()` is now total.** `discover_baked<T>` dropped its `record.baked_exists` condition, so
every catalog record gets a slot during `initialize()`, baked or not. This was the single
load-bearing change: it was the *only* reason `app_setup` had to be sequenced behind the compile.

The self-healing path already worked — a slot with no baked file goes `queued` → `flush()` →
`launch_load` → `recompile_if_stale` (stale, because `!exists(dst)`) → `bake_to_disk` → `load()`.
No new load volume: `discover_baked` already queued every baked asset, so this only adds the
not-yet-baked stragglers to a set that was already being loaded eagerly.

**2. Boot no longer compiles.** The deferred boot task now runs `app_setup` and sets
`m_boot_tasks_done`; the `compile_non_boot_critical` call is gone from the boot path. The function
itself is kept as API — it is the right primitive for an explicit cook/precompile step — it just
does not run at startup. `compile_boot_critical` is unchanged: fonts must exist before the loading
screen can draw itself, and it costs 13 ms.

**3. Failed loads now log.** `launch_load`'s `fail` lambda stored the error into the slot and
returned silently. With compilation moved onto the load path that was the *only* error path, so a
broken asset would have failed invisibly. It now logs at error level before storing.

**4. `add_text_quads` early-outs** on an invalid font handle.

**5. Font loading parallelised** — see below.

**Reverted:** the `m_boot_compiling` flag and `compile_progress` → loading-bar wiring added earlier
the same day. Both existed to make the bar honest about a compile phase that no longer blocks boot.
The settle path is the sole driver of the boot bar again.

---

## Parallelism between assets

The loads themselves were already parallel — `flush()` launches every queued slot at once and each
runs on a worker. The serialisation was in **waiting**, not loading:

**Chained waits cost a frame each.** `asset::load<T>` is a poll loop —
`co_await ctx.yield_tick()` until the handle settles. It does not initiate anything; `flush()`
does. So each `co_await asset::load<T>(...)` costs **at least one frame even when the asset is
already loaded**. `gui::init` chained three of them: text font, code font, then a separate
spin-wait on the blank texture. Three frames minimum — and during boot, frames are long because
shader compilation is saturating the workers.

Fixed by collapsing all three into a single poll loop that waits for the whole set, then checks
failures once. Three frame-waits become one.

**The general rule this establishes: poll a set, never chain the waits.** Any future code that
needs N assets before proceeding should acquire all N handles first and poll them together. Chaining
`co_await asset::load` is O(N) frames regardless of how fast the assets load.

**Remaining known serialisation, not addressed:** `gpu_resume_request` handlers are drained in one
frame but resumed inline one after another, so the GPU-side tail of each load (buffer creation,
upload recording) runs serially on the GPU system thread. Inherent to a single submission thread
and cheap per asset; revisit only if profiling shows it.

---

## Verification

Boot timeline before the asset fix, after the asset fix, and after this change:

| | boot time | compile phase |
|---|---|---|
| broken `character.v3.gsmdl` | ~62 s | 55–60 s |
| asset regenerated | 3.64 s | 44 ms |
| this change | measure | not on boot path |

Still to check by running:

- boot with `Sandbox/.gse/baked/` deleted entirely — every asset self-heals through the pre-load
  hook, nothing asserts, character still spawns on F7 once its load completes
- boot with a deliberately malformed asset — slot reaches `state::failed`, error is logged once,
  menu is unaffected
- F7 spawn immediately after the menu appears, before background compilation finishes — must
  early-out cleanly, not assert
- swap the UI font in settings — exercises the `add_text_quads` guard
- confirm no `compile_non_boot_critical` between `loading_screen pushed` and `mark_finished`

---

## Dead code removed

Six symbols had zero callers once boot stopped compiling. All deleted.

| Symbol | Why it died |
|---|---|
| `asset::load<T>` | last caller was `gui::init`; also the chained-await footgun — removing it makes the anti-pattern unavailable, not merely discouraged |
| `compile_non_boot_critical` | no longer called at boot |
| `compile_all` | already dead before this work |
| `compile_progress` | only ever a defaulted parameter; the sole surviving caller passes nothing |
| `count_compile_work` | only reachable inside `if (progress)` |
| `source_reader::exhausted()` | pre-existing dead code, unrelated to this work |

`compile<T>` and `compile_boot_critical` lost their `progress` parameter and the `tick()` lambda with it.
`compile<T>`, `bake_to_disk`, `recompile_if_stale`, `discover_baked` and `compile_boot_critical`
all remain live.

Removing `compile_progress` takes one `std::function` out of an exported `gse.assets` partition
interface — relevant given std type-erasure in a cross-loaded BMI is a known corruption source here.
It does **not** clear the hazard: `std::function` still appears in `AssetRegistry.cppm`
(`m_pre_load_fn` and its setters) and `AssetState.cppm` (the hot-reload fns). Those are a separate
question.

---

## Out of scope

- changing what is annotated `boot_critical` (`font` is correct — it draws the loading screen)
- asset dependency graphs (a skinned model pulling its textures) — `try_get` already returns empty
  handles and `uploads_ready()` already reports residency
- a general `asset::load_all` helper; the poll-a-set pattern is currently one site
