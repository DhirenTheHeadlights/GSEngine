# Phase 2 — Project model

Parent: [editor-first-restructure.md](editor-first-restructure.md). Depends on phase 1 ([editor-first-restructure-phase1.md](editor-first-restructure-phase1.md), implemented unbuilt). Status: scoped 2026-07-24, not started. Anchors verified against the worktree that day, post-phase-1.

Goal: the editor opens a **project** rather than being hardwired to this repo. `Editor.exe <path>.gseproj` works, `Game/` becomes the first real project, and per-project state lives in `<project>/.gse/`. Still one mono-build — external project builds are phase 3.

## What phase 1 already did for this

The parent plan's phase-2 bullet list ("repoint workspace root, search/index root, diagnostics compile_commands discovery, git, build runner roots/targets, layout paths") was written against ~10 scattered hardcoded sites. Phase 1 collapsed all of them into two modules. Re-verified today:

| Parent-plan anchor | Reality after phase 1 |
|---|---|
| workspace root hardcoded, EditorApp.cppm:3098 | **one writer**, [EditorApp.cppm:294](../Editor/Editor/Source/App/EditorApp.cppm) `d.ws.root = gse::config::root_dir()` |
| diagnostics discover compile_commands under ws.root, EditorApp.cppm:1870 | **gone** — [CodePanel.cppm:1178](../Editor/Editor/Source/App/CodePanel.cppm) passes `ide::config::compile_commands()` |
| build runner scans `root_dir/out/build` for newest build.ninja | **gone** — [BuildRunner.cppm:174-179](../Editor/Editor/Source/BuildRunner/BuildRunner.cppm) `find_build_dir()` just probes `config::build_dir()` |
| index root, git root, layout paths | all single `config::` calls |

So phase 2 is not a repointing exercise across the editor. It is: **give `gse.ide.config` a project, and everything downstream follows.** The call sites do not need to change again.

## The invariant phase 1 established, and how not to break it

Phase 1's accessors return `const std::filesystem::path&` into a magic-static table. That is only safe because the table is **immutable after first resolution** — and worker threads rely on it: the search index, diagnostics runner and build runner capture those references and outlive any single frame ([BuildRunner.cppm:479,544](../Editor/Editor/Source/BuildRunner/BuildRunner.cppm) bind `const path&` directly).

Making the table re-resolvable when a project opens would dangle every one of those references. Two ways out:

- **(a) Project is fixed for the process lifetime.** Chosen at startup from argv; switching projects relaunches the editor. Table stays immutable, zero call sites change, no threading question. The relaunch machinery already exists — `app::relaunch_on_exit` is used by the build runner's self-rebuild ([BuildRunner.cppm:618](../Editor/Editor/Source/BuildRunner/BuildRunner.cppm)).
- (b) Mutable table behind `shared_ptr<const resolved>` snapshots. Accessors stop returning references, all 39 editor call sites get touched again, and every worker needs snapshot discipline.

**Take (a).** It preserves the phase-1 contract, and "switching project restarts the IDE" is normal behavior. Phase 4's hub relaunches with the chosen path instead of hot-swapping.

## Design

### `.gseproj`

Sectioned ini, parsed with the existing machinery — `layout_store::read()` plus the editor's `parse_layout_sections` already do sectioned-ini; no new parser.

```ini
[project]
name = GoonSquad
engine_version = 0.1.0

[targets]
game = GoonSquad
server = Server
```

`[targets]` is what removes the hardcoded `game_target` / `editor_target` constants that phase 1 left as literals. Unknown keys ignored, same forward-compatibility rule as `gse.manifest`.

New module `gse.ide.project` (small: manifest struct + load + resolution order). It reads the file; `gse.ide.config` consumes the result.

### Project resolution order (at startup, before `gse::start`)

1. argv — `Editor.exe <path>.gseproj`. Use `GetCommandLineW` (already re-exported, [Win32.cppm:126](../Engine/Engine/Source/External/Win32.cppm)) rather than narrow `argv`, so non-ASCII project paths survive.
2. most-recent entry in `%APPDATA%\GSE\recent_projects.ini`.
3. dev mode only: `<root_dir()>/Game/GoonSquad.gseproj`.
4. otherwise: fatal for now; the hub replaces this in phase 4.

This must run before `gse::start`, because `engine_config.gui_layout_path` is passed in at that call ([Main.cpp:28](../Editor/Editor/Source/Main.cpp)).

### `gse.ide.config` gains the project half

| Function | Phase 1 (repo) | Phase 2 (project) |
|---|---|---|
| `project_root()` | — | the `.gseproj`'s directory |
| `project_source_dir()` | `game_source_dir()` | `project_root()/Source` |
| `project_assets_dir()` | — | `project_root()/Assets` |
| `project_state_dir()` | — | `project_root()/.gse` |
| `editor_layout()` | `user_config_dir()/editor_layout.ini` | `project_state_dir()/editor_layout.ini` |
| `captures_dir()` (engine) | `user_state_dir()/captures` | `project_state_dir()/captures` |
| symbol cache ([Index.cppm:1469](../Editor/Editor/Source/Search/Index.cppm), next to compile_commands) | build-adjacent | `project_state_dir()/symbols` |
| `game_target` / `editor_target` | literal constexpr | from `[targets]` |
| `build_dir()` / `compile_commands()` | repo build root | **unchanged — still mono-build** |
| `game_source_dir()` | repo `Game/Game` | deleted |

