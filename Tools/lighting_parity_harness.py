import argparse
import json
import math
import os
import re
import shutil
import subprocess
import sys
import time
import traceback
from dataclasses import asdict, dataclass, field
from datetime import datetime, timezone
from pathlib import Path

from frame_parity import compare, comparison, region_breakdown, pixel_threshold, tolerance_max_mean_error, tolerance_max_over_fraction
from repo_paths import BUILD_ROOT, GENERATED_ROOT, REPO_ROOT

scenario = "lighting_parity"
runs_per_config = 3

cross_backend_max_mean_error = 0.5
cross_backend_max_over_fraction = 0.0025

hud_regions = {
    "fps_box": (16, 16, 128, 64),
    "reset_button": (1776, 16, 1904, 68),
    "crosshair": (940, 520, 980, 560),
}

ansi_pattern = re.compile(r"\x1b\[[0-9;]*m")
line_pattern = re.compile(r"^\[[^\]]*\]\[(?P<level>[a-z]+)\]\[(?P<category>[a-z0-9_]+)\]\[[^\]]*\] (?P<message>.*)$")
hash_pattern = re.compile(r"world-state hash (0x[0-9a-fA-F]{16})")
screenshot_pattern = re.compile(r"Screenshot saved: (.+\.png)")
technique_pattern = re.compile(r"^lighting technique: (\w+)$")
helpers_pattern = re.compile(r"^shader program (\S*): helpers (.*)$")
gpu_error_pattern = re.compile(r"webgpu: .* failed validation|Validation Error|device lost|device removed|DXGI_ERROR_DEVICE", re.IGNORECASE)
fallback_pattern = re.compile(r"falling back to (\w+) backend")
gi_capture_pattern = re.compile(r"^gi_probe: capture path (\w+)$")
repeat_pattern =re.compile(r"^\(previous message repeated \d+ times\)$")


@dataclass(frozen=True)
class config:
    name: str
    backend: str
    technique: str
    validation: bool


configs = [
    config("vulkan_rt", "vulkan", "ray_traced", True),
    config("vulkan_raster", "vulkan", "raster", True),
    config("dx12_rt", "dx12", "ray_traced", True),
    config("dx12_raster", "dx12", "raster", True),
    config("webgpu", "webgpu", "ray_traced", False),
]


@dataclass
class run_record:
    config: str
    index: int
    exit_code: int | None
    seconds: float
    backend_requested: str | None = None
    backend_used: str | None = None
    state_hash: str | None = None
    technique: str | None = None
    frame: str | None = None
    errors: list[str] = field(default_factory=list)
    dx12_messages: list[str] = field(default_factory=list)
    other_errors: list[str] = field(default_factory=list)
    ray_traced_programs: list[str] = field(default_factory=list)
    fallback: str | None = None
    gi_capture: str | None = None
    note: str = ""


@dataclass
class check:
    criterion: str
    name: str
    passed: bool
    detail: str


def settings_of(c: config, extra: list[str] = []) -> list[str]:
    settings = [f"Graphics.backend={c.backend}"]
    if c.technique == "raster":
        settings.append("Graphics.lighting_technique=raster")
    if c.validation:
        settings.append("Graphics.validation_layers_enabled=true")
    return settings + extra


def backend_of(settings: list[str]) -> str:
    return [s.split("=", 1)[1] for s in settings if s.startswith("Graphics.backend=")][-1]


