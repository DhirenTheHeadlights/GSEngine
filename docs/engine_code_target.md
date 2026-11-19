# EngineCode

`EngineCode` is an optional static library beside `Engine` and `EngineServer`, built from
`Engine/Code/`. It holds the tooling for editing C++ inside a running program:

| Module | Role |
|---|---|
| `gse.syntax` | lexer, classifier and reserved-word lists |
| `gse.format` | the formatter |
| `gse.analysis` | highlighting, compiler diagnostics, `gse_tokens` plugin records, `plugin_arguments` for the plugin's command-line flags |
| `gse.complete` | the completion model, context, query, session and hover |
| `gse.complete.view` | completion popup, hints and hover card driven by a session |
| `gse.complete.widgets` | the generic popup, ghost text, signature strip and hover card widgets |

## What belongs here

Code that is about C++ source as text, does not depend on any one host, and is used by at least two
hosts. The GSE editor and CodeForces both link it. A game that does not edit code never pays for it.

## Rules

- The `Engine` target never imports anything from `Engine/Code`. `EngineCode` links `Engine`
  publicly, and a reverse import would create a cycle.
- The `gse` umbrella module does not re-export these modules, so every user imports them explicitly.
- Its tests (`gse.tests.complete`) are imported by `Tests/Main.cpp` directly instead of through
  `gse.tests`, which lives in `Engine`.
- Editor-only behaviour (panels, project indexing, the runner) stays in the editor. Only the shared core
  moves here.

## Known gap

`PackageSdk` copies only `Engine/Engine` sources, so an SDK package does not include `Engine/Code`.
`EngineServer` already has the same gap.
