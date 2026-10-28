"""Convenience wrapper over the wasm32 build's own launcher, and the toolchain probes' harness.

The `wasm32-wasi-gcc-Release` preset writes `Sandbox.mjs` beside `Sandbox.wasm`, so the
build runs without this file:

    node .generated/out/build/wasm32-wasi-gcc-Release/Sandbox/Sandbox.mjs [ARGS]

This runs that launcher and reports the hash and p50 lines it printed. It builds nothing:
build the wasm preset with `gse_build` first, and build natively too, because the run
resolves its engine paths and baked resources from the native build tree the preset's
GSE_WASM_NATIVE_BUILD names.

The toolchain builder's probes and wasm_dialect_probe.py write `harness.mjs` with
`write_harness`, which imports the same `Engine/cmake/wasm/node_host.mjs` the launcher
does, so a probe's verdict and a scenario's run are read through one host.

    python Tools/run_wasm.py [--run-args "ARGS"]
"""
import argparse
import json
import subprocess
from pathlib import Path

from msys2 import REPO_ROOT
from repo_paths import BUILD_ROOT
from parity_gate import hash_pattern, p50_pattern

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

SCENARIO_TIMEOUT_SECONDS = 900

WASM_PRESET = "wasm32-wasi-gcc-Release"
WASM_BUILD = BUILD_ROOT / WASM_PRESET / "Sandbox"
WASM_MODULE = WASM_BUILD / "Sandbox.wasm"
WASM_LAUNCHER = WASM_BUILD / "Sandbox.mjs"
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


def run_scenario(run_args: str = "") -> int:
    config = run_config()
    if config is None:
        raise SystemExit(f"no {WASM_MODULE} or {WASM_RUN_CONFIG.name}: build the "
                         f"{WASM_PRESET} configuration first")
    command = ["node", "--no-warnings", str(WASM_LAUNCHER), *run_args.split()]
    try:
        proc = subprocess.run(command, cwd=WASM_BUILD, capture_output=True, text=True,
                              encoding="utf-8", errors="replace",
                              timeout=SCENARIO_TIMEOUT_SECONDS)
        code, console = proc.returncode, proc.stdout + proc.stderr
    except subprocess.TimeoutExpired as e:
        code, console = 124, (e.stdout or b"").decode("utf-8", errors="replace")
    hashes = hash_pattern.findall(console)
    p50s = p50_pattern.findall(console)

    print(console)
    print("== scenario run ==")
    print(f"  scenario        {config['scenario']}")
    print(f"  launcher        {WASM_LAUNCHER}")
    print(f"  node exit       {code}" + {
        0: " (ran, entry returned 0)",
        TRAP_EXIT: " (trapped)",
        NO_INSTANCE_EXIT: " (did not instantiate)",
        124: f" (no exit within {SCENARIO_TIMEOUT_SECONDS}s)",
    }.get(code, " (ran, entry returned non-zero or node failed)"))
    print(f"  world-state hash {hashes[-1] if hashes else '(none printed)'}")
    print(f"  p50             {p50s[-1] + ' ms' if p50s else '(none printed)'}")
    return code


def main() -> None:
    p = argparse.ArgumentParser(description=f"Run the {WASM_PRESET} build's own launcher "
                                            f"under node and report its hash. Builds nothing")
    p.add_argument("--run-args", metavar="ARGS", default="",
                   help="Extra command-line arguments for the module, space separated")
    args = p.parse_args()
    raise SystemExit(run_scenario(args.run_args))


if __name__ == "__main__":
    main()