def parse_output(record: run_record, text: str) -> str | None:
    screenshot = None
    record.backend_used = record.backend_requested
    for raw in text.splitlines():
        line = ansi_pattern.sub("", raw).strip()
        parsed = line_pattern.match(line)
        if parsed is None:
            continue
        level, category, message = parsed.group("level"), parsed.group("category"), parsed.group("message")
        if found := hash_pattern.search(message):
            record.state_hash = found.group(1)
        if found := screenshot_pattern.search(message):
            screenshot = found.group(1)
        if found := fallback_pattern.search(message):
            record.backend_used = found.group(1)
            record.fallback = message
        if found := gi_capture_pattern.match(message):
            record.gi_capture = found.group(1)
        if found := technique_pattern.match(message):
            record.technique = found.group(1)
        if found := helpers_pattern.match(message):
            if "_ray_traced" in found.group(2):
                record.ray_traced_programs.append(found.group(1))
        if message.startswith("RT shadow:"):
            record.ray_traced_programs.append("rt_shadow system")
        if category == "dx12_validation":
            if not repeat_pattern.match(message):
                record.dx12_messages.append(message)
        elif (category == "vulkan_validation" and level == "error") or gpu_error_pattern.search(message):
            record.errors.append(f"[{level}][{category}] {message}")
        elif level == "error":
            record.other_errors.append(f"[{category}] {message}")
    return screenshot


def run_once(exe: Path, exe_stamp: tuple[int, int], env: dict, c: config, index: int, out_dir: Path, timeout: float, extra: list[str]) -> run_record:
    if not exe.exists() or (exe.stat().st_mtime_ns, exe.stat().st_size) != exe_stamp:
        return run_record(c.name, index, None, 0.0, note="executable changed or vanished during the harness (a build ran)")
    cmd = [str(exe), "--engine-bench-scenario", scenario]
    for setting in settings_of(c, extra):
        cmd += ["--engine-setting", setting]
    start = time.monotonic()
    try:
        proc = subprocess.run(cmd, cwd=REPO_ROOT, env=env, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=timeout)
    except subprocess.TimeoutExpired:
        return run_record(c.name, index, None, time.monotonic() - start, note=f"timed out after {timeout:.0f}s")
    except OSError as error:
        return run_record(c.name, index, None, time.monotonic() - start, note=f"launch failed: {error}")
    record = run_record(c.name, index, proc.returncode, time.monotonic() - start, backend_of(settings_of(c, extra)))
    log_path = out_dir / f"{c.name}_run{index}.log"
    log_path.write_text(ansi_pattern.sub("", proc.stdout + proc.stderr), encoding="utf-8")
    screenshot = parse_output(record, proc.stdout + proc.stderr)
    if screenshot and Path(screenshot).exists():
        frame = out_dir / f"{c.name}_run{index}.png"
        shutil.copyfile(screenshot, frame)
        record.frame = str(frame)
    elif screenshot:
        record.note = f"logged screenshot {screenshot} does not exist"
    missing = [name for name, value in (("output", (proc.stdout + proc.stderr).strip() or None), ("hash", record.state_hash), ("frame", record.frame)) if value is None]
    if missing:
        record.note = "; ".join(filter(None, [record.note, f"no {', '.join(missing)} in the run"]))
    return record


def describe(r: run_record) -> str:
    return f"{r.config} run {r.index}: backend {r.backend_used} exit {r.exit_code} dx12-messages {len(r.dx12_messages)} hash {r.state_hash} technique {r.technique} errors {len(r.errors)} other-errors {len(r.other_errors)} ray-traced {len(r.ray_traced_programs)} gi-capture {r.gi_capture}{' FELL BACK (requested ' + str(r.backend_requested) + '): ' + r.fallback if r.fallback else ''} frame {'yes' if r.frame else 'no'}{' (' + r.note + ')' if r.note else ''} [{r.seconds:.1f}s]"


def clean(r: run_record) -> bool:
    return r.exit_code == 0 and r.state_hash is not None and not r.errors and r.backend_used == r.backend_requested


def one_hash(records: list[run_record]) -> str | None:
    hashes = {r.state_hash for r in records}
    return next(iter(hashes)) if len(hashes) == 1 else None


def within_cross_tolerance(r: comparison) -> bool:
    return not r.note and r.mean_error <= cross_backend_max_mean_error and r.over_fraction <= cross_backend_max_over_fraction


def frame_detail(r: comparison) -> str:
    if r.note:
        return r.note
    psnr = "inf" if math.isinf(r.psnr) else f"{r.psnr:.2f} dB"
    return f"max {r.max_error} mean {r.mean_error:.4f} over8 {r.over_fraction * 100:.3f}% psnr {psnr}"