`gse::config::root_dir()` keeps meaning **engine/install root**, not workspace. That separation is the whole point.

### Workspace becomes multi-root — the non-obvious consequence

Today the editor's file tree and search index are rooted at the repo, so you can browse and xref engine source. Point `ws.root` at `<repo>/Game` and you lose the engine entirely — a real regression in daily use, and the thing most likely to make phase 2 feel worse than phase 1.

So the workspace needs two roots: the project (read-write) and the engine source (read-only reference). [Workspace.cppm:95,107](../Editor/Editor/Source/Workspace/Workspace.cppm) has singular `std::filesystem::path root` and `fs_node fs_root`, so this is a genuine structural change — either a root list, or one synthetic parent node with the two as children.

This is worth doing here rather than deferring: the phase-3 SDK ships `Engine/Source/` precisely so it stays browsable and xref-able, so multi-root is required either way. Doing it in phase 2 means the editor never regresses.

Consumers to update once the roots are plural: [EditorApp.cppm:294-296](../Editor/Editor/Source/App/EditorApp.cppm), [CodePanel.cppm:1112,1181](../Editor/Editor/Source/App/CodePanel.cppm) (`relative()` against root, and `workspace_root` passed to the analysis runner), search index root ([SearchSystem.cppm:52,66,69](../Editor/Editor/Source/Search/SearchSystem.cppm)).

### Settings scope split

The largest genuinely-new engine work. `save::registry::set_auto_save` takes a single path ([SaveSystem.cppm:116](../Engine/Engine/Source/Save/SaveSystem.cppm)) and entries carry a `category` but no scope. Phase 2 needs user-scope (`%APPDATA%\GSE\settings.ini`) and project-scope (`<project>/Config/settings.ini`).

Cheapest shape that fits the existing annotation system: a `[[= gse::settings::scope{...}]]` annotation defaulting to user, and the registry keeping two backing files, routing each entry by its scope tag. Load order user-then-project; project wins.

Defer if it turns out to be the expensive part — everything else in phase 2 is independent of it.

## Work steps

1. `gse.ide.project` — manifest struct, sectioned-ini load, resolution order. Standalone and testable before anything consumes it.
2. `Main.cpp` — `GetCommandLineW` parse, resolve project, fatal-with-message when unresolved. Project resolved before `gse::start`.
3. `gse.ide.config` — project half of the table per the table above; delete `game_source_dir`; targets from the manifest.
4. `Game/GoonSquad.gseproj` committed; dev mode opens it by default so day-to-day behavior is unchanged while the machinery gets exercised.
5. Workspace multi-root (project + engine source read-only) and its four consumers.
6. `.gse/` retargets: editor layout, captures, symbol cache. `.gitignore` in the project ignores `.gse/`.
7. Recent projects list in `%APPDATA%\GSE\recent_projects.ini` — reuse `layout_store`.
8. Settings scope split (defer-able).
9. Build runner targets from `[targets]` instead of constants.

Steps 1-4 are the spine; the editor should open `Game/GoonSquad.gseproj` and behave exactly as today before 5-9 start.

## Non-goals

External/per-project builds and the SDK (phase 3 — `build_dir()` and `compile_commands()` deliberately stay repo-mono here), the hub/launcher and templates (phase 4), packaging (phase 5), physically moving `Game/` out of the repo (phase 6), multi-target beyond reading `[targets]`, locomotion checkpoint paths (they follow the game in phase 6).

## Risks

- **Breaking the immutable-table invariant** — the one thing that would force re-touching all 39 phase-1 call sites and introduce a threading problem. Mitigated by fixing the project for the process lifetime.
- **Losing engine browsing** when the workspace narrows to the project — mitigated by doing multi-root in this phase rather than phase 3.
- Symbol index currently derives its cache location from `compile_commands.parent_path()` ([Index.cppm:1469](../Editor/Editor/Source/Search/Index.cppm)); moving the cache to `.gse/symbols` while compile_commands stays repo-mono splits those two apart — check nothing else assumes they are siblings.
- Settings scope split touches the reflection-driven settings registry, which the whole editor and engine depend on; it is the most likely source of broad breakage, hence defer-able.
- Spaces in project paths: quoting discipline in the build runner, plugin args and module mapper. Parent plan lists this as a phase-3 test-matrix item, but argv makes it reachable in phase 2.

## Phase 1 tail found while scoping

[SearchSystem.cppm:69-70](../Editor/Editor/Source/Search/SearchSystem.cppm) still computes `root/Engine/Resources/Misc/log.txt` to exclude the log file from the file watcher. Phase 1 moved the log to `%LOCALAPPDATA%\GSE\logs`, so that exclusion is now dead code. Harmless, but it should go.
