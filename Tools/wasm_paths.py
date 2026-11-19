"""The wasm32 build's paths and the toolchain probes' harness, shared by the wasm tools.

Running the build is not done here. In the editor, pick the `wasm32-wasi-gcc-Release`
configuration in a build profile; its "Run in browser" option opens it in Chrome. Outside
the editor the preset's own launchers run it:

    node .generated/out/build/wasm32-wasi-gcc-Release/Sandbox/Sandbox.mjs [ARGS]
    node .generated/out/build/wasm32-wasi-gcc-Release/Sandbox/serve.mjs --open --headed

The toolchain builder's probes and wasm_dialect_probe.py write `harness.mjs` with
`write_harness`, which imports the same `Engine/cmake/wasm/node_host.mjs` the launcher
does, so a probe's verdict and a scenario's run are read through one host.
"""
import json
from pathlib import Path

from msys2 import REPO_ROOT
from repo_paths import BUILD_ROOT

NODE_HOST = REPO_ROOT / "Engine" / "cmake" / "wasm" / "node_host.mjs"

# Every run probe exports this and returns an int, so the harness needs no per-probe
# configuration.
ENTRY = "entry"

# The probes' memory, in wasm pages. The engine's lives in the wasm toolchain file
# (GSE_WASM_INITIAL_PAGES / GSE_WASM_MAX_PAGES), which its link and its launcher read; a
# probe links and instantiates against these instead.
PAGE_SIZE = 65536
HARNESS_INITIAL_PAGES = 256
HARNESS_MAX_PAGES = 8192

# node_host.mjs's exit codes for a module that trapped or did not instantiate.
TRAP_EXIT = 2
NO_INSTANCE_EXIT = 3

WASM_PRESET = "wasm32-wasi-gcc-Release"
WASM_BUILD = BUILD_ROOT / WASM_PRESET / "Sandbox"
WASM_MODULE = WASM_BUILD / "Sandbox.wasm"
WASM_SERVER = WASM_BUILD / "serve.mjs"
WASM_RUN_CONFIG = WASM_BUILD / "wasm_run.json"


def harness_text() -> str:
    """A probe harness: `node harness.mjs MODULE WANT [shared]`, hosted by node_host.mjs."""
    return (f'import {{ run }} from "{NODE_HOST.as_uri()}";\n'
            f'await run ({{ module: process.argv[2], want: Number (process.argv[3]),\n'
            f'              shared: process.argv[4] === "shared", entry: "{ENTRY}",\n'
            f'              initial_pages: {HARNESS_INITIAL_PAGES}, '
            f'max_pages: {HARNESS_MAX_PAGES} }});\n')


def write_harness(directory: Path) -> None:
    """Write the probe harness as `harness.mjs` where a probe run will invoke it."""
    directory.mkdir(parents=True, exist_ok=True)
    with open(directory / "harness.mjs", "w", encoding="utf-8", newline="\n") as f:
        f.write(harness_text())


def run_config() -> dict | None:
    """The run configuration the wasm preset wrote beside its module, or None without one."""
    if not WASM_MODULE.is_file() or not WASM_RUN_CONFIG.is_file():
        return None
    return json.loads(WASM_RUN_CONFIG.read_text(encoding="utf-8"))
