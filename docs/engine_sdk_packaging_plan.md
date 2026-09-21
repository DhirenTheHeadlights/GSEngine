# Phase 5 — Engine SDK + packaging

Phases 1 to 4 of the editor-first restructure shipped; this is the only open phase. Status: **SDK spike PASSED 2026-07-31.** The gate is cleared; what remains is packaging work, not a research question.

Everything here serves *shipping to someone else*. Daily work stays on engine-source-by-default (phase 3a).

## Per-project engine workflow (2026-09-20, landed)

Decided before starting the packager, because engine fixes made from inside a project need to be trackable and pushable per project. The primitive already existed — projects name an engine checkout, engine lines are git worktrees — what was missing was a convention and editor support. Submodules and per-project engine copies were both rejected (nested checkout breaks the short-path rule; copies stop fixes propagating).

- **One engine worktree per project, on a branch named for the project.** New Project has a "Create Engine Worktree" toggle (default on). It runs `git worktree add` from the editor's own engine tree into a *sibling* folder `<EngineFolder>-<ProjectName>` on branch `project/<ProjectName>` (based on the editor's HEAD), then rebinds the project's `[engine]` section to it and registers it in `engines.ini` under the folder name. Sibling placement keeps paths short and needs no fixed folder — worktree discovery goes through git's own metadata. Failure logs and leaves the project bound to the editor's engine.
- **Engine commit pin.** `[engine] commit = <sha>` in `.gseproj`, written after every *successful game build* whose engine HEAD differs from the pin. The runtime pin lives in workspace state (seeded from the manifest at init) so the panel never reads a stale load. Verified end to end on the Throwaway project: pin equals worktree HEAD after the first build.
- **Source Control panel** (`gse.ide.source_control`, hidden by default). One section per discovered repository — project and engine worktree alike — with branch, ahead/behind, upstream state, and for the engine repo one of *unpinned / pinned / drifted from <sha7>*. Rows are the changed files with `+N -N` counts (numstat vs HEAD; untracked files count lines on disk) and a checkbox left of the name; the header checkbox selects the whole repository. Commit stages only the selected paths (batched `git add -A -- …`, message via `-F` scratch file) and Push adds an upstream to origin when the branch has none. Actions run one at a time through the git system's `task::pending` command slot; failures show at the top of the panel.
- **Git system changes that made this possible.** Status moved to porcelain v2 with branch headers (branch, upstream, ahead/behind, HEAD oid). Init/commit/push are all step lists on a root through one runner. The watcher resolves the real git dir through the `.git` *file*, so linked worktrees refresh on HEAD/index changes. Status codes and action builders are enumerator annotations, not switches.
- **Any jump-to-file reveals the Code panel.** `editor_app` also reads `jump_to_request`; it inserts the Code panel into the primary view if no window hosts it, activates its tab, and pushes `window_focus_request` so a click from a detached panel brings the code window forward.
- **Shared fix found along the way:** `layout_store::apply` accumulated one blank line per section rewrite (the separator before a removed section stayed attached to the previous one). It now trims the tail before appending, so every layout file gets exactly one separator.

The pin is also the value item 4 below should stamp; the two share a field.

## Spike result

A foreign translation unit compiled against relocated BMIs, linked against relocated libraries, and ran:

```
sdk ok: 6 m
```

Setup: the engine's 395 `.gcm` files and 5 `.a` files were copied out of `out/build/x64-mingw-gcc-RelWithDebInfo` into `%TEMP%/gse-sdk-layout` — a path with no relationship to the repo. A consumer TU in a third directory (`%TEMP%/gse-sdk-consumer`) did `import std; import gse;`, used `gse::vec3<gse::length>` and `std::println`, and produced a 197 KB object, a 589 MB executable, and correct output with the unit formatter intact.

Path A (hand-generated flags) is therefore **viable**, and needed only two direct module mappings — `gse` and `std`. Everything else resolved on its own, for the reason below.

## The finding that constrains the design

**GCC bakes each dependency BMI's path into the importing BMI, and does not re-map transitive imports through the module mapper.** Surfaced by:

```
gse.assert: error: failed to read compiled module: No such file or directory
gse.assert: note: compiled module file is 'Engine/CMakeFiles/Engine.dir/gse.assert.gcm'
```

The mapper said `gse.assert` lived at `<sdk>/Modules/gse.assert.gcm`; GCC used the path stored inside `gse.gcm` instead. A second variant confirmed it by prefixing `$root` onto that baked path rather than onto the mapped one.

The baked path is **relative**, and is resolved against the compiler's working directory:

| Image shape | Consumer cwd | Result |
|---|---|---|
| Flat `Modules/*.gcm`, every module mapped | anywhere | **fails** — transitive deps unresolvable |
| Original `Engine/CMakeFiles/Engine.dir/…` layout preserved | SDK root | **works** — all 395 BMIs load |

So the SDK image is **a relocatable copy of a build tree**, not an arbitrary directory of modules. Two consequences:

- The packager must preserve the module tree's relative layout verbatim. It cannot flatten, rename, or reorganise into a prettier `Modules/` directory.
- The consumer must compile with its root at the SDK. CMake sets the working directory to the *consumer's* build tree, so either the SDK is unpacked into that build tree at the expected relative path, or the generated project passes a root that makes the baked paths resolve. **This is the main open design question for Path B.**

