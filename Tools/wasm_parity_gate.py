"""Compare the browser run of a headless scenario against the native run of the same one.

`parity_gate.py` is the standing CPU<->GPU gate and is native-only by construction: it
takes one `--exe`, launches scenarios with it and compares runs of that binary against
each other. This is the cross-target half, and it reuses three things from it rather
than restating them -- the hash and p50 patterns, which are the output contract, and the
drift-row pattern that reads the engine's own comparator. A second spelling of any of
them is a gate reporting a pass against something only this file knows.

The comparison itself is NOT done here. `--engine-bench-state-dump-out` writes a record
whose format the engine owns (`Engine/Engine/Source/Runtime/StateDump.cppm`, magic and
version 2, little-endian on both arms), and the Sandbox already carries the comparator
for two of them: `--compare-states-a/--compare-states-b`, keyed on (frame, owner) with
`gse::distance` per body. So the wasm dump is handed to the native exe and the arithmetic
is the engine's, which is the only way the number means the same thing the native gate's
does.

Where the arms FIRST part is `wasm_divergence.py`'s bit scan, imported rather than restated:
it reads the two dumps field by field and names the frame, body, field and ulp count of the
first difference. A one-ulp seed in one body is the two arms doing the same arithmetic in a
different order, and then agreement throughout is a question about building the
portable-SIMD native arm; many bodies parting at once, or by more than a rounding step, is a
fidelity defect in the port. The final-frame drift below cannot tell those apart, which is
why the headline is in the artifact beside it. The scan is pure arithmetic over records
already on disk, so it costs no second comparator run.

Both arms are run with `--no-engine-gpu`. The wasm arm has no choice -- no GPU backend is
compiled for it, so `gpu::context::data` is dropped -- and without the same flag natively
the comparison would be measuring the dropped context as well as the arm, and would
attribute both to SIMD.

The browser arm is the wasm build's own `serve.mjs --open`, which serves the page, fetches
each file the run opens from the repo tree and exits with the run's verdict. This driver
only starts it and reads its console.

Usage, from the repo root:

  python Tools/wasm_parity_gate.py

What it does not do is build anything. The wasm arm is whatever the wasm32-wasi-gcc-Release
configuration last built, and the native reference is whatever binary is on disk; `--exe`
names another.
"""

import argparse
import os
import re
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from msys2 import REPO_ROOT
from run_wasm import WASM_BUILD, WASM_MODULE, WASM_PRESET, WASM_SERVER, run_config
from parity_gate import drift_row_pattern, hash_pattern, p50_pattern
from repo_paths import BUILD_ROOT, GENERATED_ROOT
from wasm_divergence import bit_scan, read_dump, scan_bits

# The comparator's own header, and the reason it is parsed rather than trusted.
# `frame_drift` skips an owner that is absent from the other dump, so two dumps whose
# bodies carry disjoint owner ids compare as a perfect match -- zero drift out of zero
# comparisons. `scene_reset_cpu` rebuilds its bodies mid-run, so owner agreement across
# arms is an assumption and not a given, and a gate that cannot tell a match from an
# empty intersection is worse than no gate.
header_pattern = re.compile(r"^([AB]): (\d+) records over (\d+) frames", re.MULTILINE)

SCENARIO = "scene_reset_cpu"
FRAMES = 240
TOLERANCE = 0.002
DUMP_NAME = "parity.dump"

NATIVE_EXE = (BUILD_ROOT / "x64-mingw-gcc-Release" / "Sandbox"
              / "Sandbox.exe")

# How long the page run gets to report its verdict, terminate its browser and remove its
# temp profile after its own timeout has fired. The driver waits this much longer than
# the child so the child is always the one that gives up first -- see `run_page`.
PAGE_CLEANUP_MARGIN = 120.0


@dataclass
class arm_run:
    label: str
    state_hash: str | None
    p50: str | None
    dump: Path | None
    exit_code: int
    seconds: float
    command: str
    timed_out: bool = False


def produced(dump: Path, returncode: int) -> Path | None:
    """The dump this run wrote, or nothing -- never merely the dump that is lying there.

    `fresh()` deletes it before the arm starts, so existence is evidence that THIS
    invocation wrote it; the exit code is required as well because a run can fail
    partway through one. Both, because either alone leaves the gate able to report a
    pass on a previous invocation's pair: a page run that fails before it collects
    anything leaves last time's 1.2 MB file in place, and `[PASS] final 1.796 mm` with
    `exit 1` beside it in the runs table is the false pass this gate exists to make
    impossible.
    """
    return dump if returncode == 0 and dump.is_file() else None


