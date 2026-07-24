# Editor-First Restructure

Status: settled 2026-07-20, not started. Goal: the editor is the installed all-in-one product — it ships the engine as a prebuilt SDK, creates/opens projects from a projects directory, and never writes runtime artifacts into its install dir or the repo.

## Settled decisions

- **Engine consumption**: prebuilt SDK only. Projects compile game code against shipped static libs + BMIs; engine is read-only and version-locked to the editor. Manifest schema leaves room for per-project source builds later.
- **GoonSquad**: stays in-repo through phases 1-5 (becomes the dev-mode project), then physically moves to the projects dir once the SDK path is proven (phase 6). Server moves with it (links GoonSquadLib).
- **Per-project editor state**: everything derived/local lives in `<project>/.gse/`, gitignored.
- **Packaging v1**: per-user Inno Setup installer into `%LOCALAPPDATA%\Programs\GSE`, no UAC. Portable zip falls out of the same package step.

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

### Phase 2 — Project model (editor becomes project-shaped, still mono-build)

- `.gseproj` ini manifest (reuse sectioned-ini machinery; no new parser). argv in Main.cpp (`Editor.exe <path>.gseproj`) — also the file-association hook.
- Project context set at open; repoint: workspace root (EditorApp.cppm:3098), search/index root (SearchSystem.cppm:59), diagnostics compile_commands discovery (EditorApp.cppm:1870), git, build runner roots/targets, layout paths.
- Recent projects in `%APPDATA%`. Settings registry gains user|project scope; two files.
- `Game/` gets a committed `.gseproj` = first project; dev mode opens it by default → day-to-day behavior unchanged while the machinery gets exercised.

### Phase 3 — Engine SDK + external project builds (gated by spike)

- **Spike (gate)**: build a hello-world game TU in a foreign directory against the repo build's BMIs + libs using the shipped toolchain. Path A: hand-generated flags (precedent: SymbolIndexBuilder already compiles standalone TUs against build BMIs — the recipe exists). Path B: CMake `install(TARGETS ... FILE_SET CXX_MODULES)` + BMI install + find_package. Pick by pain. Pass criterion: sample project builds and runs a windowed app from the SDK image with the repo renamed away.
- Package script (scripts/ convention) assembling the SDK image; toolchain-hash lockstep stamp, editor hard-refuses mismatch (Slang-drift / toolchain-swap class of failure — must be loud).
- Generated project CMakeLists template (includes runtime-deps copy mirroring gse_copy_runtime_deps); build runner configures + builds external projects; ship Debug AND RelWithDebInfo engine variants (no cross-config BMI bets).
- Asset system grows multi-root: engine root read-only/prebaked + project Assets/ watched → `.gse/baked/`.
- CI sample project consuming the SDK — doubles as the engine smoke workload after phase 6.

### Phase 4 — Product UX

- Hub/launcher on projectless launch: recent list, New Project (template copy + name substitution + configure), Open.
- Templates (Blank first). Shipped prebuilt engine symbol index in the SDK (package step runs the indexer) + index root layering → instant engine-wide search/xref on fresh installs.

### Phase 5 — Packaging + installer

- Package step produces the relocatable image (portable zip for free). Inno Setup per-user installer: install dir, projects-dir creation, Start Menu, `.gseproj` association. Treat install dir as read-only everywhere regardless (Program Files stays possible).
- Updates v1: installer replaces editor+SDK+toolchain as one matched unit; projects record engine version, rebuild prompt on mismatch.

### Phase 6 — GoonSquad relocation (after SDK proven)

- Move Game/ (+ Server, which links GoonSquadLib) out of the repo into the projects dir as real SDK consumers; locomotion tooling paths follow.
- Requires either minimal multi-target manifest support (game + server) or temporarily parking Server.
- Engine CI smoke switches to the phase-3 sample project. Repo becomes Engine + Editor + Tools.

## Sharp edges

1. SDK export of GCC C++-module BMIs is the critical-path risk — spike gates phase 3; hand-rolled SDK is the proven-viable fallback.
2. Toolchain↔SDK lockstep enforced by stamp, never assumed.
3. Ship both Debug and RelWithDebInfo SDK variants; installed footprint likely 1-2 GB incl. toolchain.
4. `import std`: each project's first configure builds std.gcm once; pre-sharing from SDK is a later optimization.
5. Multi-instance: `.gse/` isolates per-project state; user-global writes are last-write-wins (layout_store atomic rename) — accepted v1.
6. SmartScreen/AV: unsigned installer shipping a compiler will eventually trip heuristics; code signing is the eventual fix.
7. Spaces in user-chosen paths: quoting discipline in build runner, plugin args, module mapper; phase-3 test matrix item.
8. Game code needs a sanctioned project-data write API (checkpoint files etc.).

## Parked

Project export (shipping a finished game), per-project engine source builds, auto-update/deltas, side-by-side engine versions, broader asset pipeline, multi-target beyond the phase-6 minimum.