def native_checks(backend: str, results: dict[str, list[run_record]]) -> list[check]:
    checks = []
    rt = results[f"{backend}_rt"]
    raster = results[f"{backend}_raster"]
    if fell_back := sorted({f"{r.backend_used}: {r.fallback}" for r in rt + raster if r.fallback}):
        checks.append(check("1", f"{backend} runs use the {backend} backend", False, f"{backend} unavailable, fell back to " + "; ".join(fell_back)))
    rt_clean = all(clean(r) for r in rt)
    rt_hash = one_hash(rt)
    checks.append(check("1", f"{backend} ray-traced runs clean, one hash", rt_clean and rt_hash is not None, f"{sum(clean(r) for r in rt)}/{len(rt)} clean, hashes {sorted({str(r.state_hash) for r in rt})}"))
    raster_clean = all(clean(r) and r.technique == "raster" and not r.ray_traced_programs for r in raster)
    checks.append(check("1", f"{backend} raster runs clean, technique raster, no ray-traced program", raster_clean, f"{sum(clean(r) for r in raster)}/{len(raster)} clean, techniques {sorted({str(r.technique) for r in raster})}, ray-traced programs {sorted({p for r in raster for p in r.ray_traced_programs})}"))
    raster_hash = one_hash(raster)
    checks.append(check("1", f"{backend} raster hash equals ray-traced hash", raster_hash is not None and raster_hash == rt_hash, f"rt {rt_hash} raster {raster_hash}"))
    for label, records in (("ray-traced", rt), ("raster", raster)):
        frames = [Path(r.frame) for r in records if r.frame]
        if len(frames) < len(records):
            checks.append(check("1", f"{backend} {label} frames repeat within same-backend tolerance", False, f"{len(frames)}/{len(records)} runs saved a frame"))
            continue
        pairs = [compare(frames[0], f) for f in frames[1:]]
        checks.append(check("1", f"{backend} {label} frames repeat within same-backend tolerance", all(p.within_tolerance for p in pairs), "; ".join(frame_detail(p) for p in pairs)))
    return checks


def webgpu_checks(reference: str, results: dict[str, list[run_record]]) -> tuple[list[check], list[dict]]:
    checks = []
    webgpu = results["webgpu"]
    checks.append(check("2", "webgpu runs exit 0 with no validation or device-lost errors", all(clean(r) for r in webgpu), "; ".join(f"run {r.index}: exit {r.exit_code}, {len(r.errors)} errors{', ' + r.note if r.note else ''}" for r in webgpu)))
    reference_hash = one_hash(results[f"{reference}_rt"])
    webgpu_hashes = [r.state_hash for r in webgpu]
    checks.append(check("3", f"webgpu hash equals {reference} hash", reference_hash is not None and all(h == reference_hash for h in webgpu_hashes), f"{reference} {reference_hash}, webgpu {webgpu_hashes}"))
    reference_frames = [Path(r.frame) for r in results[f"{reference}_raster"] if r.frame]
    measurements = []
    if not reference_frames:
        checks.append(check("4", f"webgpu frame matches the {reference} raster frame", False, f"no {reference} raster frame"))
        return checks, measurements
    passed = True
    details = []
    for r in webgpu:
        if not r.frame:
            passed = False
            details.append(f"run {r.index}: no frame")
            continue
        result = compare(reference_frames[0], Path(r.frame))
        regions = region_breakdown(reference_frames[0], Path(r.frame), hud_regions, Path(r.frame).with_name(f"webgpu_vs_ref_diff_run{r.index}.png"))
        measurements.append(asdict(result) | {"psnr": None if math.isinf(result.psnr) else result.psnr, "within_cross_backend_tolerance": within_cross_tolerance(result), "regions": [asdict(g) for g in regions]})
        passed = passed and within_cross_tolerance(result)
        details.append(f"run {r.index}: {frame_detail(result)}")
    checks.append(check("4", f"webgpu frame matches the {reference} raster frame within the cross-backend tolerance", passed, "; ".join(details)))
    return checks, measurements


