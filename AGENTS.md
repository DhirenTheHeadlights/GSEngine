The style for this codebase is the STL style. We do, however, prefix private variables with m\_.

Do NOT put comments in your code. The code should be self documenting.

See `docs/STYLEGUIDE.md` for the full style guide.

## Function Signature Layout

- Declarations with parameters always wrap: one parameter per line and `)` on its own line.
- Definitions never wrap their parameter list.
- Zero-parameter declarations remain inline as `()`.
- Put one blank line between adjacent function declarations.
- This applies to constructors, operators, static functions, and templates.

## Unit Types

Unit types (e.g., `gap`, `displacement`, `velocity`, `force`, etc.) have the **same memory layout as their underlying float**. They can be passed directly to GPU push constants, `memcpy`, etc. without any cast or extraction — they are layout-compatible with `float`. Never `static_cast` a unit type to float. Just use it directly.

## Logging System

A logging system writes to `%LOCALAPPDATA%\GSE\logs\<exe>.log` (e.g. `Editor.log`, `GoonSquad.log`), cleared on each run, keeping the last 5. Assertions automatically log failures.

**To debug issues:** Read the log file instead of asking the user to paste console output.

**Implementation:** `Engine/Engine/Import/Log.cppm` - read this file for the current API.

## Config Module

`gse.config` (in `Engine/Engine/Import/Config.cppm`) provides every engine path as a **function**, resolved at runtime — `resource_path()`, `root_dir()`, `user_config_dir()`, etc. Re-exported by `gse.utility`. Nothing is baked into the binary: paths come from a `gse.manifest` marker file found by walking up from the executable's directory (CMake writes one to the build root at configure time with `mode = dev` and `root = <source tree>`).

Where runtime files go — **none of them land in the repo**:

| What | Where |
|---|---|
| `<executable>.ini`, `gui_layout.ini`, `editor_layout.ini` | `%APPDATA%\GSE` (`user_config_dir()`) |
| logs, crash dumps, profiles, screenshots/recordings/clips | `%LOCALAPPDATA%\GSE` (`logs_dir()`, `crash_dir()`, `profile_dir()`, `captures_dir()`) |

**User-scope settings live at `%APPDATA%\GSE\<executable_stem>.ini`** — one file per executable, so the editor and the games it launches no longer share a `[Window]` section. That is the file to hand-edit; the old `%APPDATA%\GSE\settings.ini` and `Engine/Resources/Misc/settings.ini` are no longer read. There is no automatic migration from the old locations.

Override the roots with the `GSE_USER_DIR` and `GSE_STATE_DIR` environment variables. Running an executable with no `gse.manifest` above it aborts with a message on stderr.

