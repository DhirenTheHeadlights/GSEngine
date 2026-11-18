# Goal: First published game release

## Aim
Get the release pipeline to produce a real, published GitHub release of Sandbox, so that a friend can download it and the launcher's auto-update path has a feed to read. This is item 1 of the distribution status from 2026-11-18. The later items (clean-machine install, a live update between two versions, multiplayer, code signing, pruning old installs) are separate goals or tasks, not this one.

CI stays GPU-free. `verify_game` remains `false`, and nothing in this goal requires a GPU or a window on a hosted runner.

## Starting point (from scoping, 2026-11-18)
- `.github/workflows/release.yml` runs on `workflow_dispatch` or when a `v*` tag is pushed. It resolves the tag, then on `windows-latest` it builds `Sandbox Installer Setup Launcher Packer`, bakes assets (`Sandbox.exe --engine-bake-assets`), and packages the SDK (verified through `SdkConsumer`). It then packages the game with `--no-verify`, zips it, hashes it, and creates a release through `softprops/action-gh-release@v2`, as a draft by default.
- My notes say run #2 failed because the baked `<build>/Engine/Resources` tree did not exist on a clean runner. The bake step was added afterwards. I don't know whether any run has passed since; the first steering run should read the run history before it dispatches anything.
- `ci.yml` now passes on hosted runners without `--no-engine-gpu`, because GPU device creation falls back across backends instead of asserting. That makes it more likely that `--engine-bake-assets` works on the runner, but nobody has observed it there.
- The launcher fetches `https://github.com/<repo>/releases/latest/download/feed.json`. That URL ignores drafts, so only a published release counts.
- The working tree holds a lot of uncommitted audio work that belongs to another chat. A release builds from the pushed ref, not from the working tree.

## Pass criteria
Every criterion is checked from GitHub artifacts through `gh`, plus local downloads into a fresh, empty directory. Criteria 1 to 4 must hold for a single release.

1. **A green run.** A `release` workflow run on a pushed commit ends with conclusion `success`. Every step must pass, including the SDK verify step, which builds and runs the consumer.
2. **Published with all assets.** The release that run created is not a draft and not a prerelease. It carries exactly these files: `Sandbox-<tag>-<preset>-setup.exe`, `feed.json`, `GSEngine-<tag>-<preset>-setup.exe` and `Sandbox-<tag>-windows-x64.zip`.
3. **The feed is reachable and agrees with the assets.** `releases/latest/download/feed.json` downloads and resolves to this release. Its version equals the tag and its asset URL points at this release's game setup exe. The SHA-256 of that downloaded exe equals the feed's hash, and also equals the hash in the release body.
4. **The zip is a real game image.** The downloaded zip extracts to a folder containing `Bin/Sandbox.exe`, `Bin/Launcher.exe`, `Engine/Resources`, `Engine/Baked`, `Assets/`, `.gse/baked/` and a `gse.manifest` with `mode = installed` and `version = <tag>`. The goal only inspects the extracted files; it does not run the downloaded executables.
5. **Nothing local regresses.** Any engine or Packer change the goal makes builds cleanly through `gse_build`. The local game package still passes its scenario check (`gse_package_sdk kind=game`, `render_stress`).

## Charter (no need to ask)
- **Read GitHub:** use `gh run list/view/download`, `gh release view/download` and `gh api` read calls freely.
- **Dispatch:** run `gh workflow run release.yml` against `dev`, as often as fixing failures needs. Draft runs are free; set `draft=false` only for a run whose commit already produced a green draft, or once the remaining failure is known to be in the publish step. Leave `verify_game=false` and `server` blank.
- **Clean up after yourself:** delete draft releases and tags created by this goal's own failed or superseded runs. Never delete a release or tag this goal did not create.
- **Fix the pipeline:** these are in scope:
  - `.github/workflows/release.yml`;
  - `.github/actions/gse-build-setup/**`;
  - `Packer/**`, the Packer's `Image.cppm` and the packaging path in `Editor/Editor/Source/BuildRunner/PackageSdk.cppm`;
  - `Installer/**` and `Launcher/**`;
  - `gse.sdk` (`Engine/Engine/Source/Sdk/**`);
  - the bake entry point (`--engine-bake-assets` and its runtime path);
  - and whatever else a failed run's log points at directly in build, bake or packaging code.

  Every fix follows `docs/CODE_REVIEW_GUIDE.md` and `docs/STYLEGUIDE.md`, and is built through `gse_build`.
- **Commit and push:**
  - Commit only the files this goal changed. Stage them by explicit path, never with `-A` on the tree.
  - Push to `dev`, with no Claude or AI attribution in commit messages.
  - Leave other chats' uncommitted work alone. If a run fails because the remote `dev` lacks someone else's work, escalate rather than commit it.
