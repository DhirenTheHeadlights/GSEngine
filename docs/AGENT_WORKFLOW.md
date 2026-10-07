# Agent Workflow

Standing rules for every agent the editor runs. These do not change between tasks or phases, and the editor inlines this file into each session's appended system prompt.

## Where You Are

You are editing inside the editor that is running you. You are not driving a CLI from outside it. The project root, the build directory, the module cache, and the logs all belong to this editor process, and every path you are given is local to this tree.

## Builds And The Executable

Queue builds freely through `gse_build`. Never invoke `cmake`, `ninja`, `make`, `msbuild`, or a compiler directly, and never run a single-file `-E` or `-fdeps` scan: the editor owns the build directory and the module cache, and a build started outside it corrupts them. That ownership is the reason for the rule, not the cost of a compile.

A running executable never blocks a build. The binary is renamed before the new one is linked, so a link cannot fail because the game or the editor is open. Do not refuse a build, defer one, or warn about this. You may launch the executable to verify a change while one is already running.

`gse_run` launches the existing executable without building it, for watching real behaviour instead of reasoning about it. It runs what is on disk: if you have edited since the last build, `gse_build` first or you are observing stale code. If a build is in flight the run waits and then starts the new binary, so you cannot race a half-written one.

**Name a scenario.** A run you launch has nobody at the keyboard, so without one it boots into a world where nothing happens and sits there until something kills it — which truncates the log you wanted to read. `scenario` is the point of the tool: a named script that settles the world, drives it through a fixed sequence on a fixed-step clock, and exits on its own with a profile, a percentile summary and a world-state hash. The catalogue is the annotated declarations in `Sandbox/Sandbox/Source/Sandbox/Scenarios.cppm`; take the name from there, because one that is not in it exits immediately and writes no log. `docs/scenario_authoring.md` is the manual if the behaviour you want to watch has no scenario yet — writing one is two short additions, and unlike a run you watched once it is reusable.

`seconds` bounds a plain, unscripted run. It exists for the two things a scenario cannot do: a crash during boot, and a bug that only appears under the owner's saved settings, which a hermetic scenario run never reads. A run with neither is refused.

Pass `settings` to override configuration for that run alone — `["Graphics.validation_layers_enabled=true"]` — delivered as `--engine-setting` arguments and applied in memory. **Never edit `settings.ini` to turn a diagnostic on.** That is the owner's file, it persists into every later run including theirs, and it is the kind of change that gets forgotten and then blamed on something else.

A `deferred` result means another chat is still editing and your build is queued. Nothing failed. Hibernate, wait, or move to unrelated work; do not re-run in a loop.

## Build Errors Are Attributed

Errors come back split by who edited the file, and they arrive as messages. Act only on the ones reported as yours.

- A failure in which none of the errors are yours does not implicate your change. Report the state of the tree and stop; do not start fixing it.
- Files another chat is working on are theirs to fix even when they break your build. Repairing them silently overwrites work in progress.
- You do not need to pre-emptively verify that someone else's work still compiles. If something of yours breaks, you will be told.

State clearly when a change has not been compiled. Never assert a build status you did not observe.

**The owner builds too.** They are working in this editor alongside you and may build at any time, for their own reasons, covering your edits as a side effect. So "this is still unbuilt" is not something you can know by remembering that you never built it — the tree moves without you. Your phase prompt carries the build state as of the moment the phase started, and the editor clears a chat's unbuilt set whenever any build covers its files, whoever started it. If the answer matters, build and observe the result; if it does not, say what you actually know ("I have not built this") rather than asserting the tree's state.

## Other Agents In The Tree

Other chats are working in this tree and in sibling worktrees, and the presence list names them. Their edits land in files you may be reading, so a signature that does not match its declaration is more likely a half-applied edit than a bug.

Before editing a file, consider whether someone else is in it. When your change and theirs overlap, say so and ask rather than racing. `build_hold::tree_busy` is information, not a failure: a busy tree is a wait.

## The Review Contract

Plan and review every change against `docs/CODE_REVIEW_GUIDE.md` and `docs/STYLEGUIDE.md`. Read them before designing, not after implementing — the guide decides what the design has to be, so consulting it at the end only finds what has to be redone. For renderer, shader, or GPU work add `docs/GRAPHICS_REVIEW.md`.

Two of the guide's demands are worth restating because they are the ones most often skipped under time pressure:

- Name the nearest existing feature and read it end to end before writing a panel, a render loop, a parser, or a measurement helper. A search that returned nothing is evidence about vocabulary, not about capability.
- Establish that a branch is reachable from its write site before adding it, and that a cache, lock, or generation has a caller whose frequency you read. Defensive code and plausible machinery both read as care and are neither free nor challenged.

## Reporting

Report outcomes faithfully. If part of a task is blocked, finish everything else and say exactly what you left out and why. Scaling work down is the owner's call, not yours.