def fresh(*dumps: Path) -> None:
    for dump in dumps:
        dump.unlink(missing_ok=True)


def kill_tree(proc: subprocess.Popen) -> None:
    """Kill a process and its descendants, which `Popen.kill` does not.

    The page run's browser is started by the page run, so it is this driver's grandchild
    and survives its parent's death -- headless, with no window to notice and no timer
    left to stop it.
    """
    if sys.platform == "win32":
        subprocess.run(["taskkill", "/F", "/T", "/PID", str(proc.pid)],
                       capture_output=True, text=True)
    try:
        proc.kill()
    except OSError:
        pass


@dataclass
class comparison:
    records_a: int
    records_b: int
    frames_a: int
    frames_b: int
    final: float
    peak: float
    rows: int
    note: str

    @property
    def comparable(self) -> bool:
        return (self.rows > 0 and self.records_a > 0
                and self.records_a == self.records_b
                and self.frames_a == self.frames_b)

    @property
    def passed(self) -> bool:
        return self.comparable and self.final <= TOLERANCE


def native_environment() -> dict:
    """The gcc runtime on PATH, which `parity_gate.py` does for the same reason.

    The engine is built with a trunk GCC whose libstdc++ lives beside it; without this the
    exe exits 127 before printing anything, which reads as a scenario failure.
    """
    env = dict(os.environ)
    runtime_bin = Path.home() / ".gcc-trunk" / "current" / "bin"
    env["PATH"] = f"{runtime_bin}{os.pathsep}{env.get('PATH', '')}"
    return env


def run_native(exe: Path, dump: Path, timeout: float, gpu: bool) -> arm_run:
    # No `--no-engine-persist-settings`, which `parity_gate.py` passes and this cannot:
    # the engine now rejects a run whose command line names a flag the scenario's own
    # annotation owns, and that is one of them. A scenario run is hermetic anyway, which
    # is what the flag was there to assert.
    cmd = [str(exe), "--engine-bench-scenario", SCENARIO,
           "--engine-bench-state-dump-out", str(dump)]
    if not gpu:
        cmd.insert(1, "--no-engine-gpu")
    fresh(dump)
    start = time.monotonic()
    try:
        proc = subprocess.run(cmd, cwd=REPO_ROOT, env=native_environment(),
                              capture_output=True, text=True, encoding="utf-8",
                              errors="replace", timeout=timeout)
    except subprocess.TimeoutExpired:
        return arm_run("native", None, None, None, -1, time.monotonic() - start,
                       " ".join(cmd), timed_out=True)
    text = proc.stdout + proc.stderr
    found = hash_pattern.search(text)
    p50 = p50_pattern.search(text)
    return arm_run("native", found.group(1) if found else None,
                   p50.group(1) if p50 else None,
                   produced(dump, proc.returncode),
                   proc.returncode, time.monotonic() - start, " ".join(cmd))


def run_page(state_dir: str, out_dir: Path, port: int, timeout: float) -> arm_run:
    """One browser run, through the wasm build's own `serve.mjs`.

    It owns the isolation headers, the file serving, the browser and the verdict.
    """
    guest_dump = f"{state_dir}/{DUMP_NAME}"
    # The budget is forwarded so the CHILD is the one that times out: it started the
    # browser and owns the only cleanup that stops it, and the browser is this driver's
    # grandchild. The margin keeps the driver's wait strictly the longer one.
    cmd = ["node", "--no-warnings", str(WASM_SERVER), "--open",
           "--port", str(port),
           "--timeout", str(int(timeout)),
           "--collect", guest_dump,
           "--collect-out", str(out_dir),
           "--", "--engine-bench-state-dump-out", guest_dump]
    out_dir.mkdir(parents=True, exist_ok=True)
    fresh(out_dir / DUMP_NAME)
    start = time.monotonic()
    proc = subprocess.Popen(cmd, cwd=WASM_BUILD, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, encoding="utf-8",
                            errors="replace")
    try:
        text = proc.communicate(timeout=timeout + PAGE_CLEANUP_MARGIN)[0] or ""
    except subprocess.TimeoutExpired:
        # Reaching here means the child outlived its own timeout plus the margin, so its
        # cleanup is not going to run and the tree is killed from here instead.
        kill_tree(proc)
        text = proc.communicate()[0] or ""
        print(f"  the page run outlived its own {int(timeout)}s budget; killed its "
              f"process tree", flush=True)
        return arm_run("wasm", None, None, None, -1, time.monotonic() - start,
                       " ".join(cmd), timed_out=True)
    # Kept per run, before the relay below, which is the thing here most able to fail: a
    # gate whose artifact cannot be re-read is a gate that has to be re-run to be believed.
    (out_dir / "run-page.log").write_text(text, encoding="utf-8")
    # Through the byte stream with replacement, because the subject's lines carry ANSI
    # colour and whatever the page's own decode could not represent, and this console is
    # cp1252: re-encoding the relay is what crashed the first run of this gate, after a
    # page run that had already succeeded.
    encoding = sys.stdout.encoding or "utf-8"
    sys.stdout.buffer.write(text.encode(encoding, errors="replace"))
    sys.stdout.flush()
    found = hash_pattern.findall(text)
    p50 = p50_pattern.findall(text)
    return arm_run("wasm", found[-1] if found else None, p50[-1] if p50 else None,
                   produced(out_dir / DUMP_NAME, proc.returncode),
                   proc.returncode, time.monotonic() - start, " ".join(cmd))