def write_markdown(path: Path, header: dict, records: list[run_record], checks: list[check], measurements: list[dict]) -> None:
    lines = [
        f"# `{scenario}` parity report, {header['stamp']}",
        "",
        f"- Executable: `{header['exe']}` (modified {header['exe_modified']})",
        f"- Reference backend for criteria 3 and 4: **{header['reference']}**",
        f"- Same-backend tolerance: mean <= {tolerance_max_mean_error}, pixels over {pixel_threshold} <= {tolerance_max_over_fraction * 100:g}%",
        f"- Cross-backend tolerance: mean <= {cross_backend_max_mean_error}, pixels over {pixel_threshold} <= {cross_backend_max_over_fraction * 100:g}%",
        f"- Extra settings: {', '.join(f'`{s}`' for s in header['settings']) or 'none'}",
        f"- Verdict: **{'PASS' if header['passed'] else 'FAIL'}**",
        "",
        "## Criteria",
        "",
        "| Criterion | Check | Result | Detail |",
        "|---|---|---|---|",
    ]
    lines += [f"| {c.criterion} | {c.name} | {'PASS' if c.passed else 'FAIL'} | {c.detail} |" for c in checks]
    lines += ["", "## Runs", "", "| Config | Run | Backend used | Exit | Hash | Technique | Validation / device-lost errors | Other error lines | DX12 debug-layer messages | Ray-traced programs | GI capture | Frame | Seconds | Note |", "|---|---|---|---|---|---|---|---|---|---|---|---|---|---|"]
    lines += [f"| {r.config} | {r.index} | {r.backend_used} | {r.exit_code} | {r.state_hash} | {r.technique} | {len(r.errors)} | {len(r.other_errors)} | {len(r.dx12_messages)} | {len(r.ray_traced_programs)} | {r.gi_capture} | {Path(r.frame).name if r.frame else '-'} | {r.seconds:.1f} | {r.note} |" for r in records]
    if measurements:
        lines += ["", "## WebGPU difference by region", "", "Regions label pixels only; criterion 4 is judged on the whole frame. Heatmaps: `webgpu_vs_ref_diff_run<N>.png`.", "", "| Run | Region | Pixels | Mean | Pixels over 8 | Share of frame over 8 |", "|---|---|---|---|---|---|"]
        lines += [f"| {i} | {g['name']} | {g['pixels']} | {g['mean_error']:.4f} | {g['over_pixels']} | {g['over_fraction_of_frame'] * 100:.3f}% |" for i, m in enumerate(measurements, 1) for g in m["regions"]]
    distinct = sorted({m.split(": ")[0] for r in records for m in r.dx12_messages})
    if distinct:
        lines += ["", "## DX12 debug-layer messages", "", "The engine forwards D3D12 errors and warnings to `dx12_validation` without their severity, so the harness lists them here and does not count them as errors. Distinct sources:", ""]
        lines += [f"- `{d}`" for d in distinct]
    noisy = [r for r in records if r.other_errors]
    if noisy:
        lines += ["", "## Other error lines (not validation, not counted)", ""]
        for r in noisy:
            lines.append(f"- {r.config} run {r.index}: " + "; ".join(sorted({f"`{e[:120]}`" for e in r.other_errors})[:4]))
    errored = [r for r in records if r.errors]
    if errored:
        lines += ["", "## First errors per run", ""]
        for r in errored:
            lines.append(f"- {r.config} run {r.index}:")
            lines += [f"  - `{e[:300]}`" for e in r.errors[:5]]
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=f"Runs {scenario} {runs_per_config} times on each backend and lighting technique, one run at a time, and judges the WebGPU goal's pass criteria 1-5 from the logs and the presented frames.")
    parser.add_argument("--exe", type=Path, default=BUILD_ROOT / "x64-mingw-gcc-Release" / "Sandbox" / "Sandbox.exe")
    parser.add_argument("--reference", choices=["dx12", "vulkan"], default="dx12")
    parser.add_argument("--timeout", type=float, default=600.0)
    parser.add_argument("--out", type=Path, default=GENERATED_ROOT / "parity_reports")
    parser.add_argument("--setting", action="append", default=[], metavar="KEY=VALUE")
    args = parser.parse_args()
    if malformed := [s for s in args.setting if "=" not in s]:
        parser.error(f"--setting needs KEY=VALUE: {malformed}")

    stamp = datetime.now(timezone.utc).strftime("%Y%m%d_%H%M%S")
    out_dir = args.out / stamp
    out_dir.mkdir(parents=True, exist_ok=True)
    print(f"harness {stamp}: exe {args.exe}, report dir {out_dir}", file=sys.stderr, flush=True)

    results: dict[str, list[run_record]] = {c.name: [] for c in configs}
    stage = "startup"
    try:
        if not args.exe.exists():
            return fail(out_dir, stage, f"missing {args.exe}", results)
        pinned = args.exe.with_name(f"{args.exe.stem}.parity{args.exe.suffix}")
        shutil.copy2(args.exe, pinned)
        args.exe = pinned
        exe_stat = args.exe.stat()
        exe_stamp = (exe_stat.st_mtime_ns, exe_stat.st_size)

        env = dict(os.environ)
        env["PATH"] = f"{Path.home() / '.gcc-trunk' / 'current' / 'bin'}{os.pathsep}{env.get('PATH', '')}"

        for c in configs:
            for index in range(1, runs_per_config + 1):
                stage = f"{c.name} run {index}"
                record = run_once(args.exe, exe_stamp, env, c, index, out_dir, args.timeout, args.setting)
                results[c.name].append(record)
                print(describe(record), flush=True)
                if not clean(record):
                    print(f"unclean: {describe(record)}", file=sys.stderr, flush=True)
                (out_dir / "partial.json").write_text(json.dumps([asdict(r) for records in results.values() for r in records], indent=2), encoding="utf-8")

        stage = "judging"
        return judge(args, out_dir, stamp, exe_stat, results)
    except Exception:
        return fail(out_dir, stage, traceback.format_exc(), results)