- **Local diagnosis:** use `gse_package_sdk` and `gse_run` scenarios, with diagnostics turned on only through `settings` overrides.
- **Off limits:**
  - pushing to `main`, force-pushing, rewriting history;
  - changing repository settings or secrets;
  - running the downloaded setup exe or launcher on this machine, which counts as the clean-machine test (item 2);
  - turning on GPU verification in CI;
  - editing `settings.ini`;
  - scheduled or cron runs of the workflow;
  - releases from any workflow other than `release.yml`.
- **Escalate when:**
  - a fix needs a self-hosted runner, a paid runner, or a change of preset;
  - a run is above about 5 hours (the job timeout is 360 minutes);
  - the failure is in vcpkg or toolchain infrastructure outside the repo;
  - or three consecutive dispatches fail at the same step after a fix aimed at it.

## Ledger
- 2026-11-18 — Run history read: only runs #1 (37790539139) and #2 (37791957745), both 2026-10-08, both failed; nothing since the bake step landed. Dispatched draft run 44328400061 on dev @ 7d9908a8 (draft=true, verify_game=false). Verdict: pending — first observation of the current pipeline on a clean runner.
- 2026-11-18 — Run 44328400061 failed in build setup: install_gcc_trunk.py link_current raised PermissionError on `Path.exists()` of the cache-restored `~/.gcc-trunk/current` junction (stat follows the link). Fix: use `os.path.lexists` for both junction checks. No release or tag reached. Verdict: refuted the "pipeline is green since the bake fix" assumption; failure is now upstream of bake. Note: the previous await had no pid, so the failure went unnoticed until the owner reported it — watch runs via a local `gh run watch` pid.
- 2026-11-18 — Run 44329955785 (dev @ c11f10b0) failed at the same gcc-trunk setup step (2 of 3 toward escalation). The lexists fix got further: `rmdir` said "The directory name is invalid", then `force_remove_tree` → `Path.exists()` raised PermissionError. The root cause is that `~/.gcc-trunk/current` is saved inside the actions/cache entry and restored as a dangling reparse point. Fix: exclude `current` from the cache paths (a new path list means a new cache version, so the poisoned entry is skipped), and unlink a leftover reparse point with os.rmdir/os.unlink before the real-directory fallback. No release or tag was created. Verdict: the cached junction is refuted as fixable by lexists alone.
- 2026-11-18 — Run 44331880282 (dev @ e91a1ff0): gcc-trunk setup, build, and bake all passed. This is the first time bake has been observed green on a hosted runner. It failed at "Package the SDK image" in the SdkConsumer link: `ld: cannot find -l_wgpu-native/v29.0.1.1/lib/libwgpu_native.dll.a`. A vendored library path in gse.link reaches the consumer link line as a mangled `-l` argument. No release or tag was created. Verdict: the gcc-trunk cache fix is confirmed. The new failure is in the gse.link vendored-archive handling (Packer Image.cppm ↔ GSEEngineSdk.cmake).
- 2026-11-18 — Fix 1cee95d2 (Packer stages every build-relative vendored archive in gse.link; GSEEngineSdk.cmake roots them at the SDK) verified locally: sdk ok, render_stress green. Dispatched draft run 44341404794 on dev @ 1cee95d2. Verdict: pending — judge "Package the SDK image" and everything after it.
- 2026-11-18 — Run 44341404794 (dev @ 1cee95d2, draft) concluded `success`: every step was green, including Package the SDK image (the consumer build and run), the game pack, zip, hashes and the release. The draft v2026.1118.1 carried exactly the 4 required assets (Sandbox-…-x64-mingw-gcc-Release-setup.exe, feed.json, GSEngine-…-setup.exe, Sandbox-…-windows-x64.zip). Verdict: the vendored-archive fix is confirmed and the pipeline is green end to end. Deleted the superseded draft (it had no tag). Dispatched the published run 44346747667 (draft=false) on the same commit, watched by gh pid 4468. Note: the repo's current "Latest" is gcc-trunk-v8, so criterion 3 depends on the new release taking Latest.
- 2026-11-18 — Published run 44346747667 (dev @ 1cee95d2, draft=false) concluded `success`. Release v2026.1118.1 is Latest, not draft, not prerelease, exactly the 4 assets. releases/latest/download/feed.json resolves to it: version v2026.1118.1, url → this release's Sandbox setup exe, sha256 6b55f2dc…5132 equals the downloaded exe's hash and the body's. The zip extracts to Bin/Sandbox.exe, Bin/Launcher.exe, Engine/Resources, Engine/Baked, Assets/, .gse/baked/, gse.manifest mode=installed version=v2026.1118.1. Criterion 5: the only engine/Packer change (1cee95d2) was verified locally (sdk ok, render_stress green). Verdict: all pass criteria met.
