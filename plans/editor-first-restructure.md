# Editor-First Restructure

Status: settled 2026-07-20, not started. Goal: the editor is the installed all-in-one product — it ships the engine as a prebuilt SDK, creates/opens projects from a projects directory, and never writes runtime artifacts into its install dir or the repo.

## Settled decisions

- **Engine consumption**: ~~prebuilt SDK only~~ — **AMENDED 2026-07-24: engine source by default, SDK for distribution.** Projects name an engine checkout in their `.gseproj`; engine edits from inside a project are expected and frequent, not an escape hatch. The prebuilt SDK becomes what you ship to someone else, not what you develop against. See [Workflow decisions](#workflow-decisions-2026-07-24).
- **GoonSquad**: stays in-repo through phases 1-5 (becomes the dev-mode project), then physically moves to the projects dir once the SDK path is proven (phase 6). Server moves with it (links GoonSquadLib).
- **Per-project editor state**: everything derived/local lives in `<project>/.gse/`, gitignored.
- **Packaging v1**: per-user Inno Setup installer into `%LOCALAPPDATA%\Programs\GSE`, no UAC. Portable zip falls out of the same package step.

## Workflow decisions (2026-07-24)

Dhiren runs 4-6 agents at once across engine branches (`editor`, `locomotion`, `dx12`) as git worktrees, will add a 2D tool project alongside GoonSquad, and needs to switch between them freely. Two axes were being conflated: **engine development lines** (git branches — worktrees are correct for these) and **projects** (different products with different lifetimes). Today both live in one tree, so every engine branch drags a full copy of Game/, Editor/, docs/ and scripts/ — the source of the repo bloat.

**SETTLED — one project per process, distinct-looking windows.** The complaint was "three editors that look the same", which is an identity problem, not a multi-project-per-process problem. Fix: project name in the window title and custom chrome, plus a per-project accent swatch. Quick-switch becomes a picker that relaunches (`app::relaunch_on_exit` + the recent-projects list). This preserves the phase-1/2 invariant that config accessors hand out `const path&` into an immutable table, which the search index, diagnostics runner and build runner all rely on across frames. Multi-project-per-process would have forced snapshot discipline into every worker and a second pass over all editor call sites.

**SETTLED — projects name an engine, they do not contain one.** "Each game holds its own engine copy" was considered and rejected: copies mean engine fixes don't propagate and the same fix gets hand-merged N times. Instead `.gseproj` gains an `[engine]` section — `source = <path to an engine worktree>` for development, `version = <n>` for a packaged SDK. Two projects can share a worktree and branch when they diverge.

**SETTLED — engine source by default; the SDK is for distribution.** This supersedes the original "prebuilt SDK only, read-only, version-locked" decision. Consequences:

1. **The single largest risk in this plan mostly evaporates.** Sharp edge #1 was exporting GCC C++-module BMIs, which gated everything. That problem only exists for a *packaged* SDK. A project consuming engine **source** just points its CMake at the engine tree — no BMI export, no install/export rules, no toolchain-hash lockstep for daily work. The spike moves from "gate for phases 3-6" to "needed before packaging".
2. **Project relocation becomes the enabler, not the finale.** Moving Game/ and Server/ out of the repo is what makes projects independent, shrinks every worktree, and delivers the repo cleanup — so it moves ahead of the SDK work.

**Two distinct engine notions, easy to conflate.** After the split these can differ and must not be merged:

| | Meaning | Source |
|---|---|---|
| `gse::config::root_dir()` | the **running editor's own** install/build — where its shaders, fonts and resources come from | `gse.manifest` beside the exe |
| `ide::config::engine_root()` | the **project's** engine — what the game compiles against, and what you browse and index | `.gseproj` `[engine] source`, defaulting to the editor's own |

They are the same today. They stop being the same the moment an editor built from one worktree opens a project bound to another.

## Why this is a restructure, not a feature

`gse.config` (Engine/Engine/Import/Config.cppm.in) and `gse.ide.config` (Editor/Editor/Import/Config.cppm.in) are CMake `configure_file`-generated modules baking absolute configure-time paths (`root_dir`, `resource_path`, `baked_resource_path`, `game_source_dir`, `token_plugin`) into the binaries. Every resource lookup, runtime write, workspace root, index root, and build invocation resolves through them. Binaries are not relocatable and the editor's "project" is hardwired to this repo.

Key anchor points (verified 2026-07-20):

- Workspace root hardcoded: Editor/Editor/Source/App/EditorApp.cppm:3098 (`d.ws.root = gse::config::root_dir`)
- Index root: Editor/Editor/Source/Search/SearchSystem.cppm:59; plugin gets `-fplugin-arg-gse_tokens-root=` from it (Analysis/SymbolIndexBuilder.cppm:113)
- Build runner: scans `root_dir/out/build` for newest build.ninja (BuildRunner.cppm:112-136), hardcoded targets `GoonSquad`/`Editor`, game exe at `build_dir/Game/GoonSquad.exe` (:425), build cwd = root_dir (:457)
- Diagnostics find compile_commands.json under ws.root (EditorApp.cppm:1870)
- `main()` takes no argv (Editor/Editor/Source/Main.cpp:7)
- Engine is never install()ed/exported; editor links same-build `Engine` target (Editor/CMakeLists.txt:83,86). No packaging exists anywhere; `dist/` ships only the compiler (scripts/package_gcc_toolchain.py).

## Target disk layout

### Install image (relocatable; the portable zip is this exact tree)

| Path | Contents |
|---|---|
| `GSE/Editor/` | Editor.exe, runtime DLLs (vcpkg + gcc runtime + Agility SDK), editor Resources/, gse_tokens.dll, cppref.idx |
| `GSE/Engine/` | `Source/` (full engine source: reference, debugging, xref targets), `Lib/<config>/`, `Modules/<config>/` (BMIs incl. std), `Resources/`, prebaked engine assets |
| `GSE/Toolchain/` | gcc-trunk + pinned cmake + pinned ninja (user installs nothing else) |
| `GSE/Templates/` | project templates |
| `GSE/gse.manifest` | product version + toolchain hash; doubles as the install-root marker file |

### Machine state

| Path | Contents |
|---|---|
| `%APPDATA%\GSE\` | user-scope settings.ini, recent_projects, default layouts, keybinds/themes later |
| `%LOCALAPPDATA%\GSE\` | `logs\` (file log sink — currently dormant, only the terminal ring sink exists), `cache\` (font atlas, engine symbol index), `crash\` (Aftermath dumps + shader blobs), `profile\` |

### Project

```
MyGame/
├─ MyGame.gseproj      ini manifest: [project] name, engine version; targets section reserved
├─ Source/             game modules + entry
├─ Assets/             source assets (project-side replacement for Engine/Resources)
├─ Config/settings.ini project-scope settings
├─ .gitignore          ignores .gse/
└─ .gse/               build/<config>/, baked/, symbols/, layout inis, captures/, compile_commands
```

Projects root default: `%USERPROFILE%\GSEProjects` (no spaces on purpose; spaced paths still supported — quoting discipline in every spawned command line).

### Dev mode

Running from a repo build: detection via a CMake-generated marker in the build dir (installed mode: walk up from exe dir to `gse.manifest`). Resource roots point at the source tree (live engine-resource iteration); user/machine state goes to REAL AppData — the repo stops being dirtied at phase 1, before any product work. Env overrides `GSE_USER_DIR` / `GSE_STATE_DIR` for isolation and tests. Dev-only features (editor self-rebuild `.bak` dance, Server target) gate on dev mode.

## Runtime-write migration table

| Artifact | Today (code anchor) | Target |
|---|---|---|
| settings.ini | `resource_path/Misc/settings.ini` (Runtime/Engine.cpp:55; write SaveSystem.cppm:320) | `%APPDATA%\GSE\` (user scope) + `Config/settings.ini` (project scope, phase 2 split) |
| editor_layout.ini | `resource_path/editor_layout.ini` (EditorApp.cppm:407) | `<project>/.gse/` (AppData default until a project exists) |
| gui_layout.ini | CWD-relative literal `Misc/gui_layout.ini` (Gui/Gui.cppm:84) | same as above |
| Aftermath crash dumps + shader blobs | `resource_path/Misc/crash_dumps`, `aftermath_shaders` (Vulkan/Aftermath.cppm:108,112) | `%LOCALAPPDATA%\GSE\crash\` |
| profile.txt/json | `resource_path/Misc/` (Diag/ProfileAggregator.cppm:63,67) | `%LOCALAPPDATA%\GSE\profile\` |
| Captures (screenshots/recordings/clips) | via resource_path (Graphics/Renderers/CaptureRenderer.cpp:190,283,359) | `<project>/.gse/captures/` |
| Baked assets | `baked_resource_path` in build dir (Assets/AssetSystem.cppm:132) | engine: prebaked in SDK; project: `.gse/baked/` |
| Font atlas debug PNG | `baked_resource_path/Fonts` (FontCompiler.cppm:235) | with baked output |
| TU symbol cache | next to compile_commands.json, `gseditor_symbols/` (Search/Index.cppm:999) | project: `.gse/symbols/`; engine: shipped in SDK / `%LOCALAPPDATA%` cache |
| Game-code writes (locomotion_checkpoint.bin → CWD, LocomotionTrainer.cppm:27) | CWD | sanctioned project-data path API |
| Log file | none wired (json_sink exists, Log.cppm:125) | `%LOCALAPPDATA%\GSE\logs\` |

Already correct (system temp): analysis/diagnostics/git intermediates, build game-graph cache.

## Phases

### Phase 1 — Relocatable paths + artifact hygiene (standalone pain relief; prerequisite for everything)

Detailed implementation plan: [editor-first-restructure-phase1.md](editor-first-restructure-phase1.md) (planned 2026-07-20).

- New runtime-resolved paths module (`gse.paths` or repurposed `gse.config`): install_root, engine resources, baked roots, user_config_dir, user_state_dir, logs/cache/crash/profile dirs, project-slot setters (consumed in phase 2). Known-folder lookup via the gse.win32 wrapper (no raw #includes). Constants become functions; call-site migration is mechanical.
- Mode detection: exe-dir walk-up to `gse.manifest` → installed; CMake-generated dev marker in build dir → dev (points at source tree). Env overrides.
- Migrate every writer per the table; wire json_sink to logs dir.
- The uncommitted `gse.win32.environment` work (PATH-prefix env blocks for children) composes here: it is how the editor spawns the private shipped toolchain without touching user PATH.

### Phase 2 — Project model (editor becomes project-shaped, still mono-build) — **DONE, built**

Notes against the plan as written: **argv needed no `Main.cpp` change** — `project::current()` is a magic static that reads `GetCommandLineW()` itself, so there is no ordering dependency on `gse::start` and the immutable-table invariant is preserved structurally rather than by discipline. Settings scope ended up **per-field** (`gse::settings::project_scope{}` on any member, inheriting from the struct) rather than per-struct; entry-level scope was removed as a second source of truth. Workspace went **multi-root** here rather than in phase 3, so the editor never regressed to losing engine browsing.

- `.gseproj` ini manifest (reuse sectioned-ini machinery; no new parser). argv in Main.cpp (`Editor.exe <path>.gseproj`) — also the file-association hook.
- Project context set at open; repoint: workspace root (EditorApp.cppm:3098), search/index root (SearchSystem.cppm:59), diagnostics compile_commands discovery (EditorApp.cppm:1870), git, build runner roots/targets, layout paths.
- Recent projects in `%APPDATA%`. Settings registry gains user|project scope; two files.
- `Game/` gets a committed `.gseproj` = first project; dev mode opens it by default → day-to-day behavior unchanged while the machinery gets exercised.

### Phase 3 — Project relocation + engine binding (was phase 6; now the enabler)

Reordered 2026-07-24. This is what makes projects independent, shrinks every engine worktree, and delivers the repo cleanup — and with engine-source-by-default it no longer waits on the SDK spike.

**Split into 3a and 3b, 2026-07-24.** Moving `Game/` out of the repo lands on `main` eventually, which hits the **locomotion branch mid-flight** — active training work lives in `Game/`. That is a merge conflict against another agent's in-progress work, not a refactor. So the capability ships first, without moving anything:

- **3a — engine binding + external project builds. DONE (built, spike passed).** `[engine] source` in the manifest, `engine_root()`/`engine_source_dir()`, per-project build trees with automatic nested-vs-standalone detection, BuildRunner distinguishing the two build trees, manifest-driven `game_target()`, index and diagnostics on the project's compilation database. Proven by an out-of-tree project that builds and runs.
- **3b — relocate GoonSquad + Server.** Purely the file move plus multi-target manifest support. Needs coordinating with the locomotion branch; do it when that work is at a stopping point, not opportunistically.

- `.gseproj` gains `[engine] source = <path>`; `ide::config` grows `engine_root()` / `engine_source_dir()` resolving from it and defaulting to the editor's own `gse::config::root_dir()`. Browse roots and the index follow the **project's** engine, not the editor's.
- Move Game/ out of the repo into the projects dir; locomotion tooling paths follow. Repo becomes Engine + Editor + Tools.
- **Server resolved 2026-07-24 — done ahead of 3b.** The original note ("Server moves with the game, it links GoonSquadLib") described the CMake, not the code. `gse.server` had **zero** `gs::` references and was already engine-namespaced; the entire game dependency was one line in `Server/Main.cpp`. So Server is **not** a tool like the Editor — it is a *second target of a project*, and it split accordingly:
  - `gse.server` → `Engine/Server/`, built as a separate **`EngineServer`** target. Separate rather than folded into `Engine` because it does `import gse;` and **nothing inside the Engine target imports the umbrella** — doing so forces a full load of the re-export graph, the same shape as the `Bad file data` BMI failure. Folding it in would need its umbrella import decomposed into ~8 specific imports first; worth doing eventually, not blocking.
  - The entry point → `Game/Game/Source/ServerMain.cpp`, target `GoonSquadServer` declared in `Game/CMakeLists.txt`, named in `[targets] server`.
  - **Top-level `Server/` is gone.** It hardcoded `GoonSquadLib` from the repo root, which breaks the moment there is a second game. Any project wanting a server declares its own target linking `Engine` + `EngineServer` + its own game lib.
  - Multi-target manifest support therefore already exists — `[targets]` is read, and `game_target()` now defaults to the project's own name rather than a hardcoded `"GoonSquad"`. The editor's dev-default project is **discovered** by scanning for a `.gseproj` one level below the engine root, not hardcoded to `Game/GoonSquad.gseproj`.
  - Verified: `GoonSquad` appears nowhere outside `Game/` in any CMake or source file.
- Per-project build dir under `.gse/build/<config>`; project CMakeLists template points at the bound engine tree (plain `add_subdirectory`/path reference — **no BMI export, no install/export rules, no toolchain stamp**, because the engine is source here). Runtime-deps copy mirrors `gse_copy_runtime_deps`.
- ~~Symbol cache moves to `.gse/symbols`~~ — **DROPPED 2026-07-24, no work needed.** The cache path is derived as `compile_commands.parent_path() / "gseditor_symbols"` ([Index.cppm:1466](../Editor/Editor/Source/Search/Index.cppm)). With per-project builds that already resolves to `<project>/.gse/build/<config>/gseditor_symbols` — inside `.gse`, gitignored, and separated per config. A flat `.gse/symbols` would be strictly worse: it decouples the cache from the compilation database whose fingerprints key it, so a config switch would silently reuse stale entries. Keep the derivation.
- Targets from `[targets]` (also deferred from phase 2 — pointless until a project can build its own target).
- Asset system grows multi-root: engine root + project Assets/ watched → `.gse/baked/`.
- ~~The old phase-3 spike survives in reduced form~~ — **SPIKE PASSED 2026-07-24.** A project at `C:/Users/Dhiren/AppData/Local/Temp/gse-spike` (outside the engine tree entirely) configured, built the full engine plus its own exe with zero errors, and ran. Resolved paths from the out-of-tree binary:

  | | Resolved to |
  |---|---|
  | `root_dir()` / `resource_path()` | the **engine worktree** — engine source assets |
  | `build_root()` / `baked_resource_path()` | the **project's** `.gse/build/<config>` — baked output |
  | log file | `%LOCALAPPDATA%\GSE\logs\SpikeGame.log` — per-exe naming held for a foreign consumer |

  That is the engine-vs-project split working end to end: source assets read from the engine, derived assets written into the project. Nothing leaked into the engine tree. Engine-source-by-default is therefore proven, not assumed — no BMI export, no install rules, no toolchain stamp involved.

#### Sites still resolving to the editor's own tree — each needs a which-root decision

These are all `gse::config::root_dir()` today. That is currently correct *only because project, engine and editor trees are the same path*. They silently become wrong the moment a project binds a different engine worktree, and nothing will fail loudly. Audited 2026-07-24 after `engine_root()` landed.

**No longer hypothetical, 2026-07-27.** `BasePlanningTool` at `%USERPROFILE%\GSEProjects` builds and runs from the editor, so project root and engine root are now different paths in normal use and these sites are live-wrong. What already adapted: `browse_roots()` returns project → Engine → Editor and the file tree follows it, and `search_system::init` binds `compile_commands` to the **project**. What did not: `index->workspace_root`, `build_files_and_content`, and the file watcher are all still rooted at the editor's own tree ([SearchSystem.cppm:52,66,69](../Editor/Editor/Source/Search/SearchSystem.cppm)) — so for an external project, file search and content search cannot see the project's own sources and edits to them invalidate nothing. The symbol index is fine; it keys off the compilation database.

**Resolved 2026-07-27 — the index, the analyzer and the plugin are all multi-root now.** `config::analysis_roots()` is the single derivation (from `browse_roots()`), so the tree, the file/content index, the file watcher, the symbol index and the live analyzer all see the same set. Shape of the change:

- `index_state::workspace_root` (one path) → `std::vector<index_root>` (`path`, `name`, `is_project`); `owning_root(roots, path)` replaces `is_indexed_path(workspace_root, path)` at every membership check. `build_files_and_content` walks each root, first-root-wins so overlapping roots can't double-index a file, and search results are prefixed with the root name.
- **The GCC plugin had to change** — it filtered emission through a single `g_root` prefix (`under_root`, `main_file_under_root`), so any root but the first would have produced *zero* tokens for its TUs. Now `g_roots[8]`, `-fplugin-arg-gse_tokens-root=` accepted repeatedly, `under_root` loops. Cost is nil: `under_root_cached`'s 256-slot direct-mapped memo and `main_file_under_root`'s one-shot cache both sit in front of it, so the loop only runs on a miss. `gse_tokens.dll` rebuilds automatically (`DEPENDS` on the plugin source), and the contract test still passes one root, which remains valid.
- LOC counting now credits any root marked `is_project`, since `is_counted_source_dir`'s hardcoded `Editor/Engine/Game/Server` allowlist can't know a new project's layout.
- `ws.root` survives only as the display anchor for diagnostic paths and now points at the project, so a project-file diagnostic no longer renders as `../../../../GSEProjects/…`.

**Multi-root was only half of it — analysis is also multi-database, 2026-07-28.** Browse roots let you open files from three trees, but every analysis request went to `project_compile_commands()`, which structurally cannot contain Editor TUs: a project does not build the editor. Opening `EditorApp.cppm` from inside `BasePlanningTool` therefore reported `entry_unavailable` forever. Fix: `browse_root` carries the database that analyzes its files (project and Engine → the project's; Editor → the editor's own `build_dir()`), `config::compile_commands_for(file)` resolves by owning root, and `index_state::compile_commands` became a deduped list. Notes:

- **Engine TUs live in both databases**, so TU discovery dedups on `canonical_path_id`, first database wins. Without that the index would compile ~400 engine TUs twice.
- The **in-tree project is unaffected**: for a nested project `project_compile_commands()` and `build_dir()` resolve to the same file, so the list dedups to one entry and behaves exactly as before.
- The symbol cache still derives from `compile_commands.front().parent_path()`, keeping sharp-edge-#140's invariant (cache tied to the database whose fingerprints key it). Entries from the editor's database land in the project's cache dir, which is safe because the cache key includes `entry.command.fingerprint`.
- Cost: the index now compiles the Editor's ~90 TUs on top of the project's, so a cold symbol build is meaningfully longer.
- `analysis_unavailable` (no database — build the project) and `analysis_outside_build` (database exists, file absent from it) are now distinct statuses. They were both reported as `analysis_crashed`, which rendered "analyzer exited abnormally" for a project that simply had not been built.

| Site | Should probably become |
|---|---|
| ~~[SearchSystem.cppm](../Editor/Editor/Source/Search/SearchSystem.cppm) index/workspace root~~ | **DONE** — `index_state::roots` from `browse_roots()`, one watcher per root |
| ~~[EditorApp.cppm](../Editor/Editor/Source/App/EditorApp.cppm) `ws.root`~~ | **DONE** — `project_root()`; display anchor only, tree uses `fs_root.children` |
| ~~[GitSystem.cppm](../Editor/Editor/Source/Git/GitSystem.cppm) repo root~~ | **DONE** — `project_root()` |
| ~~[Terminal.cpp](../Editor/Editor/Source/Terminal/Terminal.cpp) cwd and prompt~~ | **DONE** — `project_root()` for all three |
| [BuildRunner.cppm:447,506](../Editor/Editor/Source/BuildRunner/BuildRunner.cppm) build cwd | the tree being built — engine for editor builds, project for game builds |
| [BuildRunner.cppm:618](../Editor/Editor/Source/BuildRunner/BuildRunner.cppm) relaunch cwd | editor's own tree — correct as-is |
| [Config.cpp:96](../Editor/Editor/Import/Config.cpp) Editor browse root | editor's own tree — correct as-is |

### Phase 4 — Product UX — **switcher + New Project done (unbuilt); hub pending**

- ~~Project switcher~~ **DONE.** `project_screen`, Ctrl+Shift+P or the project glyph in the title bar. Lists recent ∪ discovered, dedup, missing entries filtered. Selecting relaunches with the chosen manifest on the command line.
- ~~New Project~~ **DONE.** Header button → name field with live validation → generates the project, records it, relaunches into it. The generated `CMakeLists.txt` **is the spike's**, i.e. the one configuration proven to build and run out of tree.
  - **The capability that was actually missing was `cmake` configure** — the build runner could only `--build`. `ensure_configured()` now configures a project that has a `CMakeLists.txt` but no `build.ninja`, with every setting lifted from the editor's own `CMakeCache.txt` (toolchain file, triplet, overlays, both compilers, build type) so a new project matches the editor that made it instead of a hardcoded guess. Reuses the engine build's `vcpkg_installed` in non-manifest mode — correct because the project compiles the engine from source and needs exactly its dependency set.
  - Validation rejects deep paths up front; the 250-char object-path limit otherwise fails late as a misleading `ar` error.
- Hub/launcher on projectless launch: recent list, New Project, Open. **Still pending** — currently a projectless launch falls back to repo roots rather than showing a hub.
- Templates (Blank first). Engine symbol index layering → instant engine-wide search/xref. **Pending.**

#### GUI work that came out of this phase

Generic, not project-specific — all in the engine widget layer:

- **Click-outside dismisses any dismissable screen**, engine-wide (one check in `process_screen`, before `tick()` since that can pop and dangle the captured screen).
- **`selectable`** gained `detail` (second column + divider), `align`, `accent` swatch, and an explicit `key` so same-named rows don't collide on widget id. It had zero callers before, so the API was free to fix.
- **`draw::button_in_rect`** following the existing `*_in_rect` convention, and `enabled` on both button forms — disabled uses the already-existing `color_text_disabled` / `color_widget_background`, so it themes correctly across all six presets.
- **`marquee_text`** primitive (`:marquee_widget`), stateless — phase from absolute time, no per-widget state. Wired into `selectable`'s detail column, scrolling only the hovered row.
- **UI text rendering is allocation-free.** `text_command::text` and `unified_command::text` are `std::string_view`; `queue_text` interns into a `per_frame_resource<std::deque<std::string>>`. Previously every text command allocated **twice per frame** (caller temporary + the renderer's `unified` copy). See the gotcha below.

### Phase 5 — Engine SDK + packaging (was phases 3 and 5; now distribution-only)

Everything here serves *shipping to someone else*, not daily work. The BMI-export risk is confined to this phase.

- **SDK spike (gate for this phase only)**: build a hello-world game TU in a foreign directory against prebuilt BMIs + libs. Path A: hand-generated flags. Path B: CMake `install(TARGETS ... FILE_SET CXX_MODULES)` + find_package. Pick by pain. Pass criterion: sample project builds and runs a windowed app from the SDK image with the repo renamed away.
- Package script assembling the SDK image; toolchain-hash lockstep stamp, editor hard-refuses mismatch (Slang-drift / toolchain-swap class of failure — must be loud). Ship Debug AND RelWithDebInfo variants (no cross-config BMI bets).
- `.gseproj` `[engine] version = <n>` selects a packaged SDK instead of a source tree.
- Inno Setup per-user installer: install dir, projects-dir creation, Start Menu, `.gseproj` association. Treat install dir as read-only regardless.
- Updates v1: installer replaces editor+SDK+toolchain as one matched unit; projects record engine version, rebuild prompt on mismatch.

## Sharp edges

0. **UI text commands are views into a per-frame pool.** The renderer consumes them off an ECS channel backed by `double_buffer<std::vector<T>>` — i.e. **one frame of latency**. The pool must therefore rotate (`per_frame_resource`, N=2), never be reset in place. Getting this wrong corrupts text every other frame, and because `assign` reuses the buffer it manifests as fragments of one string welded to another ("Libpng.cp", "#includecppm") rather than a crash. Anything that builds a `text_command` outside `queue_text` must intern first — there are three such sites in Gui.cpp (tooltip, tab title, context-menu label). If the channel ever gains depth, N must follow.

1. ~~SDK export of GCC C++-module BMIs is the critical-path risk~~ — **downgraded 2026-07-24.** With engine-source-by-default this is confined to phase 5 (distribution) and no longer gates daily work or any earlier phase. Hand-rolled SDK remains the proven-viable fallback.
2. Toolchain↔SDK lockstep enforced by stamp, never assumed.
3. Ship both Debug and RelWithDebInfo SDK variants; installed footprint likely 1-2 GB incl. toolchain.
4. `import std`: each project's first configure builds std.gcm once; pre-sharing from SDK is a later optimization.
5. Multi-instance: `.gse/` isolates per-project state; user-global writes are last-write-wins (layout_store atomic rename) — accepted v1.
6. SmartScreen/AV: unsigned installer shipping a compiler will eventually trip heuristics; code signing is the eventual fix.
7. Spaces in user-chosen paths: quoting discipline in build runner, plugin args, module mapper; phase-3 test matrix item.
   - **Path LENGTH is a harder limit than spaces — measured 2026-07-24.** The spike initially failed with `ar: error reading .../std.cc.obj: No such file or directory`, because CMake's per-target hashed object directories (e.g. `CMakeFiles/__cmake_cxx_std_26.dir/7af8aa8ecf477d1b29e070821960b954/`) pushed object paths past Windows' 250-char `CMAKE_OBJECT_PATH_MAX`. Configure warns about this but the build fails much later and the message names `ar`, not the real cause. Re-running from a 44-char root built cleanly. So the `%USERPROFILE%\GSEProjects` default is short **and** space-free on purpose, and `New Project` should reject or warn on deep destinations. `import std` makes this worse — `__cmake_cxx_std_26` is one of the longest generated target names.
8. Game code needs a sanctioned project-data write API (checkpoint files etc.).
9. **A setting that lives only in `CMakePresets.json` does not ship with the engine.** Found 2026-07-27 building the first out-of-tree project: `CMAKE_CXX_MODULE_STD`, `-mavx2` and `CMAKE_EXPORT_COMPILE_COMMANDS` were preset-only, so external configure produced a build with no `std` module target (fatal at the first `export import std;`, in `vulkan.cppm`), no AVX2 (`SIMD.cppm` → `inlining failed in call to always_inline '_xgetbv': target specific option mismatch`, preceded by a `-Wpsabi` "AVX vector return without AVX enabled" warning), and no `compile_commands.json` (silent — the editor's index and analysis for that project just come up empty). Anything the engine *requires* to compile belongs in `GSEEngine.cmake`; the lifted-cache list in `build_runner::configure_command` is only for reproducing *this machine* (toolchain, triplet, compilers, build type). The in-tree build cannot catch this class of bug — the preset masks it.
   - **Fix it with `add_compile_options` or a `FORCE`d cache entry — never `CMAKE_<LANG>_FLAGS_INIT`, and never a bare `set(... CACHE ...)`.** Both failed here, the same way, and neither failed loudly. `_INIT` is consumed once, when `CMAKE_CXX_FLAGS` is first written to the cache, and is silently ignored on every later configure of that build dir. A non-`FORCE` `set(... CACHE ...)` loses to *any* existing entry — including one CMake declared itself, which is what happens with `CMAKE_EXPORT_COMPILE_COMMANDS` (pre-declared, empty). Since `ensure_configured()` skips configure entirely when `build.ninja` exists, a fix that only applies to virgin build dirs reaches almost nothing. Assume every consumer's build dir already exists.

## Parked

Project export (shipping a finished game), per-project engine source builds, auto-update/deltas, side-by-side engine versions, broader asset pipeline, multi-target beyond the phase-6 minimum.