def compare(exe: Path, a: Path, b: Path, timeout: float) -> comparison:
    # A comparator that hangs is a failed comparison, not a failed gate run: the report
    # is written once at the end, so letting the timeout propagate would lose every run
    # that had already succeeded along with it.
    try:
        proc = subprocess.run(
            [str(exe), "--compare-states-a", str(a), "--compare-states-b", str(b)],
            cwd=REPO_ROOT, env=native_environment(), capture_output=True, text=True,
            encoding="utf-8", errors="replace", timeout=timeout)
    except subprocess.TimeoutExpired:
        return comparison(0, 0, 0, 0, 0.0, 0.0, 0,
                          f"the comparator did not finish within {timeout:.0f}s")
    text = proc.stdout + proc.stderr
    counts = {m[0]: (int(m[1]), int(m[2])) for m in header_pattern.findall(text)}
    rows = drift_row_pattern.findall(text)
    records_a, frames_a = counts.get("A", (0, 0))
    records_b, frames_b = counts.get("B", (0, 0))
    final = float(rows[-1][1]) if rows else 0.0
    peak = max((float(r[1]) for r in rows), default=0.0)
    note = ""
    if not rows:
        note = "the comparator printed no drift rows"
    elif records_a == 0 or records_b == 0:
        note = "one dump carries no records"
    elif records_a != records_b or frames_a != frames_b:
        note = ("the dumps do not describe the same world, so a drift of zero would "
                "mean an empty intersection rather than a match")
    return comparison(records_a, records_b, frames_a, frames_b, final, peak,
                      len(rows), note)


def first_divergence(native: Path, wasm: Path) -> bit_scan | None:
    """Where the two dumps first differ in a bit, or nothing if either cannot be read.

    `read_dump` returns nothing for a file whose header it does not recognise, and raises
    for one that is not there at all -- neither is a reason to lose the runs that have
    already passed, so both end the same way here: no headline for this pair, and the
    drift table beside it unaffected.
    """
    try:
        records_a = read_dump(native)
        records_b = read_dump(wasm)
    except OSError:
        return None
    if records_a is None or records_b is None:
        return None
    return scan_bits(records_a, records_b)


def headline(scan: bit_scan | None) -> str:
    if scan is None:
        return "the dumps could not be read"
    if scan.first_frame is None:
        return scan.note or "no bit difference in any field of any body"
    worst = scan.worst
    if worst is None:
        return (f"frame {scan.first_frame}, {scan.bodies_at_first} of "
                f"{scan.bodies_present} bodies")
    return (f"frame {scan.first_frame}, {scan.bodies_at_first} of "
            f"{scan.bodies_present} bodies, {worst.ulps} ulp"
            f"{'' if worst.ulps == 1 else 's'} of "
            f"{worst.field_name}.{worst.component} on body {worst.owner}")


def one_line(run: arm_run) -> str:
    if run.timed_out:
        return f"TIMEOUT after {run.seconds:.0f}s"
    state = run.state_hash or "no hash"
    p50 = f" p50 {run.p50} ms" if run.p50 else ""
    dump = "" if run.dump else ", no dump"
    return f"exit {run.exit_code}, {state}{p50} ({run.seconds:.1f}s){dump}"


