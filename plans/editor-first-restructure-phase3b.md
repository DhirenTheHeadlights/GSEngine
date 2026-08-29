# Phase 3b — Relocate GoonSquad, extract locomotion

Parent: [editor-first-restructure.md](editor-first-restructure.md). Status: **DONE 2026-07-31.** Anchors measured against the working tree the day it was scoped; the move landed the same day.

The demo did not keep the `GoonSquad` name — `Game/` became `Sandbox/`, targets `Sandbox` / `SandboxServer`, manifest `Sandbox.gseproj`. Step 4 below anticipated deferring that rename; it happened with the move instead, which is cheaper than doing it twice.

Phase 3a delivered the capability (a project names an engine and builds out of tree, proven by `BasePlanningTool`). 3b is the file move that makes it real: the repo becomes Engine + Editor + Tools, and every worktree stops carrying a copy of the game.

## Measured current state

`Game/Game/Source`, 11,656 lines:

| Area | Files | Lines | Share |
|---|---:|---:|---:|
| `Locomotion/` | 12 | 7,093 | **61%** |
| `Shared/` | 10 | 1,910 | 16% |
| `Sandbox/` | 3 | 1,427 | 12% |
| `Ui/` | 4 | 616 | 5% |
| top level | 6 | 610 | 5% |

- Root `CMakeLists.txt` couples to the game in exactly **one line**: `add_subdirectory(Game)`.
- **Zero** `GoonSquad` references anywhere in `Engine/` or `Editor/` (only a stale string in `Engine/Resources/Misc/log.txt`). The phase-3 verification still holds.
- Targets: `GoonSquadLib` (library), `GoonSquad` (exe), `GoonSquadServer` (exe, links `EngineServer`). `Game/GoonSquad.gseproj` already exists with `[engine] source = ..` and `[targets] game/server`.
- `datasets/` is 11 MB of mocap, **untracked and not gitignored** — unversioned data sitting loose in the repo root.
- `docs/LOCOMOTION.md` is the live tracker.

## The locomotion question — decide this first

Locomotion is not separable the way Server was. The Server split worked because `gse.server` had zero `gs::` references and was already engine-namespaced. Locomotion is the exact opposite:

```
export module gs:balance_controller;      namespace gs::locomotion
```

Twelve **partitions of the `gs` module itself**. Extracting it is a module-identity change, not a file move alone.

Two facts make it tractable anyway:

1. **Locomotion imports nothing from the game.** Only `gse`, `std`, and its own partitions. The dependency runs game → locomotion; five files consume it (`Client.cpp`, `Main.cpp`, `Sandbox/SandboxScene.cppm`, `Shared/Player.cppm`, `WorldLoader.cppm`).
2. **It splits cleanly by role, and the two halves do not reference each other.** Measured partition graph:

| Group | Files | Lines | Imports |
|---|---|---:|---|
| **Controller** | Types, LegIK, LegController, GaitScheduler, BalanceController, FootstepPlanner, StateEstimator, SmokeTest | 2,734 | only `locomotion_types` (and `leg_ik`) |
| **Training** | LocomotionNn, LocomotionMdp, LocomotionRecorder, LocomotionTrainer | 4,359 | `locomotion_types`, `mdp`, `nn`, `recorder` |

No controller file imports a training file; no training file imports a controller. They share only `Types.cppm` (203 lines). The training loop reaches the controller through ECS components rather than imports, which is why the module graph is already clean.

### Settled 2026-07-31 — the humanoid leaves entirely

The role split is **not** the shape. Owner's decision: the engine repo ships **only the physics stress sandbox**, driven by a free camera. No humanoid, so no reason to promote the controller into the engine — the whole locomotion effort plus the player moves to `%USERPROFILE%\GSEProjects\HumanoidLocomotion` (created and `git init`-ed 2026-07-31).

This is simpler than promoting a controller target, and it means `EngineLocomotion` should **not** be created. The engine keeps no character code at all.

**Moves out** (≈8,413 lines):

