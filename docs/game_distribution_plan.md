# Game Distribution — download, auto-update, play together

Status: planned 2026-10-07, not started. Parent context: [engine_sdk_packaging_plan.md](engine_sdk_packaging_plan.md) (the SDK image, pack format, installer stub and registry this plan reuses).

Goal: a friend downloads one thing, runs it, and is playing Sandbox on Dhiren's server within a minute — and the next time they launch, they are on the current build without doing anything. SDK releases on GitHub come along for the ride because the same CI produces them.

The SDK is *not* what friends need. The SDK is for building games against the engine; it carries BMIs, archives and sources. Friends need a **game pack**: the game exe, its runtime DLLs, engine resources, baked assets and an installed-mode manifest. The packager already stages all of that except the exe.

## Rules for whoever picks this up

- **Decisions are deferred to the start of the phase that needs them.** Each phase below has a "Decide first" list. Do not settle those earlier, and do not settle a later phase's list while in an earlier phase — the earlier phase will produce information that changes the answer.
- Phases are ordered by dependency. Phase 3 needs phase 2's release feed; phase 4 needs phase 3's launcher to carry a server address. Phase 1 can start now.
- Multiplayer must work at the end. Phase 4 is not optional polish; it is the acceptance test for the whole plan.
- Verification rules from `docs/CODE_REVIEW_GUIDE.md` apply: builds go through the editor (`gse_build` / the build inbox), never a bare `cmake`/`ninja`. The packager is driven the same way (`gse_package_sdk`, or the inbox `package/` request the MCP server writes — see `Tools/gse-mcp/server.mjs`).
- Agent build requests are only claimed by an editor whose project owns the request's `cwd`. A request issued from the engine checkout does not reach an editor open on a project under `%USERPROFILE%\GSEProjects`.

## What already exists (read these before designing anything)

| Piece | Where | What it gives you |
|---|---|---|
| SDK packager (editor side) | `Editor/Editor/Source/BuildRunner/PackageSdk.cppm` | `package_system`: stages an image from a build tree, verifies it by building and running the shipped consumer, builds the `Installer` and `Setup` targets, appends the image as a pack to the stub, registers the version. Reads the toolchain from the build tree's `CMakeCache.txt`. Runs on request from the terminal's build row or from the agent inbox. |
| SDK packager (CI form) | `Tools/package_engine_sdk.py` | Copy-only stager with `--verify` and `--out` (zip). The shape a GitHub workflow should call. Writes every text file with `newline="\n"` — a CRLF mapper maps nothing. |
| Pack format | `Engine/Engine/Source/Sdk/Pack.cppm` | `append_pack(root, target, version, preset)`: one LZMS blob per file, a reflection-serialised `pack_table`, a fixed trailer at EOF so the stub finds its payload by seeking back. `read_pack`, `extract_entry`. |
| Setup stub | `Installer/Setup/Main.cpp` | Static exe importing only `gse.sdk` + win32 wrappers; runs on a machine with none of the image's DLLs. Extracts `Bin/`, `Engine/Resources`, `Engine/Baked` to `sdk::extraction_dir(version, preset)` under `%LOCALAPPDATA%\GSE\cache\installer\`, writes an installed-mode manifest there, launches `Bin/Installer.exe --payload <setup exe>`. |
| Installer gui | `Installer/Source/{Installer,Screen,Stages}.cppm` | Destination field, progress bar from a background `task::pending`, registry registration (`gse.sdk:registry`), `gse.install` file list, HKCU `Uninstall\GSEngineSDK-<version>-<preset>` entry. Headless `--uninstall` deletes the recorded files, unregisters, removes the key and the extraction cache, then hands the folder to a temp `.cmd`. |
| Installed-mode path resolution | `Engine/Engine/Import/Config.cpp` (`run_mode::installed`) | `root` = image, `Engine/Source` for source assets, `Engine/Baked` for baked, logs/crash/cache under `%LOCALAPPDATA%\GSE`. **`project_data` still resolves to `<project>/.gse/data`, i.e. inside the install dir — phase 1 fixes this.** |
| Registry | `Engine/Engine/Source/Sdk/Registry.cppm` | `[sdks] <version> = <parent>` in `%APPDATA%\GSE\engines.ini`; `extraction_dir(version, preset)`. |
| Layout store | `Engine/Engine/Source/Fs/LayoutStore.cppm` | `flush()` now drains on the calling thread when no task worker exists, so headless tools (stub, uninstaller, a launcher) can write ini files and exit. |
| Toolchain releases on GitHub | `.github/workflows/build-gcc-trunk.yml` | Weekly cron + manual dispatch; builds on `windows-latest` via MSYS2, uploads an artifact, creates a release with `softprops/action-gh-release`. The template for phase 2. |
| Run bounds | `Engine/Engine/Source/Runtime/Bootstrap.cppm` (`exit_after`), `Engine/Engine/Source/Runtime/Bench.cppm` (scenarios) | A game run can be bounded by time or driven by a named scenario and exit on its own — the acceptance mechanism for packed-game verification. The build runner passes these to a launched game; see `pool_args` in `BuildRunner.cppm` (`--engine-bench-scenario`). |
| Http client | `Engine/Engine/Source/Http/` (`Client.cpp` is the WinHttp backend, `System.cppm` a throttled system form) | Ticketed `client::send`/`poll`, plus a file `sink` and a `received` progress counter added in phase 3; redirects and TLS 1.2/1.3 by default. |
| Networking | `Engine/Engine/Source/Network/` (`Client.cppm`, `Discovery.cppm`, `Config.cppm` — `default_port = 9000`), `Engine/Server/{Application,Server}.cppm` | Client connect state machine, hostname resolution, saved servers and a server dashboard landed in `149eaba4`; silent-peer reaping and disconnect notices in `f4858f91`; reliable-message retransmit fix in `b82791e0`. |