def hash_identity(wasm: list[arm_run], native: list[arm_run]) -> bool:
    """Criterion (3): the two arms printed one hash each and it is the same hash.

    One each is part of it rather than a separate check: a wasm arm that printed two
    different hashes across runs has nothing for a native hash to be identical TO, and
    reporting identity against whichever one happened to match would be the false pass
    `produced()` exists to prevent, one level up.
    """
    wasm_hashes = {r.state_hash for r in wasm if r.state_hash}
    native_hashes = {r.state_hash for r in native if r.state_hash}
    return len(wasm_hashes) == 1 and wasm_hashes == native_hashes


def report(path: Path, exe: Path, wasm: list[arm_run], native: list[arm_run],
           checks: list[comparison], scans: list[bit_scan | None], gpu: bool,
           portable: bool) -> None:
    module = WASM_MODULE
    lines = [
        "# wasm <-> native parity gate",
        "",
        f"Written {time.strftime('%Y-%m-%d %H:%M:%S')} by `Tools/wasm_parity_gate.py`.",
        "",
        f"Scenario `{SCENARIO}`, {FRAMES} measured frames, 0 warmup. Positional budget "
        f"{TOLERANCE} m ({TOLERANCE * 1000:.3f} mm).",
        "",
        "## Subjects",
        "",
        f"- native `{exe}` ({exe.stat().st_size} bytes, mtime "
        f"{time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(exe.stat().st_mtime))})"
        + (", declared to be a `-DGSE_NO_SIMD_X86` portable-SIMD build"
           if portable else ", an ordinary build (x86 SIMD arm, `-mavx2`)"),
    ]
    if module.is_file():
        lines.append(
            f"- wasm `{module}` ({module.stat().st_size} bytes, mtime "
            f"{time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(module.stat().st_mtime))}"
            f", built by the {WASM_PRESET} configuration)")
    lines += [
        f"- `--no-engine-gpu` on both arms: {'yes' if not gpu else 'NO, native ran with the gpu context'}",
        "",
        "## Runs",
        "",
        "| run | arm | hash | p50 ms | detail |",
        "| --- | --- | --- | --- | --- |",
    ]
    for i, (w, n) in enumerate(zip(wasm, native), start=1):
        for run in (w, n):
            lines.append(f"| {i} | {run.label} | `{run.state_hash or '-'}` | "
                         f"{run.p50 or '-'} | {one_line(run)} |")
    lines += [
        "",
        "## Positional agreement, native A against wasm B",
        "",
        "| run | records A | records B | frames | final drift | peak drift | verdict |",
        "| --- | --- | --- | --- | --- | --- | --- |",
    ]
    for i, c in enumerate(checks, start=1):
        verdict = "PASS" if c.passed else ("FAIL " + c.note if c.note else "FAIL")
        lines.append(f"| {i} | {c.records_a} | {c.records_b} | {c.frames_a} | "
                     f"{c.final * 1000:.3f} mm | {c.peak * 1000:.3f} mm | {verdict} |")

    lines += [
        "",
        "## Where the arms first part",
        "",
        "`wasm_divergence.py`'s bit scan over the same two dumps, field by field. The "
        "drift table above is a distance and cannot tell a one-ulp seed from a "
        "reordering; this can.",
        "",
        "| run | first frame | bodies | first difference | fan-out | peak drift |",
        "| --- | --- | --- | --- | --- | --- |",
    ]
    for i, (s, c) in enumerate(zip(scans, checks), start=1):
        if s is None:
            lines.append(f"| {i} | - | - | the dumps could not be read | - | "
                         f"{c.peak * 1000:.3f} mm |")
            continue
        if s.first_frame is None:
            lines.append(f"| {i} | none | - | "
                         f"{s.note or 'no bit difference in any field of any body'} | - | "
                         f"{c.peak * 1000:.3f} mm |")
            continue
        worst = s.worst
        where = (f"{worst.ulps} ulp{'' if worst.ulps == 1 else 's'} of "
                 f"`{worst.field_name}.{worst.component}` on body {worst.owner} "
                 f"({worst.a!r} against {worst.b!r})") if worst else "-"
        fan = " -> ".join(str(n) for n in s.propagation) or "-"
        lines.append(f"| {i} | {s.first_frame} | {s.bodies_at_first} of "
                     f"{s.bodies_present} | {where} | {fan} | "
                     f"{c.peak * 1000:.3f} mm |")
    lines += [
        "",
        "A first difference of a few ulps in one body, fanning out from 1 rather than "
        "starting at the full count, is the two arms doing the same arithmetic in a "
        "different order -- 256-bit intrinsics against wasm's 128 -- and then agreement "
        "throughout is a question about criterion (3)'s portable-SIMD native arm, not a "
        "defect in the port. Many bodies parting at once, a difference larger than a "
        "rounding step, or a difference in a count or an ordering, is the other answer "
        "and is a defect. `Tools/wasm_divergence.py` run over this directory writes the "
        "full per-frame drift curve and the longer argument.",
    ]

    wasm_hashes = {r.state_hash for r in wasm if r.state_hash}
    native_hashes = {r.state_hash for r in native if r.state_hash}
    lines += [
        "",
        "## Criteria",
        "",
        f"- (4) positional agreement at {TOLERANCE} m, require: "
        f"{'MET' if checks and all(c.passed for c in checks) else 'NOT MET'}",
        f"- peak drift, report only: "
        f"{max((c.peak for c in checks), default=0.0) * 1000:.3f} mm. The verdict above "
        f"is on the FINAL frame, which is what `parity_gate.py`'s positional check "
        f"compares and therefore what criterion (4) names. A peak far above the budget "
        f"that settles back under it is the two arms diverging during the strike and "
        f"reconverging as the pile comes to rest -- agreement at rest, not agreement "
        f"throughout. Read both numbers.",
        f"- (5) three consecutive runs: "
        f"{'MET' if len(checks) >= 3 and all(c.passed for c in checks) else 'NOT MET'}",
        f"- (5) hash stable across page reloads: "
        f"{'MET' if len(wasm_hashes) == 1 else 'NOT MET'} "
        f"({', '.join(sorted(wasm_hashes)) or 'none'})",
        f"- native hash across runs: {', '.join(sorted(native_hashes)) or 'none'}",
        f"- (3) hash identity, wasm against this native arm, "
        f"{'require' if portable else 'REPORT ONLY'}: "
        f"{'MET' if hash_identity(wasm, native) else 'NOT MET'} "
        f"({', '.join(sorted(wasm_hashes)) or 'none'} against "
        f"{', '.join(sorted(native_hashes)) or 'none'})",
        "",
    ]
    if portable:
        lines += [
            "The native arm was declared portable-SIMD with `--native-portable`, so (3) "
            "is this run's `require` and is in its exit status. The declaration is a "
            "claim about the binary on disk, which nothing here can verify: a build",
            "configured without `-DGSE_NO_SIMD_X86=ON` would be measured as the portable",
            "arm and a failure attributed to the port rather than to the configuration.",
        ]
    else:
        lines += [
            "`report` and not `require` because the native arm above is an ordinary x86",
            "build: criterion (3) names a `-DGSE_NO_SIMD_X86` one, which runs the same",
            "sequential reductions wasm runs. Against `-mavx2` intrinsics the hashes",
            "differ by construction and the result above carries no information about the",
            "port. `Engine/cmake/GSEEngine.cmake`'s `gse_configure_compiler()` has the",
            "option, off by default; build with it on and pass that exe as `--exe` with",
            "`--native-portable`. Note that `-mavx2` stays on either way, so the portable",
            "arm's scalar loops still autovectorize at 256 bits against wasm's 128:",
            "identity under (3) is an assumption about that arm, not a consequence of it.",
        ]
    lines += [
        "",
        "## Invocations",
        "",
        "```",
    ]
    for run in wasm[:1] + native[:1]:
        lines.append(run.command)
    lines += ["```", ""]
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--exe", type=Path, default=NATIVE_EXE,
                   help="the native reference binary, taken from disk and never built. "
                        "Also the comparator and so the arithmetic both arms are "
                        "measured by. This is how criterion (3)'s portable-SIMD build "
                        "is pointed at, with --native-portable beside it")
    p.add_argument("--runs", type=int, default=3,
                   help="paired runs per arm; criterion (5) asks for three")
    p.add_argument("--out", type=Path, default=GENERATED_ROOT / "out" / "wasm_parity_gate")
    p.add_argument("--port", type=int, default=8713)
    p.add_argument("--page-timeout", type=float, default=1200.0,
                   help="the page run's budget, forwarded to serve.mjs as its "
                        f"own --timeout. This driver waits {PAGE_CLEANUP_MARGIN:.0f}s "
                        "longer, so the child always gives up first and gets to stop "
                        "its browser")
    p.add_argument("--native-timeout", type=float, default=900.0)
    p.add_argument("--native-portable", action="store_true",
                   help="the binary named by --exe is a -DGSE_NO_SIMD_X86 build, so it "
                        "runs the same sequential reductions wasm does. Promotes "
                        "criterion (3), hash identity, from report to require and puts "
                        "it in the exit status. Off against an ordinary -mavx2 build, "
                        "where the hashes differ by construction and a require would "
                        "report a failure of the configuration as a failure of the port")
    p.add_argument("--native-gpu", action="store_true",
                   help="leave the gpu context in place on the native arm. The fallback "
                        "for a native binary that cannot run headless without it, and a "
                        "weaker comparison: the gate then measures the dropped context "
                        "as well as the arm")
    args = p.parse_args()

    if not args.exe.is_file():
        print(f"missing {args.exe}")
        print("an editor build in progress displaces a locked exe to Sandbox.exe.bak; "
              "it returns on the next build")
        return 2
    # The module's entry runs the scenario its configure named, so a build configured for
    # another one would be gated against the wrong native run.
    config = run_config()
    if config is None:
        print(f"refusing to gate: no {WASM_MODULE}; build the {WASM_PRESET} configuration")
        return 2
    built_for = config["scenario"]
    if built_for != SCENARIO:
        print(f"refusing to gate: {WASM_MODULE} runs {built_for}, not {SCENARIO}")
        return 2

    print("== wasm <-> native parity gate ==")
    print(f"  wasm module     {WASM_MODULE}")
    print(f"  scenario        {SCENARIO}, {FRAMES} frames")
    print(f"  native exe      {args.exe}"
          f"{' (declared portable-SIMD)' if args.native_portable else ''}")
    print(f"  budget          {TOLERANCE} m ({TOLERANCE * 1000:.3f} mm)")
    print(f"  runs            {args.runs} per arm")
    print(f"  output          {args.out}")
    print()

    wasm: list[arm_run] = []
    native: list[arm_run] = []
    checks: list[comparison] = []
    scans: list[bit_scan | None] = []

    for i in range(1, args.runs + 1):
        out_dir = args.out / f"run{i}"
        out_dir.mkdir(parents=True, exist_ok=True)

        print(f"run {i}: native", flush=True)
        n = run_native(args.exe, out_dir / "native.dump", args.native_timeout,
                       args.native_gpu)
        native.append(n)
        print(f"  {one_line(n)}", flush=True)

        print(f"run {i}: browser page", flush=True)
        w = run_page(config["environment"]["GSE_STATE_DIR"], out_dir, args.port,
                     args.page_timeout)
        wasm.append(w)
        print(f"  {one_line(w)}", flush=True)

        if n.dump is None or w.dump is None:
            missing = " and ".join(r.label for r in (n, w) if r.dump is None)
            note = f"{missing} wrote no dump in this invocation"
            checks.append(comparison(0, 0, 0, 0, 0.0, 0.0, 0, note))
            scans.append(None)
            print(f"  [FAIL] no comparison: {note}", flush=True)
            continue

        c = compare(args.exe, n.dump, w.dump, args.native_timeout)
        checks.append(c)
        scan = first_divergence(n.dump, w.dump)
        scans.append(scan)
        print(f"  first divergence: {headline(scan)}", flush=True)
        print(f"  [{'PASS' if c.passed else 'FAIL'}] positional: final "
              f"{c.final * 1000:.3f} mm, peak {c.peak * 1000:.3f} mm, budget "
              f"{TOLERANCE * 1000:.3f} mm, {c.records_a} vs {c.records_b} records over "
              f"{c.frames_a} vs {c.frames_b} frames"
              + (f" -- {c.note}" if c.note else ""), flush=True)

    artifact = args.out / "report.md"
    report(artifact, args.exe, wasm, native, checks, scans, args.native_gpu,
           args.native_portable)

    wasm_hashes = {r.state_hash for r in wasm if r.state_hash}
    failed = [c for c in checks if not c.passed]
    identical = hash_identity(wasm, native)

    print()
    print(f"gate: {len(checks)} comparisons, {len(failed)} failures", flush=True)
    print(f"  wasm hash       {', '.join(sorted(wasm_hashes)) or '(none)'}"
          f"{'' if len(wasm_hashes) == 1 else '  <- not stable across runs'}")
    print(f"  native hash     "
          f"{', '.join(sorted({r.state_hash for r in native if r.state_hash})) or '(none)'}")
    weight = "require" if args.native_portable else "report only, x86 native arm"
    print(f"  hash identity   {'MET' if identical else 'NOT MET'} ({weight})")
    print(f"  report          {artifact}")
    if args.native_portable and not identical:
        return 1
    return 1 if failed or len(wasm_hashes) != 1 else 0


if __name__ == "__main__":
    sys.exit(main())