def fail(out_dir: Path, stage: str, detail: str, results: dict[str, list[run_record]]) -> int:
    print(f"harness failed during {stage}: {detail}", file=sys.stderr, flush=True)
    (out_dir / "failure.json").write_text(json.dumps({"stage": stage, "detail": detail, "runs": [asdict(r) for records in results.values() for r in records]}, indent=2), encoding="utf-8")
    return 2


def judge(args: argparse.Namespace, out_dir: Path, stamp: str, exe_stat: os.stat_result, results: dict[str, list[run_record]]) -> int:
    checks = native_checks("vulkan", results) + native_checks("dx12", results)
    webgpu, measurements = webgpu_checks(args.reference, results)
    checks += webgpu
    checks.append(check("5", f"criteria 1-4 hold on {runs_per_config} consecutive runs", all(c.passed for c in checks), f"{sum(not c.passed for c in checks)} failing checks"))

    passed = all(c.passed for c in checks)
    records = [r for c in configs for r in results[c.name]]
    header = {
        "stamp": stamp,
        "scenario": scenario,
        "exe": str(args.exe),
        "exe_modified": datetime.fromtimestamp(exe_stat.st_mtime, timezone.utc).isoformat(),
        "reference": args.reference,
        "passed": passed,
        "settings": args.setting,
    }
    (out_dir / "report.json").write_text(json.dumps(header | {
        "tolerance": {
            "pixel_threshold": pixel_threshold,
            "same_backend": {"max_mean_error": tolerance_max_mean_error, "max_over_fraction": tolerance_max_over_fraction},
            "cross_backend": {"max_mean_error": cross_backend_max_mean_error, "max_over_fraction": cross_backend_max_over_fraction},
        },
        "checks": [asdict(c) for c in checks],
        "webgpu_frame_vs_reference": measurements,
        "runs": [asdict(r) for r in records],
    }, indent=2), encoding="utf-8")
    write_markdown(out_dir / "report.md", header, records, checks, measurements)

    print()
    for c in checks:
        print(f"[{'PASS' if c.passed else 'FAIL'}] {c.criterion} {c.name}: {c.detail}", flush=True)
    print(f"report: {out_dir / 'report.md'}")
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
