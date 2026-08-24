# Editor build recovery

`run_build_with_module_recovery` in `Editor/Editor/Source/BuildRunner/BuildRunner.cppm` wraps every
build the editor drives — the game target and the editor's own self-rebuild both go through it. It
runs the build, and when the build fails it inspects the captured transcript for failures it knows
how to repair, repairs them, and retries. Each repair is recorded in a `recovered` set keyed by the
path it touched, so a repair that does not actually fix anything is attempted once and then the
build is allowed to fail with its real diagnostics.

There are three recognised failures.

## Stale compiled module files

GCC writes a module interface to a `.gcm` and refuses to overwrite one it did not create in this
run, reporting `failed to write compiled module` alongside `File exists`, or `failed to read
compiled module` when the file on disk is truncated. `collect_module_write_conflicts` pulls the
`.gcm` paths out of the `FAILED:` lines and the `compiled module file is '...'` notes, keeps the
ones inside the build tree, and `clear_stale_module_file` deletes them so the retry regenerates
them. The delete is retried for ten seconds because semantic analysis in another thread may still
have the file mapped; `analysis_busy_state` is polled before the first attempt for the same reason.

## Locked runtime outputs

A DLL or data file the running editor has loaded cannot be overwritten, so the CMake copy step fails
with `Error copying file` and `Permission denied`. Windows does allow renaming an open file, so the
target is moved to `<name>.bak` and the copy lands on the freed path. This is the same trick the
self-rebuild uses on `Editor.exe`. The `.bak` is reclaimed by a later build.

## Poisoned ninja dependency log

This is the failure that motivated the third repair. Ninja merges the dependencies recorded in
`.ninja_deps` into the build graph before it does any work, and it never expires an entry on its
own. GCC records the `.gcm` files a module translation unit imported, so when the import direction
between two modules reverses — splitting `Kinds` out of `Lexer` and having `Lexer` import it, for
instance — the freshly scanned dyndep says `lexer.gcm` depends on `kinds.gcm` while the stale deps
entry still says `kinds.cppm.obj` depends on `lexer.gcm`. Ninja sees a cycle and stops before
building anything:

```
ninja: build stopped: dependency cycle: Engine/CMakeFiles/Engine.dir/gse.syntax-lexer.gcm -> Engine/CMakeFiles/Engine.dir/gse.syntax-kinds.gcm -> Engine/CMakeFiles/Engine.dir/gse.syntax-lexer.gcm.
```

Nothing about the sources is wrong, and no amount of rebuilding clears it, because the poison lives
in the deps log rather than in any file the graph references. Deleting the offending `.gcm` does not
help either: cycle detection runs over the graph, not over the filesystem. The only fix is to drop
`.ninja_deps`, and ninja offers no way to drop a single entry.

So `collect_dependency_cycle` matches `build stopped: dependency cycle:` in the transcript and
returns the cycle description for the log, and `clear_dependency_log` deletes `.ninja_deps` from the
build directory and lets the loop retry. Because the deps log is also what lets ninja skip work, the
retry rescans and recompiles everything — the recovery trades a long build for a build that
completes rather than one that fails and rolls the editor back to its previous image. The message
emitted before the retry says so.

Two cases are deliberately not retried. If `.ninja_deps` does not exist, the cycle is genuinely in
the module sources and clearing nothing would change that, so the build fails with a message saying
as much. If a cycle is reported again after the deps log has already been cleared once in this
build, the same conclusion applies and the loop stops rather than deleting a file that is no longer
the cause.

## Bootstrapping

The recovery lives in the editor binary, so an editor built before this change cannot use it. A tree
that is already poisoned has to be unblocked once by hand — delete `.ninja_deps` from that build
directory, or build the editor target from a terminal — and every build after that heals itself.