## Gotchas found the hard way

- **`-fmodule-mapper=C:/…` is ambiguous with GCC's `host:port` socket syntax** and fails with no diagnostic at all. CMake always passes a *relative* mapper path; so must the packager and any generated build.
- **Relative entries inside a hand-written mapper did not resolve against cwd** even with `$root .`, while absolute entries did — and baked transitive paths *do* resolve against cwd. That asymmetry was not fully isolated and cost several iterations. Understand it before writing the real packager.
- **`g++` exits non-zero with zero output** when the toolchain's own DLLs are not on `PATH`. Any packaging script that shells out to the compiler must put `<toolchain>/bin` on `PATH` first or it will look like a silent, causeless failure.
- The engine's phase-1 path resolution already behaves correctly for an install image: without `gse.manifest` the app refuses to start and says why, then works once one is supplied. The `mode = installed` hook is real, not theoretical.

## Measured sizes

| | |
|---|---|
| Engine BMIs (395 files) | 200 MB |
| `std` + `std.compat` BMIs | 34 MB |
| `libEngine.a` | 710 MB |
| Linked consumer exe | 589 MB |

Per config. Shipping Debug **and** RelWithDebInfo puts the image comfortably at the top of the parent plan's "1-2 GB incl. toolchain" estimate, before the toolchain's own ~580 MB.

## Remaining work

Ordered 2026-09-20: get one working SDK end to end on a single config before widening or polishing. Items 1–4 are the minimum viable SDK, 5–6 widen it, 7–8 are polish that depends on everything above being stable.

1. **SDK package script** (`scripts/package_engine_sdk.py`, modelled on `package_gcc_toolchain.py`). Copy-only — it takes a build dir, the engine root and a stage dir; building stays with the build runner. Scope:
   | Source | Image | Rule |
   |---|---|---|
   | `Engine/CMakeFiles/{Engine,VulkanModule,__cmake_cxx_std_26}.dir/**/*.gcm` | same relative path | verbatim — baked relative paths (see above). Filter by extension: `Engine.dir` is >1 GB of objects around ~110 MB of BMIs |
   | `Engine/lib*.a` | `Engine/` | verbatim |
   | repo `Engine/Resources` | `Engine/Resources` | read here in both modes |
   | repo `Engine/Engine` | `Engine/Source` | installed mode resolves source assets and shaders here; ship the whole folder |
   | build `Engine/Resources` | `Engine/Baked` | installed mode reads baked output here |
   | runtime DLLs + `D3D12/` beside the built exe | `Bin/` | only what the build tree already produced |
   Plus a generated `gse.manifest` (`mode = installed`, no root line) and a module listing (name → relative BMI path, derived from `CXXModules.json` with the absolute prefix stripped). A `--verify` flag repeats the spike against the staged image: hello TU importing `std` and `gse`, absolute-path mapper generated into a temp dir and passed as a *relative* path, cwd at the image root, toolchain bin on `PATH` first. That is the acceptance criterion. The portable zip falls out of this.
2. **Vendor the dependencies.** The spike borrowed vcpkg's static libs and runtime DLLs from the build tree; the image needs its own copies plus the gcc runtime DLLs that `gse_copy_runtime_deps` normally supplies. Until then the image only runs on the packaging machine.
3. **`[engine] version = <n>`** resolution in `project::load()`, alongside `name`, `source` and `commit`. This is the consumer side — without it the editor cannot point a project at a packaged SDK, so the SDK path cannot be dogfooded from inside the editor. It also forces the open design question above (how the generated project makes the baked relative paths resolve).
4. **Toolchain lockstep stamp.** `gse_write_manifest()` writes `mode`, `root` and `project`. It needs the product version and a toolchain hash, and the editor must refuse a mismatch loudly — a BMI/compiler mismatch presents as "Bad file data" on every module, and a plugin mismatch as every analyzed TU failing identically. Neither names the real cause. Cheap; do it before anyone else touches an SDK and before swapping toolchains while dogfooding item 3.
5. **`std.gcm` policy.** Decision, not work: recommend *not* shipping it — consumers pay one build on first configure and the exact-flags problem disappears. Revisit only if first-configure time becomes a complaint.
6. **Per-config trees** — `Modules/<config>`, `Lib/<config>`. Needed once Debug ships alongside RelWithDebInfo (no cross-config BMI bets); mostly a layout change to the script from item 1.
7. **`install()` / `export()` rules.** Moved down from first: the spike showed the image must be a verbatim copy of the build tree layout, which is what a copy script does naturally and what install rules tend to fight. The benefit is a CMake-native `find_package` consumer, which is really Path B — do it after Path A works, if at all.
8. **Inno Setup installer** and updates v1, per the parent plan. It wraps a finished image, so anything wrong underneath gets baked into a release; the zip from item 1 covers testers in the meantime.

## Path B

`install(TARGETS … FILE_SET CXX_MODULES)` + `find_package` remains untried. It is the newest corner of CMake's module support and GCC is the least-exercised compiler in it, so it should be attempted *after* the packager works via Path A rather than bet on. The baked-path constraint above applies to it identically — whatever CMake generates still has to make those relative paths resolve.
