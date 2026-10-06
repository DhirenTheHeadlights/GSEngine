import argparse
import os
import re
import struct
import subprocess
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path

hash_pattern = re.compile(r"world-state hash (0x[0-9a-fA-F]{16})")
p50_pattern = re.compile(r"p50 ([0-9.]+) ms")
drift_row_pattern = re.compile(r"^\s*(\d+)\s+([0-9.]+) m\s+(\d+)\s+\d+", re.MULTILINE)

state_dump_record = struct.Struct("<QI13f")
toppled_height = 0.7


def fnv1a64(s: str) -> int:
    h = 0xCBF29CE484222325
    for b in s.encode():
        h ^= b
        h = (h * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h


domino_owners = {fnv1a64(f"Domino {i}"): i for i in range(1, 13)}


def read_domino_metrics(path: Path) -> dict[int, tuple[float, int | None]] | None:
    data = path.read_bytes()
    off = next((c for c in (12, 8, 16, 0) if len(data) > c and (len(data) - c) % state_dump_record.size == 0), None)
    if off is None:
        return None
    latest: dict[int, tuple[int, float]] = {}
    arrival: dict[int, int] = {}
    payload = len(data) - off
    for rec in state_dump_record.iter_unpack(data[off : off + payload]):
        index = domino_owners.get(rec[0])
        if index is None:
            continue
        frame, y = rec[1], rec[3]
        prev = latest.get(index)
        if prev is None or frame >= prev[0]:
            latest[index] = (frame, y)
        if y < toppled_height and (index not in arrival or frame < arrival[index]):
            arrival[index] = frame
    return {i: (fy[1], arrival.get(i)) for i, fy in latest.items()}

require = "require"
report = "report"


@dataclass
class gate_entry:
    name: str
    cpu: str | None
    gpu: str | None
    frames: int | None = None
    warmup: int | None = None
    settings: tuple[str, ...] = ()
    gpu_determinism: dict = field(default_factory=dict)
    identity: dict = field(default_factory=dict)
    positional: dict = field(default_factory=dict)
    positional_tolerance: float = 0.0
    cascade: dict = field(default_factory=dict)


entries = [
    gate_entry(
        name="drop",
        cpu="parity_drop_cpu",
        gpu="parity_drop_gpu",
        frames=300,
        gpu_determinism={"vulkan": require, "dx12": require},
        identity={"vulkan": report, "dx12": report},
        positional={"vulkan": require, "dx12": require},
        positional_tolerance=0.00001,
    ),
    gate_entry(
        name="pair",
        cpu="parity_pair_cpu",
        gpu="parity_pair_gpu",
        frames=300,
        gpu_determinism={"vulkan": require, "dx12": require},
        identity={"vulkan": report, "dx12": report},
        positional={"vulkan": require, "dx12": require},
        positional_tolerance=0.00001,
    ),
    gate_entry(
        name="stack",
        cpu="parity_stack_cpu",
        gpu="parity_stack_gpu",
        frames=300,
        gpu_determinism={"vulkan": require, "dx12": require},
        positional={"vulkan": require, "dx12": require},
        positional_tolerance=0.002,
    ),
    gate_entry(
        name="shapes",
        cpu="parity_shapes_cpu",
        gpu="parity_shapes_gpu",
        frames=300,
        gpu_determinism={"vulkan": report, "dx12": report},
        identity={"vulkan": report, "dx12": report},
    ),
    gate_entry(
        name="domino",
        cpu="parity_domino_cpu",
        gpu="parity_domino_gpu",
        frames=600,
        gpu_determinism={"vulkan": require, "dx12": require},
        cascade={"vulkan": require, "dx12": require},
    ),
    gate_entry(
        name="cluster",
        cpu="parity_cluster_cpu",
        gpu="parity_cluster_gpu",
        frames=300,
        gpu_determinism={"vulkan": require, "dx12": require},
    ),
    gate_entry(
        name="heap",
        cpu="parity_heap_cpu",
        gpu="parity_heap_gpu",
        frames=600,
        gpu_determinism={"vulkan": require, "dx12": require},
    ),
    gate_entry(
        name="mound",
        cpu="parity_mound_cpu",
        gpu="parity_mound_gpu",
        frames=900,
        gpu_determinism={"vulkan": require, "dx12": require},
    ),
    gate_entry(
        name="pile",
        cpu="parity_pile_cpu",
        gpu="parity_pile_gpu",
        frames=300,
        gpu_determinism={"vulkan": report, "dx12": report},
    ),
    gate_entry(
        name="overlap",
        cpu="parity_overlap_cpu",
        gpu="parity_overlap_gpu",
        frames=300,
        gpu_determinism={"vulkan": report, "dx12": report},
    ),
    gate_entry(
        name="pyramid",
        cpu="pyramid_cpu",
        gpu="pyramid_gpu",
        frames=300,
        settings=("Dev Spawn.pyramid.base_count=20",),
        gpu_determinism={"vulkan": require, "dx12": require},
        positional={"vulkan": require, "dx12": require},
        positional_tolerance=0.005,
    ),
    gate_entry(
        name="stress",
        cpu="physics_stress",
        gpu="parity_stress_gpu",
        warmup=0,
        gpu_determinism={"vulkan": report, "dx12": report},
    ),
]


@dataclass
class run_result:
    state_hash: str | None
    p50: str | None
    exit_code: int
    seconds: float


@dataclass
class check_result:
    entry: str
    config: str
    kind: str
    level: str
    passed: bool
    detail: str


def run_scenario(exe: Path, cwd: Path, env: dict, scenario: str, frames: int | None, settings: tuple[str, ...], backend: str | None, timeout: float, dump: Path | None = None, warmup: int | None = None) -> run_result:
    cmd = [str(exe), "--engine-bench-scenario", scenario, "--no-engine-persist-settings"]
    if dump is not None:
        cmd += ["--engine-bench-state-dump-out", str(dump)]
    if frames is not None:
        cmd += ["--engine-bench-frames", str(frames)]
    if warmup is not None:
        cmd += ["--engine-bench-warmup-frames", str(warmup)]
    for setting in settings:
        cmd += ["--engine-setting", setting]
    if backend is not None:
        cmd += ["--engine-setting", f"Graphics.backend={backend}"]
    start = time.monotonic()
    try:
        proc = subprocess.run(cmd, cwd=cwd, env=env, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=timeout)
    except subprocess.TimeoutExpired:
        return run_result(None, None, -1, time.monotonic() - start)
    text = proc.stdout + proc.stderr
    hash_match = hash_pattern.search(text)
    p50_match = p50_pattern.search(text)
    return run_result(
        hash_match.group(1) if hash_match else None,
        p50_match.group(1) if p50_match else None,
        proc.returncode,
        time.monotonic() - start,
    )


def run_config(exe: Path, cwd: Path, env: dict, label: str, scenario: str, frames: int | None, settings: tuple[str, ...], backend: str | None, runs: int, timeout: float, warmup: int | None = None) -> list[run_result]:
    results = []
    for i in range(runs):
        result = run_scenario(exe, cwd, env, scenario, frames, settings, backend, timeout, warmup=warmup)
        if result.exit_code == -1:
            print(f"  {label} {scenario} run {i + 1}/{runs}: TIMEOUT after {result.seconds:.0f}s", flush=True)
        elif result.state_hash is None:
            print(f"  {label} {scenario} run {i + 1}/{runs}: NO HASH (exit {result.exit_code}, {result.seconds:.1f}s)", flush=True)
            if result.exit_code == 127:
                print("    exit 127 usually means the gcc runtime DLLs are missing from PATH", flush=True)
        else:
            p50 = f" p50 {result.p50} ms" if result.p50 else ""
            print(f"  {label} {scenario} run {i + 1}/{runs}: {result.state_hash}{p50} ({result.seconds:.1f}s)", flush=True)
        results.append(result)
    return results


def determinism_check(entry_name: str, config: str, level: str, results: list[run_result]) -> check_result:
    hashes = [r.state_hash for r in results]
    if any(h is None for h in hashes):
        return check_result(entry_name, config, "determinism", level, False, "run produced no hash")
    if len(set(hashes)) == 1:
        return check_result(entry_name, config, "determinism", level, True, f"{len(hashes)} runs, one hash")
    return check_result(entry_name, config, "determinism", level, False, f"{len(set(hashes))} distinct hashes in {len(hashes)} runs: {' '.join(sorted(set(hashes)))}")


def identity_check(entry_name: str, config: str, level: str, cpu_results: list[run_result], gpu_results: list[run_result]) -> check_result:
    cpu_hash = cpu_results[0].state_hash
    gpu_hash = gpu_results[0].state_hash
    if cpu_hash is None or gpu_hash is None:
        return check_result(entry_name, config, "cpu-gpu identity", level, False, "run produced no hash")
    if cpu_hash == gpu_hash:
        return check_result(entry_name, config, "cpu-gpu identity", level, True, cpu_hash)
    return check_result(entry_name, config, "cpu-gpu identity", level, False, f"cpu {cpu_hash} vs gpu {gpu_hash}")


def positional_check(exe: Path, cwd: Path, env: dict, entry: gate_entry, config: str, level: str, backend: str, scratch: Path, timeout: float) -> check_result:
    cpu_dump = scratch / f"gate_{entry.name}_cpu.dump"
    gpu_dump = scratch / f"gate_{entry.name}_{backend}.dump"
    cpu_run = run_scenario(exe, cwd, env, entry.cpu, entry.frames, entry.settings, None, timeout, cpu_dump, entry.warmup)
    gpu_run = run_scenario(exe, cwd, env, entry.gpu, entry.frames, entry.settings, backend, timeout, gpu_dump, entry.warmup)
    if cpu_run.exit_code != 0 or gpu_run.exit_code != 0:
        return check_result(entry.name, config, "positional", level, False, "dump run failed")
    proc = subprocess.run(
        [str(exe), "--compare-states-a", str(cpu_dump), "--compare-states-b", str(gpu_dump)],
        cwd=cwd, env=env, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=timeout
    )
    rows = drift_row_pattern.findall(proc.stdout + proc.stderr)
    if not rows:
        return check_result(entry.name, config, "positional", level, False, "comparator produced no rows")
    final = float(rows[-1][1])
    peak = max(float(r[1]) for r in rows)
    detail = f"final {final * 1000:.3f} mm (peak {peak * 1000:.3f} mm, budget {entry.positional_tolerance * 1000:.3f} mm)"
    return check_result(entry.name, config, "positional", level, final <= entry.positional_tolerance, detail)


def cascade_check(exe: Path, cwd: Path, env: dict, entry: gate_entry, config: str, level: str, backend: str, scratch: Path, timeout: float) -> check_result:
    cpu_dump = scratch / f"gate_{entry.name}_cascade_cpu.dump"
    gpu_dump = scratch / f"gate_{entry.name}_cascade_{backend}.dump"
    cpu_run = run_scenario(exe, cwd, env, entry.cpu, entry.frames, entry.settings, None, timeout, cpu_dump, entry.warmup)
    gpu_run = run_scenario(exe, cwd, env, entry.gpu, entry.frames, entry.settings, backend, timeout, gpu_dump, entry.warmup)
    if cpu_run.exit_code != 0 or gpu_run.exit_code != 0:
        return check_result(entry.name, config, "cascade", level, False, "dump run failed")
    cpu_metrics = read_domino_metrics(cpu_dump)
    gpu_metrics = read_domino_metrics(gpu_dump)
    if not cpu_metrics or not gpu_metrics:
        return check_result(entry.name, config, "cascade", level, False, "dump parse failed or no dominoes found")
    expected = len(domino_owners)
    cpu_toppled = sum(1 for fy, _ in cpu_metrics.values() if fy < toppled_height)
    gpu_toppled = sum(1 for fy, _ in gpu_metrics.values() if fy < toppled_height)
    shifts = [
        gpu_metrics[i][1] - cpu_metrics[i][1]
        for i in domino_owners.values()
        if cpu_metrics.get(i, (9.9, None))[1] is not None and gpu_metrics.get(i, (9.9, None))[1] is not None
    ]
    worst_shift = max((abs(s) for s in shifts), default=None)
    last = max(domino_owners.values())
    cpu_last = cpu_metrics.get(last, (0.0, None))[1]
    gpu_last = gpu_metrics.get(last, (0.0, None))[1]
    detail = (
        f"toppled cpu {cpu_toppled}/{expected} gpu {gpu_toppled}/{expected}, "
        f"wave-arrival D{last} cpu {cpu_last} gpu {gpu_last}, worst arrival shift {worst_shift}"
    )
    passed = cpu_toppled == expected and gpu_toppled == expected
    return check_result(entry.name, config, "cascade", level, passed, detail)


def print_check(check: check_result) -> None:
    if check.level == require:
        tag = "PASS" if check.passed else "FAIL"
    else:
        tag = "held" if check.passed else "varied"
    print(f"  [{tag}] {check.config} {check.kind}: {check.detail}", flush=True)


def main() -> int:
    repo_root = Path(__file__).resolve().parents[1]
    default_exe = repo_root / "out" / "build" / "x64-mingw-gcc-RelWithDebInfo" / "Sandbox" / "Sandbox.exe"

    parser = argparse.ArgumentParser(description="Standing CPU<->GPU parity regression gate: runs the parity ladder + physics_stress, checks world-state hash determinism per config and CPU<->GPU identity where established. Hash checks are load-immune (fixed-step sim), so the gate does not need a quiet machine; printed p50s are informational only.")
    parser.add_argument("--exe", type=Path, default=default_exe)
    parser.add_argument("--backend", choices=["vulkan", "dx12", "both"], default="vulkan")
    parser.add_argument("--runs", type=int, default=2)
    parser.add_argument("--only", type=str, default=None, help="comma-separated entry names")
    parser.add_argument("--timeout", type=float, default=600.0)
    parser.add_argument("--list", action="store_true")
    parser.add_argument("--extra-setting", action="append", default=[], help="Category.key=value appended to every scenario run (repeatable)")
    args = parser.parse_args()

    if args.extra_setting:
        extra = tuple(args.extra_setting)
        for entry in entries:
            entry.settings = entry.settings + extra

    selected = entries
    if args.only:
        wanted = {n.strip() for n in args.only.split(",")}
        unknown = wanted - {e.name for e in entries}
        if unknown:
            print(f"unknown entries: {', '.join(sorted(unknown))}")
            print(f"available: {', '.join(e.name for e in entries)}")
            return 2
        selected = [e for e in entries if e.name in wanted]

    if args.list:
        for entry in entries:
            print(f"{entry.name}: cpu={entry.cpu} gpu={entry.gpu} frames={entry.frames or 'default'} gpu_det={entry.gpu_determinism} identity={entry.identity}")
        return 0

    if not args.exe.exists():
        print(f"missing {args.exe}")
        print("an editor build in progress displaces a locked exe to Sandbox.exe.bak; it returns on the next build")
        return 2

    env = dict(os.environ)
    runtime_bin = Path.home() / ".gcc-trunk" / "current" / "bin"
    env["PATH"] = f"{runtime_bin}{os.pathsep}{env.get('PATH', '')}"

    backends = ["vulkan", "dx12"] if args.backend == "both" else [args.backend]
    checks: list[check_result] = []

    scratch = repo_root / "out" / "parity_gate_dumps"
    scratch.mkdir(parents=True, exist_ok=True)

    for entry in selected:
        print(f"{entry.name}", flush=True)
        cpu_results = None
        if entry.cpu:
            cpu_results = run_config(args.exe, repo_root, env, "cpu", entry.cpu, entry.frames, entry.settings, None, args.runs, args.timeout, entry.warmup)
            check = determinism_check(entry.name, "cpu", require, cpu_results)
            checks.append(check)
            print_check(check)
        if entry.gpu:
            for backend in backends:
                config = f"gpu[{backend}]"
                gpu_results = run_config(args.exe, repo_root, env, config, entry.gpu, entry.frames, entry.settings, backend, args.runs, args.timeout, entry.warmup)
                det_level = entry.gpu_determinism.get(backend, report)
                check = determinism_check(entry.name, config, det_level, gpu_results)
                checks.append(check)
                print_check(check)
                identity_level = entry.identity.get(backend)
                if identity_level and cpu_results:
                    check = identity_check(entry.name, f"cpu vs {config}", identity_level, cpu_results, gpu_results)
                    checks.append(check)
                    print_check(check)
                positional_level = entry.positional.get(backend)
                if positional_level and entry.cpu:
                    check = positional_check(args.exe, repo_root, env, entry, f"cpu vs {config}", positional_level, backend, scratch, args.timeout)
                    checks.append(check)
                    print_check(check)
                cascade_level = entry.cascade.get(backend)
                if cascade_level and entry.cpu:
                    check = cascade_check(args.exe, repo_root, env, entry, f"cpu vs {config}", cascade_level, backend, scratch, args.timeout)
                    checks.append(check)
                    print_check(check)

    failed = [c for c in checks if c.level == require and not c.passed]
    held = [c for c in checks if c.level == report and c.passed]
    varied = [c for c in checks if c.level == report and not c.passed]

    print(flush=True)
    print(f"gate: {len(checks)} checks, {len(failed)} required failures, {len(varied)} report-only variations", flush=True)
    for c in failed:
        print(f"  FAIL {c.entry} {c.config} {c.kind}: {c.detail}", flush=True)
    for c in varied:
        print(f"  varied {c.entry} {c.config} {c.kind}: {c.detail}", flush=True)
    if held and not failed and not varied:
        print(f"  every report-only check held; promotion candidates: {', '.join(sorted({f'{c.entry} {c.config}' for c in held}))}", flush=True)

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