## Phase 1 — Game pack — DONE (2026-10-07), pending a clean-machine test

**Deliverable:** `dist/game/Sandbox-<version>-<preset>-setup.exe` that installs and runs Sandbox on a machine with no toolchain, no repo and no SDK. Produced and verified: a 103 MB setup exe over a 529 MB image.

**Decisions taken**
1. **`Sandbox.exe` alone.** There is no server exe and never was: `Engine/CMakeLists.txt` builds `EngineServer` as a *static library*, and the only linkers are `SandboxLib` and `Sandbox`. `Main.cpp` already selects the role through `network::resolve_role` — `session_role::dedicated` sets `create_window = false`, `render = false` and registers `server_setup(e)` instead of the client systems. **Phase 4's dedicated server is this same binary** run with the dedicated role; nothing extra needs packaging. This is why the Release tree has no `Server/` output.
2. **Installed `project_data` → `%LOCALAPPDATA%\GSE\data\<exe stem>`**, alongside logs/cache/crash. Safe to change outright: `project_data_dir()` and `project_data_path()` had **zero callers** in the tree, so this was a latent bug, not an active one, and nothing exercises the new path either.
3. **Reuse the installer, parameterized by pack kind.** Reuse costs nothing and does not constrain phase 3, because the phase-3 launcher bypasses the installer entirely — it owns its own directory and extracts with `sdk::extract_entry`. The setup stub was already generic (its `bootstrap_prefixes` are exactly the game pack's `Bin/`, `Engine/Resources/`, `Engine/Baked/`).

**How the two kinds are told apart.** `pack_table` carries a `pack_stamp` (`version`, `preset`, `product`, `kind`); `pack_version` went 1 → 2. The SDK-specific installer behaviours — default destination, SDK-registry registration, uninstall key prefix, product scoping, and the "projects bind it with [engine] version" hint — are derived from `pack_kind` through `sdk::traits_of`, reading a `pack_kind_info` enumerator annotation. Display strings come from `stamp.product`. Nothing infers SDK-ness from pack *contents* (e.g. the presence of `gse.modules`); that would be a second derivation of one fact.

**Install identity is kind + product + version + preset.** `pack_kind_info::scoped_by_product` is false for the SDK (one product, so its paths and uninstall key are unchanged from before this phase) and true for games, which puts `stamp.product` into both the destination (`%LOCALAPPDATA%\GSE\game\<product>\<version>\<preset>`) and the uninstall key (`GSEngineGame-<product>-<version>-<preset>`). Without it, two titles packed from the same engine commit and preset would install over each other and share one Add/Remove entry. Phase 2's version scheme changes the `<version>` token but not this rule.

**Two traps worth remembering when extending `pack_kind_info`.** A `char[N]` annotation field passed straight to `std::format` prints the whole array *including its NUL padding*, because `std::formatter<char[N]>` formats `basic_string_view(s, N)` — the registry key silently became `...\GSEngineSDK\0\0...` for every install. Wrap such fields as `std::string_view(field)`, which selects the `strlen` constructor; the tree's other annotation call sites (`Agent/Chats.cpp`, `Agent/Phase.cpp`, `LintPanel.cppm`) already do. Passing one into `std::filesystem::path`'s `operator/` is safe by contrast, since the array decays to `const char*`. Also bind `traits_of`'s result to a named local: it returns the aggregate by value, so a `string_view` into a temporary dangles.

**Divergence from the original plan, accepted:** no per-file content hash on `pack_entry`. There is no SHA-256 in the C++ tree (only `bootstrap.py` and CI yaml), so it needs a new win32 BCrypt wrapper; its only consumer would be phase-3 delta updates, which phase 3 defers in favour of full re-download; and the integrity hash phase 3 calls non-negotiable is a *whole-asset* hash published in the feed, which CI computes with no engine code. `pack_version` is already versioned and nothing has shipped, so adding it later is free.

**Two things the plan got wrong, found by building it**
- **The game pack also needs the project's own content**, which the original staging list omitted: `Sandbox/Assets` → `Assets/` and `Sandbox/.gse/baked` → `.gse/baked/` (~20 MB). Without them the packed game dies on `Assertion Failure: ID Clips/idle not found`.
- **`Engine/Resources` cannot be trimmed to "assets".** `PipelineBuilder.cpp` compiles shaders from `.slang` *source* at boot, so the tree is load-bearing at runtime and first launch pays the slang compile that shows up as a boot watchdog stall. `Engine/Source` (the C++ module sources) *can* be excluded — `config::source_dir()` is read only by the editor and `SdkConsumer`, never by the engine runtime.

**What the pack must carry from the project root.** Three trees, and the list is one decision with installed-mode path resolution rather than two: `Assets` → `Assets/`, `.gse/baked` → `.gse/baked/`, and `Config` → `Config/`. All four names (`Assets`, `.gse/baked`, `.gse/data`, `Config`/`settings.ini`) are now `constexpr` in `gse.config` and consumed by both the packager and `config::resolve`, because they were previously spelled independently in three places and only one side moving would have silently reintroduced the missing-content bug. Any future project-root input belongs on this list. Absent trees are skipped with a visible `none at <path>` transcript line — `fs::copy_tree` errors on a missing source, so a project without one of these would otherwise fail to package.

**Project-scope settings ship read-only and are seeded writable.** `Sandbox/Config/settings.ini` holds authored defaults (`[UI] current_theme = midnight`, the `[Dev Spawn]` counts) that load as the `project` layer — the layering is `code_default < project < user < app_pin < session`. The image cannot simply be read in place: `save::registry::save_all` writes `m_paths.project` unconditionally whenever auto-save is on, so pointing it at the read-only install dir fails on every save. Instead installed mode resolves `project_settings` to `%LOCALAPPDATA%\GSE\config\<exe stem>\settings.ini` and `engine::initialize` seeds that file from the image's `Config/settings.ini` when it is absent, before `m_save.load()`. The seed is independent of `load_settings`, so a hermetic scenario run still creates it. Verified: deleted the file, packaged, and it reappeared carrying the authored values rather than engine defaults.

**The `project_explicit` trap.** `config::resolve_content_roots()` registers the project content root only `if (has_project())`, i.e. `project_explicit`, and an installed image names no `project` in its manifest — so project assets were invisible. Installed mode now sets `project_explicit = true` (the image root *is* the project). Baking an absolute `project =` path into the manifest would have been wrong: the image is relocatable. But that flag also decided where project settings are written, which for an installed game is the read-only install dir, so installed mode now resolves `project_settings` to `%LOCALAPPDATA%\GSE\config\<exe stem>\settings.ini`. These were one flag serving two facts; they are now separate.

**Verification wired into the package step.** The SDK kind keeps the `SdkConsumer` configure/build/run. The game kind runs the packed exe from the staged image through the `render_stress` scenario and requires exit 0 plus a `frames measured` line in the captured output — a real predicate, because the engine's console sink writes to stdout and flushes when piped. `render_stress` (rather than a headless scenario) is deliberate: it is what forces the shader compile out of `Engine/Resources` and the baked assets out of `Engine/Baked`. Confirmed: 600 frames, no assertion.

**Driving it:** `gse_package_sdk` takes `kind: "sdk" | "game"` and, for the game kind, `scenario`, carried as `kind` and `scenario` lines in the inbox `package/` request. The terminal's "Package SDK" button stays SDK-only; the game path is agent- and CI-driven by design.

**The verify scenario is the project's to name, not the editor's.** Scenario names live in each project's own `Scenarios.cppm`, so editor packaging code names none: the scenario arrives through the request and `verify_game` refuses an empty one. The default (`render_stress`) sits at the agent boundary in `gse_package_sdk`, matching the Python form's `--scenario`, so the two entry points cannot diverge on the one project-specific parameter. An unknown name is reported as such — `verify_game` watches for the game's own `unknown scenario` output — rather than as a crash in the packed game. Better long-term home is a key in the `.gseproj`; that needs a new `project::manifest` field and parse, and no existing section fits, so it is deferred rather than bodged into `[targets]`.

**Still open**
- **The clean-machine test.** A fresh Windows account or VM running the setup exe and playing single-player has *not* been done — it cannot be automated from here. This is the one remaining gate on "done".
- `render_stress` needs a window, so it is unusable on a hosted runner; the headless alternatives never touch shaders. That is a phase-2 "decide first", not solved here.
- The Python packager's drift was closed in phase 2 by deleting the second packager rather than reconciling it. For the record, its gaps were worse than this doc claimed: it wrote no `version`/`toolchain`/`engine`/`build_type` stamp, and — not noted here before — no `gse.link` and none of the vendored `vcpkg_installed/` archives, so its SDK image could not be consumed at all. It *did* stage `Engine/cmake`, contrary to what this line used to say.

**Done when** a fresh Windows user account (or a VM) with nothing installed can run the setup exe and play Sandbox single-player.

## Phase 2 — CI builds and GitHub releases — WRITTEN (2026-10-07), pending a dispatch

**Deliverable:** a workflow that produces, per release, the game setup exe, the game zip, and the SDK setup exe, attached to a GitHub release tagged by version.

**The finding that set the design.** Decision 4's intended answer — "CI runs the Python packager" — cannot produce the deliverable, for three reasons that only showed up on reading both packagers against each other. `sdk::append_pack` is C++ only (LZMS through `win32::compress_lzms`, plus a `pack_table` serialised by `binary_writer` through reflection), so Python could stage and zip but never make a setup exe. Its SDK image was unusable even as an image, because `Engine/cmake/GSEEngineSdk.cmake` reads `gse.link` from the image root and Python wrote neither it nor the vendored `vcpkg_installed/` archives that the link line names — it read the link stanza live out of `build.ninja` for its own `--verify` and shipped nothing. And its manifest had no `version`, which `write_setup` reads back to stamp the pack.

So the answer to decision 4 is neither of its two options: **one packager, used by both**, rather than two kept in parity.

- **`Packer/`** — a new console exe target (`Packer/Source/{Image,Packer}.cppm`, `Main.cpp`), linking `Engine`. Args `--build-dir --engine-root --stage --kind --scenario --version --verify/--no-verify --setup/--no-setup`, transcript on stdout, nonzero exit on failure. It derives the engine root and project root from the build tree's `gse.manifest`, so `--build-dir` alone is enough. This is what CI runs.
- **The editor spawns it.** `package_sdk` keeps `request`, `owns_request` and the whole `package_system` loop (terminal row, build-inbox publish, rejection-while-building), but its body is now: build a `launch_plan` from the `config::worktree`, `cmake --build --target Packer`, then run `Packer.exe` through the existing `spawn::run_capture`. The editor still owns the stage location and passes it with `--stage`, so it knows the result path by construction rather than by parsing the child's output.
- **`project::register_sdk` is gone** (zero callers): the Packer registers the image itself through the engine-side `sdk::register_image`.
- **`.github/workflows/release.yml`** — `workflow_dispatch` + `v*` tag push, resolving the tag and the `gcc-trunk-vN` toolchain on a Linux job, then building and packaging on `windows-latest`.

**Decisions taken**
1. **`version` is the release tag when one is passed, the 12-hex engine sha otherwise.** The binding constraint is phase 3, not phase 2: the launcher must answer "is the feed newer than what I have", and a sha cannot be ordered. `v<YYYY.MMDD>.<n>` can. One new optional `--version`, no new manifest key — `engine` already carries the full sha independently — and local editor packaging is unchanged, still stamping the sha. One release, three assets, shared tag.
2. **Trigger: manual dispatch + `v*` tag push, draft by default.** Same shape as `build-gcc-trunk.yml`. Promote to automatic once hosted build time is a measured number rather than a guess.
3. **Cache the vcpkg binary archives and the toolchain zip; deliberately not the BMIs.** `bootstrap.py` itself warns the first configure is 30–60 minutes, and that cost is the vcpkg tree, not the compile — so `VCPKG_DEFAULT_BINARY_CACHE` is cached, keyed on `vcpkg.json` + the overlays + `.gitmodules`, and `~/.gcc-trunk` is cached by toolchain tag. A BMI cache is path- and toolchain-sensitive under the module mapper and a stale one fails in ways that cost far more than the compile it saves; every run takes the full engine build.
4. **Packager: the Packer exe above.**
- **Targets built on CI are `Sandbox Installer Setup Packer`, never `Editor`** — it is the heaviest target in the tree and ships in neither image.
- **The runner needs no new tooling.** `python bootstrap.py --skip-submodules --skip-cppref --skip-build --tag <toolchain> --persist` installs the toolchain where `CMakePresets.json` already looks (`~/.gcc-trunk/current`) and puts Ninja at `~/.gcc-trunk/ninja`, which the preset's `PATH` already names.

**Three deviations from the approved scope, each with a reason**
- **The packaging code lives in `Packer/Source/Image.cppm` (`module packer:image`), not as a `gse.sdk` partition.** `Installer/Setup/Main.cpp` does `import gse.sdk` whole-module, and that stub is the one binary that must run `-static` on a machine with none of the image's DLLs. Putting json, process and log behind that import to serve a tool the stub never calls risks the only component with a hard no-dependencies requirement. The packaging logic has exactly one consumer now that the editor merely spawns it, so it belongs with that consumer.
- **The Python hello-TU verify was not ported.** It would mean a second copy of `GSEEngineSdk.cmake`'s compile flags in C++, and the `SdkConsumer` configure/build the Packer already runs goes *through* that cmake — so it already fails if `gse.modules` or `gse.link` is missing, which is the gap that let Python's image ship broken. The cmake-driven consumer is the strictly better predicate and the flag list stays in one place.
- **No `--out-zip`.** The workflow zips the staged image with `Compress-Archive`. Nothing in the engine reads a zip back, so a zip writer in C++ would be machinery with one caller and a shell one-liner alternative.

**One engine change was needed.** `process::capture_request` gained `path_prefix`, used in place of the command's own directory when set. `run_capture` only put the *command's* directory on `PATH`, but the toolchain-bin-first rule is load-bearing when the child is cmake driving `g++` — that is the "`g++` exits non-zero with no output" gotcha below. The field mirrors `spawn::run_capture`'s existing parameter, so the two spawn paths now agree, and every existing caller is unaffected by the empty default.

**Verified locally, both kinds, through the editor's own path**
- Game: staged, ran `render_stress` in the staged image, built `Installer`+`Setup`, appended the pack — a fresh 108 MB `Sandbox-17594e55092f-x64-mingw-gcc-Release-setup.exe` with the same manifest shape phase 1 produced.
- SDK: staged 530 MB with `gse.modules`, `gse.link` and the vendored archives, verified by configuring/building/running `SdkConsumer` against the image, appended the pack — a fresh 202 MB `GSEngine-...-setup.exe`, and `[sdks] 17594e55092f` written to `engines.ini` by the child process.
- `--version v2026.1007.1` stamps `version = v2026.1007.1` while `engine` keeps the full sha, confirming the CI-facing override.

**Still open**
- **The workflow has never been dispatched.** It is written against the real tooling but every claim about hosted build time, cache behaviour and the vcpkg restore is untested until someone runs it. Expect the first run to need adjustment; that is what the draft default is for.
- **The game verify on a hosted runner is unsolved, as predicted.** `render_stress` needs a window and the headless scenarios never touch shaders, so the workflow stages the game pack with `--no-verify` and the SDK verify (which needs no window, and is the one that catches a broken image) carries the quality gate. A `verify_game` input runs the scenario anyway, so the "does the DX12 backend's WARP software rasteriser suffice on `windows-latest`?" experiment can be run without a failed experiment blocking a release. If WARP is not enough, the honest fix is a self-hosted runner on Dhiren's machine — which phase 4 already needs always-on.
- ~~**`Tools/package_engine_sdk.py` is still on disk.**~~ Deleted. It should have been deleted — it is now a second packager that produces a silently unusable image — but it has uncommitted working-tree changes, so the delete was refused as unrecoverable. It needs deleting by hand once those changes are committed or discarded.
- Found incidentally, not fixed: with **two editors open on one project**, both peek the same inbox `package/` request, and the one that consumes second publishes a `rejected` result under the same id while the first is already packaging — so an agent gets "a package is already being staged" for a run that in fact succeeds. Pre-existing, and orthogonal to this phase.

**Done when** a manual dispatch produces a release page with the three assets and a friend can download the game zip from it.

## Phase 3 — Launcher with auto-update — WRITTEN (2026-10-08), pending a published release

**Deliverable:** `Launcher.exe` that checks for a newer game build, installs it, then starts Sandbox pointed at Dhiren's server.

**The one choice that answered three of the "decide first" questions.** The launcher ships **inside the game image, as `Bin/Launcher.exe`**, not in a directory of its own. It therefore sits next to the engine DLLs and `Engine/Resources` its GUI needs — `PipelineBuilder` compiles `.slang` at boot, so a launcher in its own folder would need either its own copy of the runtime or a hand-rolled win32 dialog, and the shipped installer already proves the engine-GUI form works. Self-update then costs nothing (decision 4): installs are versioned directories, so an update extracts into the *new* version's directory and `relaunch_on_exit`s the new `Bin/Launcher.exe` — nothing is overwritten in place and the uninstaller's swap-after-exit `.cmd` trick is not needed. And "what is installed" needs no state file (decision 5): it is the set of version directories under `install_root`.

**Decisions taken**
1. **Feed: a `feed.json` release asset**, fetched from `https://github.com/<repo>/releases/latest/download/feed.json`. Not the Releases API (rate-limited, and couples the launcher to that schema) and not a manifest on Dhiren's machine — updates would then break whenever it is down, and the only thing that would have bought is phase 4's `server` field, which rides in this file anyway. That URL is a stable redirect and invisibly skips drafts, so **the phase-2 draft default is load-bearing in reverse: a launcher sees nothing until a release is published.**
2. **The Packer writes the feed**, from `--feed-url` (baked into the image manifest as `feed =`, so the shipped launcher knows where to look) and `--asset-base` (where this release's setup exe will be downloadable). The schema has one spelling, `gse::sdk::feed` in `gse.sdk:feed`, written with `json::stringify` and read back with `json::parse_as` — writing it from PowerShell would have been a second copy. CI's `Get-FileHash` step stays for the human-readable release-body table.
3. **The download is the setup exe itself.** `read_pack` finds its payload by the EOF trailer and `extract_entry` unpacks it, so there is no zip reader, no second asset to publish and no format change. Full re-download, not delta: `pack_entry` carries no content hash, exactly as phase 1 predicted, so a delta still needs a format change.
4. **Integrity is a SHA-256 in the feed**, and the tree now has one: `win32::hash_sha256` next to `compress_lzms` in `External/Win32.cppm` (plus `bcrypt` on the link line), wrapped as `sdk::digest_of(file)`. Two callers from the start — the Packer hashes the exe it just produced, the launcher verifies before extracting — so producer and consumer cannot disagree. Hashing reads the whole file into memory, which is what `sdk::read_bytes` already does for every packed file.
5. **Version ordering parses `v<YYYY>.<MMDD>.<n>`** into three numbers; a lexical compare puts `.10` below `.2`. When either side fails to parse — a locally-packed 12-hex sha build — any difference counts as an update, so a dev machine is never silently downgraded. `gse.tests.feed` covers the ordering and the json round trip (`--engine-test-tags feed`).

**One engine change, in the http client.** `http::request` gained `sink` (a destination file) and `received` (an optional `std::atomic<std::uint64_t>*`), and the `WinHttpReadData` loop writes through to the file instead of growing `response::body`. Without it a 110 MB download would be a 110 MB `std::string` with no progress signal; with it the progress bar is the same `std::atomic` idiom `installer::install` already uses. `max_body_bytes` now applies only to in-memory bodies, and a write failure is its own error (`sink_failed`) rather than a silent truncation.

**Shared install identity.** `install_identity`, `install_root` and `install_image` moved out of `installer:stages` into `gse.sdk:registry`, where the launcher and the installer share them. Phase 1's closing gotcha — nothing is uniquely identified by version + preset — forbids a second derivation of an install's name.

**The packaging transcript now lands on disk.** `Packer` writes every transcript line to `dist/<kind>/packer-<kind>.log` on both success and failure. This phase could not have been built without it: an agent-issued `gse_package_sdk` returns only "the transcript above says why", and "above" is the editor's package tab, which an agent cannot read. This closes the phase-2 gotcha about transcripts that agents are expected to act on.

**Verified locally**
- Packaged the game pack through the editor's own path: `Bin/Launcher.exe` is in the image and in the pack (192 files, 114 MB setup exe), and `render_stress` still passes in the staged image (600 frames).
- Ran the packed `Bin/Launcher.exe`: it booted in ~1.6 s (only the sprite and msdf shaders compile, so the feared first-run slang stall is not a factor here), found no `feed` in its manifest, and started `Sandbox.exe` from the image before exiting. That is the offline/no-feed path.
- `--engine-test-tags feed`: 3 passed.

**Still open**
- **The feed path has never run against a real release**, because `releases/latest/download` cannot see a draft. The first dispatch that publishes is the test.
- **A launcher-applied update writes no Add/Remove entry**, and the entry the setup exe wrote still points at the version it installed. Nothing dangles — old version directories are kept, which is also what the offline fallback launches — but versioned installs accumulate at ~530 MB each and nothing prunes them. Both belong with phase 4's identity work rather than here; pruning naively would orphan the setup exe's uninstall entry.
- **The phase-4 assumption that the game "connects on boot when given a server" is wrong.** `--engine-net-connect` reaches `client_system::init`, which only *seeds* the server list as "Configured Server" (`Sandbox/Source/Client.cpp:16`); the player still picks it in the network screen. The launcher passes the address and needs no engine change, but auto-connect is phase-4 work.
- The launcher boots the full engine to show one progress bar. Measured at ~1.6 s, so it stays; the fallback if that ever regresses is a render-less check pass, not a hand-rolled dialog.

**Done when** a friend with an older game build opens the launcher and is playing the current build without touching anything else.

## Phase 4 — Multiplayer against Dhiren's server

**Deliverable:** two or more friends on different networks in the same Sandbox session hosted on Dhiren's machine, joined through the launcher with no manual address entry.

**Decide first**
1. Hosting shape: the dedicated server exe running as a service on Dhiren's machine, or a listen server inside his own client. Dedicated is what the launcher feed assumes (an always-on address); confirm the server target from phase 1 decision 1 runs headless and restarts on crash (a scheduled task or NSSM-style wrapper).
2. Reachability: port forward on the router for `default_port` (UDP 9000 today — confirm the protocol in `Network/Config.cppm`), a dynamic-DNS name if the home IP changes, or a relay. Decide after measuring: try a direct port forward with one friend first; the engine already resolves hostnames (`149eaba4`), so a DDNS name needs no code.
3. Version gating: the server must refuse clients whose build does not match. Decide whether the protocol already carries a version (check the handshake in `Network/Message/` and `RemotePeer.cppm`); if not, add the pack version to the hello and reject mismatches with a message the launcher can turn into "update required".
4. Whether friends need an identity (name, persistent id) and where it lives — probably a name in the launcher's local config, nothing more.

**Work**
- Server packaging and a start script on Dhiren's machine; the feed's `server` field pointing at it.
- Launcher passes the server address to the game on start; the game's saved-servers path connects on boot when given one.
- An end-to-end run with at least one friend outside Dhiren's LAN. Record what broke in this doc's gotchas section; NAT, MTU and clock drift are the usual suspects.
- The reconnect path: server restarts while clients are connected should put clients back on the launcher's "reconnecting" state, not a crash. `f4858f91` covers silent-peer reaping server-side; verify the client side.

**Done when** the two-friend session above holds for a long play session and a server restart recovers.

## Phase 5 — SDK niceties (only when someone other than Dhiren wants the SDK)

Deferred entirely; listed so they are not forgotten. Each has its own "decide first" when it comes up.

- Bind a project to an installed image from the project screen (today: edit `[engine] version` in the `.gseproj` by hand).
- Bundle the toolchain in the SDK installer so an SDK user does not run `bootstrap.py`.
- SDK updates (the launcher pattern applies, but the registry and per-preset layout need a migration story).
- Code signing for the installers and the launcher.
- Trim the empty `[sdks]` header the registry writer leaves after the last entry is removed (cosmetic).
- A boot log line reporting the resolved run mode and image root. There is none today, so an installed game's path resolution is undiagnosable from its log; the game-pack verify has to infer success from the scenario completing instead of asserting on the root directly.

## Gotchas carried over

- A mapper or any text file with CRLF written by Python text mode breaks module resolution silently — `newline="\n"` everywhere.
- `g++` exits non-zero with no output when the toolchain's `bin` is not first on `PATH`.
- Spawn the compiler with forward-slash paths; a backslash driver path once produced `cannot find -ladvapi32` through the toolchain junction and was never fully explained.
- The toolchain stamp refusal (`build_runner::sdk_toolchain_bin`) and the configure failure text go to the terminal stream; the agent-facing build result reports "no parseable diagnostic". Log the transcript tail on failure if an agent is expected to act on it.
- Headless tools must not rely on a task worker to write ini files; `layout_store::flush` drains inline now, but anything new that posts to `task::` from a headless process needs the same care.
- The install dir is read-only by rule. Anything a running game writes goes under `%LOCALAPPDATA%\GSE`.
- `sdk::extraction_dir` used to be keyed on `(version, preset)` alone, which phase 1 left alone on the grounds that a collision needed two setups built from the same commit *and* preset. Phase 2 made that pair collide by construction — one release tag, one preset, both setup exes — so it now takes the whole `pack_stamp` and scopes the path by `install_subdir` and `product` exactly as the install destination and the uninstall key do. The lesson generalises: **nothing is uniquely identified by version + preset any more.** Anything new that keys on an install must take the stamp, not a pair of strings. `installer::install_identity(stamp)` is the one derivation of that name, shared by the uninstall registry key and the deferred-removal script.