| | Lines |
|---|---:|
| `Locomotion/` (12 files) | 7,093 |
| `Shared/HumanoidSkeleton.cppm` | 608 |
| `Shared/Player.cppm` | 326 |
| `Shared/SkeletonSpawn.cppm` | 184 |
| `Shared/PoseDriver.cppm` | 112 |
| `Shared/TestSkeletons.cppm` | 90 |

**Stays** as the physics stress sandbox: `Sandbox/DevSpawnSystem`, `Sandbox/RuntimeSpawns`, `Sandbox/SandboxScene`, and the prop/builder half of `Shared/` — `EntityBuilders`, `OrbitCamera`, `Tumbler`, `Piston`, `ControlledJoint`.

### The edges that must be cut

The sandbox currently depends on the humanoid cluster, so this is a decomposition, not only a move. Measured import edges into the departing set:

| Consumer | Imports that must go | Cost |
|---|---|---|
| `Sandbox/RuntimeSpawns.cppm` | `humanoid_skeleton`, `skeleton_spawn`, `test_skeletons` | real — loses skeleton spawning, keeps its `entity_builders` / `piston` / `tumbler` half |
| `Sandbox/SandboxScene.cppm` | `humanoid_skeleton`, `locomotion_types`, `player` | real — loses the humanoid and player, drives with a free camera |
| `Client.cpp` | `balance_controller`, `footstep_planner`, `gait_scheduler`, `leg_controller`, `player`, `pose_driver`, `state_estimator` | **mechanical** — 71-line registration file; delete six imports and three `system_manifest` blocks |

`Client.cpp` looked like the heaviest consumer from its import list and is the lightest: it only *names* systems for registration. Removing the departing ones leaves `orbit_camera`, `tumbler`, `piston`, the network seeding, and `gse::free_camera::system` — **which it already registers at line 34.** The free camera the sandbox needs is therefore already wired; `SandboxScene` just has to stop spawning a player on top of it.

### Settled — networking infra stays for now

The multiplayer half (`Client.cpp`/`Client.cppm`, `WorldLoader.cppm`, `GameUI.cppm`, `Ui/*`, `ServerMain.cpp`, `GoonSquadServer`) **stays in the engine repo** and leaves later with a multiplayer-first project. It only has to compile without the humanoid cluster, which the table above shows is a mechanical edit.

Note the engine already owns the networking *infrastructure* — `Engine/Engine/Source/Network/` (sockets, replication, discovery, bitstream, messages). What stays in `Game/` is the game's *use* of it, which is what the future project will take.

### Amendment to the parent plan

The parent says "Repo becomes Engine + Editor + Tools." **Superseded:** the physics stress sandbox stays in-repo as the engine's shipped demo, and the multiplayer code stays with it for now. The repo becomes Engine + Editor + Tools + a demo, and only the humanoid/locomotion cluster leaves in this phase.

## Work steps

