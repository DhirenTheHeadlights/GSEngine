The style for this codebase is the STL style. We do, however, prefix private variables with m\_.

Do NOT put comments in your code. The code should be self documenting.

See `.Codex/STYLEGUIDE.md` for the full style guide.

## Unit Types

Unit types (e.g., `gap`, `displacement`, `velocity`, `force`, etc.) have the **same memory layout as their underlying float**. They can be passed directly to GPU push constants, `memcpy`, etc. without any cast or extraction — they are layout-compatible with `float`. Never `static_cast` a unit type to float. Just use it directly.

## Logging System

A logging system writes to `Engine/Resources/Misc/log.txt`, cleared on each run. Assertions automatically log failures.

**To debug issues:** Read the log file instead of asking the user to paste console output.

**Implementation:** `Engine/Engine/Import/Log.cppm` - read this file for the current API.

## Config Module

`gse.config` (in `Engine/Engine/Import/Config.cppm.in`) provides CMake-configured paths like `resource_path`. Re-exported by `gse.utility`.

