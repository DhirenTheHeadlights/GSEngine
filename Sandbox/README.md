# Sandbox

The reference project for GSEngine. It is a small playable scene — crates, a character, a sidearm — plus the fixtures and benchmark scenarios the engine's physics is regression-tested against.

## Start here

Four files, in this order, are enough to understand how a GSE game is put together:

| File | What it shows |
|---|---|
| `Sandbox/Source/Main.cpp` | The entry point: parse arguments, pick a run mode, call it. Nothing else. |
| `Sandbox/Source/Startup/RunModes.cpp` | `run_game` — the `gse::start` call and the one callback that registers everything. |
| `Sandbox/Source/Game/GameSystems.cpp` | Every system the game runs, as four named manifests: shared, client, client UI, server. |
| `Sandbox/Source/Worlds/Worlds.cppm` | How a scene is built: spawn archetypes into a `gse::scene`, attach a camera. |

## Layout

```
Source/
  Main.cpp        entry point
  Startup/        run modes, the config aggregate, state-dump CLI tools
  Game/           the game itself: systems, networking, player, camera, sidearm
  Props/          reusable scene content — archetypes, machines, joints, stacks, lights, characters
  Worlds/         the nine demo worlds, their catalogue and their menu diagrams
  Fixtures/       physics-parity fixtures and the parity harness
  Scenarios/      scripted benchmark, showcase and parity runs
  Ui/             HUD, telemetry overlays, menus and screens
  DevSpawn/       F5–F9 dev spawners and their settings
```

`Props/` is layered: `Archetypes` builds component bundles, the other files compose those into props, and `Workloads` composes props into the whole-scene workloads `DevSpawn` triggers.

## Running it

A plain launch opens the main menu and the world catalogue:

```
Sandbox.exe
```

A scenario is a scripted, hermetic run that settles the world, drives it on a fixed-step clock and exits with a profile and a world-state hash:

```
Sandbox.exe --engine-bench-scenario parity_stack_cpu
Sandbox.exe --engine-bench-scenario tumbler_showcase
```

Scenario names come from the annotations in `Source/Scenarios/Scenarios.cppm`. A scenario declared with `.backend_variants = true` registers under two names — `_cpu` and `_gpu` — from one body, so `parity_stack` yields `parity_stack_cpu` and `parity_stack_gpu`. See `docs/scenario_authoring.md` to add one.

A scenario owns the flags its annotation sets and will refuse a run that also passes them on the command line; `--engine-setting Section.key=value` is always available.

## Parity fixtures

The `Parity*` worlds are deliberately minimal physics fixtures — a drop, a resting pair, a column, grids at several densities, one of every collider shape. They exist so the CPU and GPU solvers can be compared step for step. `Tools/parity_gate.py` drives the `_cpu`/`_gpu` scenario pairs and diffs their state dumps.
