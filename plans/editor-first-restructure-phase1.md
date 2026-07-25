# Phase 1 — Relocatable paths + artifact hygiene

Parent: [editor-first-restructure.md](editor-first-restructure.md). Status: **DONE — built and running 2026-07-24.** Steps 1-6 and 8-10 implemented; step 7 dropped. One build failure along the way (`executable_stem()` called unqualified from `gse::log` scope in Log.cpp), fixed. See [Execution log](#execution-log) for what changed against the plan as written.

Goal: `gse.config` / `gse.ide.config` stop being configure_file-baked absolute-path constants and become runtime-resolved functions driven by a `gse.manifest` marker file; every runtime writer moves out of the source tree (user config → `%APPDATA%\GSE`, machine state → `%LOCALAPPDATA%\GSE`). Binaries become relocatable; the repo is never dirtied at runtime.

Non-goals (deferred): project model/argv (phase 2), symbol-cache relocation (stays next to compile_commands.json — already build-adjacent, phase 2), locomotion checkpoint paths (training workflow, phase 2/6 — its gitignore lines stay), captures moving to `<project>/.gse` (phase 2; phase 1 parks them in state dir), any packaging.

## Deltas found on the 2026-07-24 re-verification

Four things changed under the original plan. Each is folded into the sections below; listed here so the diff against the 07-20 text is legible.

1. **The file log sink is not dormant.** [Log.cpp:415](../Engine/Engine/Import/Log.cpp) pushes `file_sink(log_file_path(), log_files_kept)` from `logger`'s constructor, with `log_files_kept = 5` rotation already implemented ([Log.cpp:43](../Engine/Engine/Import/Log.cpp)). `Engine/Resources/Misc/log.txt` is being written today. The work is a *retarget*, not a wiring — and `logger instance;` ([Log.cpp:174](../Engine/Engine/Import/Log.cpp)) is a namespace-scope static, so path resolution now happens during **static init, before `main`**. See [Log retarget](#step-6--log-retarget).
2. **`editor_layout_path()` moved** to [Layout.cppm:49](../Editor/Editor/Source/App/Layout.cppm) in the EditorApp split.
3. **`gse.ide.config` grew four entries** the original table predates: `game_executable`, `editor_executable`, `game_target`, `editor_target`.
4. **The editor's two layout paths are the same file.** [Main.cpp:28](../Editor/Editor/Source/Main.cpp) sets `.gui_layout_path = ide::config::resource_path / "editor_layout.ini"` and [Layout.cppm:50](../Editor/Editor/Source/App/Layout.cppm) returns the same path. Both flow through the sectioned `layout_store`; they must be retargeted **together** or the section owners split across two files.

Also confirmed: `EDITOR_BUILD_DIR = ${CMAKE_BINARY_DIR}/` ([Editor/CMakeLists.txt:15](../Editor/CMakeLists.txt)), so the `build_dir()`-derivation chain is safe — the 07-20 risk item is closed. Call-site count is **72**, not ~59.

## Design

### Marker file: `gse.manifest`

One walk-up algorithm for both modes. From the exe's directory, walk upward (bounded, ≤8 levels); the first directory containing `gse.manifest` is `build_root`. Parse `key = value` lines with a tiny hand parser (config must stay near-leaf — no fs/ini machinery); unknown keys ignored (forward-compatible for phase 3/5 `version`/`toolchain_hash`).

- Dev: root CMakeLists writes `${CMAKE_BINARY_DIR}/gse.manifest` at configure time — `mode = dev`, `root = <CMAKE_SOURCE_DIR>`. Exe at `out/build/<preset>/Editor/Editor.exe` → one level up finds it. Same for `Game/GoonSquad.exe`, `Server/`.
- Installed (phase 3/5 writes this one): `mode = installed` at image root; `root` = the manifest's own directory.
- No manifest found → fatal with a clear stderr message (cannot log — logging routes through paths; no recursion). Only reachable by running a bare exe from a random location.

Baking the source-root path into a *data file in the build dir* is fine — build dirs are machine-local by nature; the *binary* carries nothing.

### Path table (engine, module `gse.config` — same module name, constants become functions)

| Function | Dev | Installed |
|---|---|---|
| `mode()` | `run_mode::dev` | `run_mode::installed` |
| `root_dir()` | manifest `root` (repo) | manifest dir (image root) |
| `build_root()` | manifest dir (preset build root) | = `root_dir()` |
| `source_dir()` | `root/Engine/Engine` | `root/Engine/Source` |
| `resource_path()` | `root/Engine/Resources` | `root/Engine/Resources` |
| `baked_resource_path()` | `build_root/Engine/Resources` (current behavior) | `root/Engine/Baked` |
| `user_config_dir()` | `GSE_USER_DIR` env, else `%APPDATA%\GSE` | same |
| `user_state_dir()` | `GSE_STATE_DIR` env, else `%LOCALAPPDATA%\GSE` | same |
| `logs_dir()` / `cache_dir()` / `crash_dir()` / `profile_dir()` / `captures_dir()` | `user_state_dir()` + `logs`/`cache`/`crash`/`profile`/`captures` | same |

All return `const std::filesystem::path&` to function-local statics behind one lazily-resolved internal struct (magic-static, thread-safe). Reference return is load-bearing: [BuildRunner.cppm:479,544](../Editor/Editor/Source/BuildRunner/BuildRunner.cppm) bind `const path&` to these. Getters are pure — directory creation stays at the writers (they all `create_directories` already). `warm_up()` exported and called first thing in `Engine::initialize`.

### Editor (`gse.ide.config` primary; `export import :config_system` partition untouched)

| Function | Derivation |
|---|---|
| `source_dir()` | `gse::config::root_dir() / "Editor/Editor"` |
| `game_source_dir()` | `root_dir() / "Game/Game"` (phase 2 removes) |
| `resource_path()` | `root_dir() / "Editor/Resources"` |
| `build_dir()` | `gse::config::build_root()` |
| `compile_commands()` | `build_dir() / "compile_commands.json"` |
| `token_plugin()` | `build_dir() / "Editor" / "gse_tokens.dll"` |
| `cppref_index()` | `resource_path() / "cppref.idx"` |
| `game_executable()` | `build_dir() / "Game" / (game_target + ".exe")` |
| `editor_executable()` | `build_dir() / "Editor" / (editor_target + ".exe")` |
| `game_target` / `editor_target` | stay `constexpr std::string_view`, literals in source — nothing to resolve |

Identical composition works installed: the image mirrors `<build>/Editor` for the dll and `<build>/Game` for the exe.

### Win32 additions

`gse.config` gains `import std; import gse.win32;` (plain imports, not `export import`). This is the proven `Win32Environment.cppm` pattern — the STL-with-windows.h trap only bites a module that *itself* includes windows.h; importing `gse.win32` is safe. Cycle-checked: `gse.win32` imports nothing; `gse.fs`/`gse.log` interfaces don't import `gse.config` (only `Log.cpp`, an impl unit, does — impl units are leaves).

In `Win32.cppm` (which re-exports raw API via `using ::`, no `import std`):
- `#include <shlobj.h>` in the GMF; `using ::SHGetFolderPathW;`. Use the CSIDL API, not `SHGetKnownFolderPath` — it fills a caller `wchar_t[MAX_PATH]` buffer (fits the module's buffer-driven style) and avoids ole32 + `CoTaskMemFree` + FOLDERID GUID linkage entirely. AppData paths >260 chars are not a real concern.
- CSIDL values are macros — macros don't survive `using`; re-export as `constexpr` ints (`csidl_appdata`, `csidl_local_appdata`, `shgfp_type_current`), matching the existing constexpr block at [Win32.cppm:51-69](../Engine/Engine/Source/External/Win32.cppm).
- `GetModuleFileNameW` is already re-exported ([Win32.cppm:114](../Engine/Engine/Source/External/Win32.cppm)).
- Env overrides use `std::getenv` from inside `gse.config` — no additional Win32 re-export needed.
- Engine/CMakeLists.txt link libs (:125 GNU+WIN32 block): add `shell32`.

### Module-conversion mechanics (the careful bit)

Module identity does NOT change (still `gse.config`/`gse.ide.config`) — this is content replacement delivered as a natural file-move (generated build-dir file → source-tree file), which is the safe shape per the module-rename scar tissue. Never wipe the build dir.

**Stale-file guard ordering (new).** Both projects glob the binary dir into the module file set — [Engine/CMakeLists.txt:8](../Engine/CMakeLists.txt) and [Editor/CMakeLists.txt:30](../Editor/CMakeLists.txt) — and existing build dirs still contain the old generated `Config.cppm`, which would be collected alongside the new source file → duplicate module definition. The `file(REMOVE ...)` guard must therefore sit **above the glob**, not next to the deleted `configure_file` block (Engine's is at :180, well after its glob). CMake evaluates top-to-bottom; get this backwards and the guard does nothing on the very reconfigure that needs it.

Gotcha checklist for the new module code: no local lambdas stored in module fn bodies (hoist to file-scope fns), no exported std type-erasure, no recursive consteval, no `constexpr` unit-typed variables at namespace scope, designated init, no comments, single-space style.

---

## Execution order

Ten steps. Two gates; only Gate B needs a build go-ahead.

### Step 0 — pre-flight (no edits)

Confirm the two files newer than the last link ([Index.cppm](../Editor/Editor/Source/Search/Index.cppm), [Annotations.cppm](../Engine/Engine/Source/Meta/Annotations.cppm)) are settled work, not an in-flight edit that will collide. Baseline is otherwise built: `RelWithDebInfo/Editor/Editor.exe` @ 07-24 10:32, module set @ 07-23 20:29. Never wipe the build dir at any point in this phase.

### Step 1 — Win32 surface

`Engine/Engine/Source/External/Win32.cppm`
- GMF: `#include <shlobj.h>` after `<tlhelp32.h>` (:9).
- Exported: `using ::SHGetFolderPathW;` beside `GetModuleFileNameW` (:114).
- Constexpr block (:51-69): `csidl_appdata`, `csidl_local_appdata`, `shgfp_type_current`.

`Engine/CMakeLists.txt:125` — append `shell32` to the GNU+WIN32 link list.

**Gate A** (cheap, no full build): scratch mirror-TU — a standalone module TU reproducing the GMF + the three constexpr re-exports, compiled with the same flags and linked against shell32. Validates the macro→constexpr conversion and that `shlobj.h` drags in nothing that breaks the module. `gse.win32` has no `import std`, so the "no STL templates with Win32 headers" trap does not apply here.

### Step 2 — engine `gse.config`

New `Engine/Engine/Import/Config.cppm`; delete `Config.cppm.in`. Shape:

- `export module gse.config;` / `import std;` / `import gse.win32;`
- `export enum class run_mode { dev, installed };`
- File-scope (non-exported) helpers: `exe_dir()` via `GetModuleFileNameW` into `wchar_t[MAX_PATH]`; `find_manifest(dir)` walking up ≤8; `parse_manifest(path)` hand parser for `key = value`; `known_folder(int csidl)` wrapping `SHGetFolderPathW(nullptr, csidl, nullptr, shgfp_type_current, buf)`; `env_or(name, fallback)` over `std::getenv`.
- One internal `struct resolved` holding every path; one `const resolved& table()` magic static.
- Exported getters per the path table, each `return table().x;`.
- `export auto warm_up() -> void { (void)table(); }`
- Fatal path: `std::fwprintf(stderr, ...)` + `std::abort()` — no logging (recursion).

### Step 3 — engine CMake

`Engine/CMakeLists.txt`
- Insert `file(REMOVE "${CMAKE_CURRENT_BINARY_DIR}/Engine/Import/Config.cppm")` **above** the glob at :8.
- Delete :174-184 (the four `set()`s + `configure_file`).
- Leave the binary-dir glob and `BASE_DIRS` entries alone — harmless once empty, and removing them is unrelated churn.

Root `CMakeLists.txt` — after `project(GSEngine)` (:19), before the `add_subdirectory` block (:81-84):
```
file(WRITE "${CMAKE_BINARY_DIR}/gse.manifest" "mode = dev\nroot = ${CMAKE_SOURCE_DIR}\n")
```

Side benefit: this removes a latent ordering bug. Today the glob at :8 runs *before* the `configure_file` at :180, so a freshly-created build dir only picks up the generated `Config.cppm` on its second configure.

### Step 4 — engine mechanical sweep (33 sites, `config::x` → `config::x()`)

Semantics identical in dev. By file:

| File | Sites |
|---|---|
| [AssetSystem.cppm](../Engine/Engine/Source/Assets/AssetSystem.cppm) | 181, 216, 243, 246, 264, 273, 275, 338, 339 |
| [PipelineBuilder.cpp](../Engine/Engine/Source/GpuRecord/PipelineBuilder.cpp) | 152, 269, 279 |
| [Gui.cpp](../Engine/Engine/Source/Graphics/2D/Gui/Gui.cpp) | 170, 399, 675, 679 |
| [FontCompiler.cppm](../Engine/Engine/Source/Graphics/2D/FontCompiler.cppm) | 249, 265 |
| [Font.cpp](../Engine/Engine/Source/Graphics/2D/Font.cpp) | 77 (member-init), 96 |
| [Model.cppm](../Engine/Engine/Source/Graphics/3D/Model.cppm) | 60 (member-init), 102 |
| [Texture.cpp](../Engine/Engine/Source/Graphics/2D/Texture.cpp) | 19 (member-init) |
| [Audio.cppm](../Engine/Engine/Source/Audio/Audio.cppm) | 29 (member-init) |
| [Game/Main.cpp](../Game/Game/Source/Main.cpp) | 62 — `locomotion_artifact()`, stays on `resource_path()/"Misc"` per non-goals |

Member-init lists and the `ProfileAggregator` default args (step 5) are fine with reference-returning functions; the default-arg temporaries already behave this way today.

### Step 5 — engine writer retargets (7 sites)

| Site | New target |
|---|---|
| [Engine.cpp:55](../Engine/Engine/Source/Runtime/Engine.cpp) settings.ini | `user_config_dir() / "settings.ini"` |
| [Gui.cppm:84](../Engine/Engine/Source/Graphics/2D/Gui/Gui.cppm) default | absolute `user_config_dir() / "gui_layout.ini"`; drop the `config::resource_path /` composition at the four Gui.cpp sites (path is now always absolute). Editor's `gui_layout_path` override keeps working |
| [Aftermath.cppm:108,112](../Engine/Engine/Source/Vulkan/Aftermath.cppm) | `crash_dir()` and `crash_dir() / "shaders"`; eager caching into `dumps` at session setup unchanged |
| [ProfileAggregator.cppm:63,67](../Engine/Engine/Source/Diag/ProfileAggregator.cppm) | `profile_dir() / "profile.txt"` / `"profile.json"` |
| [CaptureRenderer.cpp:190,283,359](../Engine/Engine/Source/Graphics/Renderers/CaptureRenderer.cpp) | `captures_dir() / "screenshots"|"recordings"|"clips"` |

Note on gui_layout: any engine app using the default shares this file until phase 2 project-scopes it — acceptable, only GoonSquad hits it.

### Step 6 — log retarget

[Log.cpp:177-179](../Engine/Engine/Import/Log.cpp): `log_file_path()` returns `logs_dir() / (exe_stem + ".log")`, exe stem from `GetModuleFileNameW` so editor/game/server don't fight over one file. Keep `file_sink` and the existing `log_files_kept = 5` rotation — that is already the retention story; `json_sink` stays additive and out of phase 1.

**Ordering decision:** `logger instance;` is a namespace-scope static, so its ctor resolves `gse.config` during static init, before `main` — `warm_up()` in `Engine::initialize` will never actually be first. Accept this: `GetModuleFileNameW`, `SHGetFolderPathW` and filesystem reads are all valid during static init (kernel32/shell32 come in via the import table), and a missing-manifest fatal still reaches stderr pre-main. `warm_up()` stays as the first line of `Engine::initialize` ([Engine.cpp:47](../Engine/Engine/Source/Runtime/Engine.cpp), above `trace::start`) as a net for any future non-logging entry point. The alternative — deferring the file sink to `Engine::initialize` — was rejected because it drops log capture for everything before that point.

### Step 7 — dropped

No legacy migration. Cut on Dhiren's call 2026-07-24: settings, gui layout and editor layout all regenerate from defaults on first run in the new locations. Any hand-tuned `settings.ini` is re-created by hand at `%APPDATA%\GSE\settings.ini`.

### Step 8 — editor `gse.ide.config` + CMake

New `Editor/Editor/Import/Config.cppm` per the editor table; delete `Config.cppm.in`. `export import :config_system` line survives verbatim.

`Editor/CMakeLists.txt`
- `file(REMOVE "${CMAKE_CURRENT_BINARY_DIR}/Editor/Import/Config.cppm")` **above** the glob at :30.
- Delete :12-28 (the twelve `set()`s + `configure_file`), keeping `EDITOR_SEMANTIC_CONTRACT` (:18) — it feeds `list(REMOVE_ITEM)` at :39, not the config template.

### Step 9 — editor sweep + coupled layout retarget (39 sites)

| File | Sites |
|---|---|
| [BuildRunner.cppm](../Editor/Editor/Source/BuildRunner/BuildRunner.cppm) | 176, 179, 183, 185, 186, 190, 191, 447, 479, 506, 541, 544, 556, 557, 586, 605, 618, 651, 652 |
| [SearchSystem.cppm](../Editor/Editor/Source/Search/SearchSystem.cppm) | 52, 55, 56, 59, 60, 66, 69 |
| [Terminal.cpp](../Editor/Editor/Source/Terminal/Terminal.cpp) | 126, 267, 288 |
| [CodePanel.cppm](../Editor/Editor/Source/App/CodePanel.cppm) | 1172, 1173, 1178, 1787 |
| [EditorApp.cppm](../Editor/Editor/Source/App/EditorApp.cppm) | 294, 300 |
| [GitSystem.cppm](../Editor/Editor/Source/Git/GitSystem.cppm) | 43 |
| [Layout.cppm:50](../Editor/Editor/Source/App/Layout.cppm) + [Main.cpp:28](../Editor/Editor/Source/Main.cpp) | **retarget together** → `gse::config::user_config_dir() / "editor_layout.ini"` |

`config::editor_target` / `config::game_target` comparisons stay as-is (still constexpr string_views).

### Step 10 — hygiene

- `.gitignore`: drop `*.ini`, `*.log`, `/Engine/Resources/Screenshots`, `/Engine/Resources/Recordings`. KEEP `/Engine/Resources/Misc` until locomotion artifacts move (phase 2/6) — add a comment saying it survives only for those.
- One-time dev cleanup: delete stale `Editor/Resources/editor_layout.ini`, and `Engine/Resources/{Misc (non-locomotion files), Screenshots, Recordings, Clips}` leftovers.
- AGENTS.md: update the log-path note (currently `Engine/Resources/Misc/log.txt`), document the new locations and the `GSE_USER_DIR`/`GSE_STATE_DIR` overrides. Call out loudly: **settings.ini now lives at `%APPDATA%\GSE\settings.ini`** — hand-edit workflows (locomotion overrides) go there.

**Gate B** — full reconfigure + build (needs go-ahead), then the validation checklist.

Commit boundaries if wanted: 1-3 (config module + CMake), 4-6 (engine retarget), 8-9 (editor retarget), 10 (hygiene).

## Validation checklist

1. Reconfigure an EXISTING build dir: stale-guard removes the old generated Config.cppm; no duplicate-module error; `gse.manifest` appears at build root. No build-dir wipe.
2. Clear `%APPDATA%\GSE` + `%LOCALAPPDATA%\GSE`; run editor from build tree → files are created fresh from defaults, panels and settings survive a restart, files update in AppData on quit.
3. `git status` + untracked scan clean after a full session (editor + F5 game run + screenshot + profiler dump).
4. Log file appears under `%LOCALAPPDATA%\GSE\logs` named per exe stem; existing 5-file rotation still trims.
5. `GSE_USER_DIR` override redirects; renaming `gse.manifest` away → clean fatal message (expect it pre-main, from the logger's static ctor).
6. Game run: gui_layout lands in AppData, not `Misc/`; capture lands in `state\captures\`.
7. Aftermath: verify `crash_dir()` resolution is warm before renderer init.
8. Editor layout: confirm both section owners still land in one `editor_layout.ini` (delta 4 regression check).

## Risks

- Duplicate `gse.config` from stale generated files — covered by the `file(REMOVE)` guard, **which must precede the glob**; if a phantom import cycle still appears, remedy is the known file-move delivery pattern, never a build-dir wipe.
- CSIDL macro re-export as constexpr — closed at Gate A.
- Path resolution during static init (logger ctor) — accepted deliberately, see step 6.
- Editor layout file split across two paths if step 9's coupled retarget is done piecemeal — checklist item 8.
- Settings and layouts reset to defaults on first run in the new locations (no migration, by choice) — AGENTS.md documents where they moved to.

---

## Execution log

Implemented 2026-07-24, unbuilt. Deviations and refinements against the plan text above:

1. **`gse.config` split interface/impl** (`Config.cppm` declarations + `Config.cpp` bodies), following the `Log.cppm`/`Log.cpp` precedent. The plan said `gse.config` gains `import gse.win32` — it does not: the interface imports only `std`, and `gse.win32` is imported by the impl unit. Its ~20 importers therefore never transitively load win32's CMI. Same split for `gse.ide.config`.
2. **Separator normalization.** The old CMake-baked values were all forward-slash; composing with `operator/` produces backslashes, which would have changed every path string compared against `compile_commands.json`/ninja output. `gse::config::generic()` round-trips through `generic_wstring()` and is applied to every resolved path. It is exported (not module-private) so `gse.ide.config` uses the one implementation rather than a second copy.
3. **`executable_stem()`** added to `gse.config` — the log file is now per-executable (`Editor.log`, `GoonSquad.log`), so editor and game no longer overwrite each other's log.
4. **`ide::config::editor_layout()`** added as a single accessor. This dissolves delta 4 rather than working around it: `Layout.cppm` and `Main.cpp` now both call it, so the two paths cannot drift apart.
5. **Step 6 was path-only.** `file_sink`'s ctor already does `create_directories(path.parent_path())` and `rotate_logs(path, log_files_kept)`, so no directory-creation or retention work was needed.
6. **Step 7 dropped** — no migration, on Dhiren's call.
7. **Trailing slashes are irrelevant on this toolchain.** Measured: `lexically_relative`, `fs::relative` and `operator/` return identical results with and without a trailing separator, so dropping the `.../Resources/` trailing slash is behavior-preserving at the three `lexically_relative` sites.
8. `Engine/Resources/Misc/` still holds the pre-change `settings.ini`, `gui_layout.ini`, logs and profiles. Left in place (gitignored, harmless) rather than deleted — `settings.ini` is hand-tuned and there is no migration. Only the newly-unignored `Editor/Resources/editor_layout.ini` was removed.

### Gate A results

- `shlobj.h` coexists with the existing Win32 GMF; `CSIDL_APPDATA`/`CSIDL_LOCAL_APPDATA`/`SHGFP_TYPE_CURRENT` convert to `constexpr int` (static_asserted); shell32 links; both known folders resolve.
- `gse.config` scratch-compiled as real modules against a stub `gse.win32` and exercised end to end: dev mode, installed mode, `GSE_USER_DIR`/`GSE_STATE_DIR` overrides, CRLF + extra-whitespace + unknown-key parsing, missing trailing newline, and the missing-manifest fatal (clean stderr message, exit 3).