1. **Cut the sandbox free of the humanoid first, in place.** Strip skeleton spawning from `RuntimeSpawns`, and swap `SandboxScene`'s humanoid + player for `Examples/Object/FreeCamera`. Do this before moving anything: it is the only step with real design content, and it is far easier to iterate while the tree still builds as one unit.
2. **Humanoid cluster → `HumanoidLocomotion`.** All of `Locomotion/` plus the five `Shared/` files above, delivered as a **file move** with new module identity (`gs:` partitions → the project's own module), never an in-place rename. `docs/LOCOMOTION.md` and `datasets/` move with it.
3. **Strip the departing registrations from `Client.cpp`** — six imports and three `system_manifest` blocks. Everything else there stays.
4. **`Game/` stays in-repo** as the engine's demo, so `add_subdirectory(Game)` and `Game/GoonSquad.gseproj` are unchanged. Worth renaming the demo away from `GoonSquad` once the multiplayer project claims that name, but not in this phase.
5. **Sanctioned project-data path** for `checkpoint_path = "locomotion_checkpoint.bin"` (`HumanoidLocomotion/Source/Locomotion/LocomotionTrainer.cppm:30`), which wrote to CWD — sharp edge #8, and it becomes visible the moment the trainer runs from its own project directory. **DONE — see below.**

## Project-data writes (2026-07-31)

Sharp edge #8 closed. It was two bugs that hid each other, and neither would have failed loudly:

- `gse::config` had no notion of the project a running game belongs to, so game code had nothing to write *to*.
- The editor launched the game with its cwd set to `gse::config::root_dir()` — **the editor's own tree**. So a relative game write landed in the engine repo, which is precisely what this restructure exists to stop, and a training run from a project would have dirtied the engine checkout.

Shape of the fix:

- `gse.manifest` gains a `project` key beside `mode` and `root`, written by `gse_write_manifest()`. It defaults to `CMAKE_SOURCE_DIR`, which is already correct for every out-of-tree project. The engine repo's own build tree holds the engine *plus* a game beside it, so its root `CMakeLists.txt` passes `Sandbox` explicitly — without that the in-tree demo would resolve its project to the engine root.
- `gse::config::project_root()`, `project_data_dir()` (`<project>/.gse/data`, already gitignored everywhere) and `project_data_path(relative)`, which passes an absolute path through untouched and resolves a relative one under the data dir. `GSE_PROJECT_DIR` overrides, matching `GSE_USER_DIR` / `GSE_STATE_DIR`. Falls back to `root` when the manifest predates the key, so an unconfigured build tree behaves as before rather than resolving to nothing.
- The trainer routes `checkpoint_path` and `state_path` through it at all five call sites and logs the **resolved** path, so where a checkpoint went is visible rather than inferred. `cfg` is not mutated — it is a settings-registry struct, and writing an absolute machine path back into `settings.ini` would make the project non-portable.
- `checkpoint_save` / `checkpoint_save_full` create their parent directory; nothing else would have created `.gse/data` on a fresh clone.
- Build and launch cwds now follow the tree being built: `run_build_with_module_recovery` takes an explicit `source_dir` (project for a game build, engine for the editor's self-rebuild) and `launch_game_attached` runs the game from `project_root()`.

A side effect worth knowing: relative *read* paths in game code now resolve against the project too, so `reference_clip_path = datasets/mocap/cmu/...` works as written from the project root without absolutising it.

## Hazards

- **Module-identity change must ship as a file move.** In-place renames produce the phantom-cycle failure; the implementation unit must not re-import a partition its primary export-imports. Do not wipe the build dir to "fix" it.
- **The `gs` primary module loses 12 partitions.** Its export graph shrinks; expect the primary interface to need edits, not just the movers.
- **`datasets/` is 11 MB and untracked.** Moving it is invisible to git — it will silently not follow a `git mv`, and it is not currently gitignored either, so it is one `git add .` away from being committed to the wrong repo.
- ~~**Dev-default project discovery goes empty.**~~ **Did not happen** — `discover()` scans one level below the engine root for `*.gseproj` and `Sandbox/Sandbox.gseproj` stayed, so the dev default still resolves. This hazard was written against the version of the plan where the whole game left; it becomes live again only if the demo ever moves out.
- **Both projects need engine bindings.** Use `[engine] name = GSEngine` with `source` as the fallback, per the registry added in 3a's notes.
- **Cross-project symbol reuse is already in place**, so the second and third projects will not pay a cold engine index — verified 366/366 key matches.

## Verification

- `Sandbox`, `SandboxServer` and the locomotion trainer all build and run from their new locations, launched from the editor.
- `grep -r GoonSquad Engine/ Editor/` stays empty — confirmed; the only hits are stack frames in the committed `Engine/Resources/Misc/log.txt`, which is a stale captured log and should go.
- Both projects open from the launcher's recent list, and the engine binding resolves by name with the registry — confirmed: `HumanoidLocomotion` and `BasePlanningTool` both carry `[engine] name = GSEngine` with `source` as fallback.
- A locomotion training run writes its checkpoint inside its own project, not the CWD it happened to launch from. Nothing to migrate: no `locomotion_checkpoint.bin` or `locomotion_train_state.bin` exists in either tree, so the first run under the new resolution starts clean.
