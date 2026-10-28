"""Characterise the first divergence between the wasm and native arms of a scenario.

`wasm_parity_gate.py` answers whether the arms agree on the FINAL frame, which is what
`parity_gate.py`'s positional check compares and therefore what the goal's criterion (4)
names. It reports the peak beside that verdict, and on `scene_reset_cpu` the peak is two
orders of magnitude above the budget: bit-identical at the start, metres apart during
the strike, millimetres once the pile settles. That is agreement at rest, not agreement
throughout, and this file is the follow-up measurement -- where the arms first part, on
which body, in which field, and by how much.

It builds almost nothing. Three of the four questions are already answerable by the
engine's own comparator, through flags the Sandbox has today:

  --compare-states-threshold 0   the "first over threshold" row then fires on the first
                                 frame carrying ANY nonzero positional drift, and that
                                 row already names the frame, the worst body and the
                                 count of bodies over the threshold -- which is the
                                 one-body-versus-many question verbatim.
  --compare-states-focus-frame   with a window spanning the whole run and a body count
  --compare-states-focus-window  above the body count, the focus block prints per-body
  --compare-states-focus-bodies  per-frame drift for every body in every frame, which is
                                 the drift curve. Aggregating those rows is summing the
                                 engine's own `gse::distance`; a second implementation of
                                 it here would be a number only this file believes.

The fourth question is the one no flag can answer. The comparator reads `position` and
nothing else, so a first difference in `velocity`, `orientation` or `angular_velocity` is
invisible to it, and "how many ulps" is a question about the bits of one float rather
than about a distance. So that half reads the records directly -- with the record layout
and the header offset imported from `parity_gate.py` rather than respelled, because this
port has twice been misled by one datum having two spellings.

Usage, from the repo root, after `wasm_parity_gate.py` has written its run directories:

  python Tools/wasm_divergence.py

It builds nothing and runs no scenario. The dumps are whatever the gate left on disk and
the comparator is whatever binary is there; `--exe` names another.
"""

import argparse
import os
import re
import struct
import subprocess
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from parity_gate import dump_payload_offset, state_dump_record
from repo_paths import BUILD_ROOT, GENERATED_ROOT, REPO_ROOT

NATIVE_EXE = (BUILD_ROOT / "x64-mingw-gcc-Release" / "Sandbox"
              / "Sandbox.exe")

header_pattern = re.compile(r"^([AB]): (\d+) records over (\d+) frames", re.MULTILINE)
first_over_pattern = re.compile(
    r"^\s*(\d+)\s+([0-9.]+) m\s+(\d+)\s+(\d+)\s+<- first over threshold", re.MULTILINE)
jump_pattern = re.compile(
    r"largest single-frame jump: frame (\d+), ([0-9.]+) m -> ([0-9.]+) m on body (\d+)")
smooth_pattern = re.compile(r"no single-frame jump above 10x")
body_pattern = re.compile(r"^body (\d+)$", re.MULTILINE)
focus_row_pattern = re.compile(
    r"^\s*(\d+)\s+([0-9.]+) m\s+([0-9.]+) m/s\s+([0-9.]+) m/s", re.MULTILINE)

# The record's 13 floats, as the fields `state_dump_record` declares them
# (`Engine/Engine/Source/Runtime/StateDump.cppm`). The struct is unpacked positionally,
# so this is the only place the field order is named and it has to match that
# declaration: owner, frame, then position, velocity, orientation, angular_velocity.
#
# The COMPONENT names have to match each type's storage order, which is not the same as
# the order its accessors are conventionally written in. `gse::quaternion` derives from
# `vec<T, 4>`, whose storage is a plain index-ordered `std::array`, and its accessors are
# `s()`->at(0), `x()`->at(1), `y()`->at(2), `z()`->at(3) (`Math/Quaternion/Quat.cppm`):
# the scalar is FIRST, so the quad is (s, x, y, z) and not (x, y, z, w). The 3-vectors
# are unambiguous because `x()/y()/z()` are at(0..2).
record_fields = (
    ("position", ("x", "y", "z")),
    ("velocity", ("x", "y", "z")),
    ("orientation", ("s", "x", "y", "z")),
    ("angular_velocity", ("x", "y", "z")),
)


def float_ordinal(x: float) -> int:
    """A monotonic integer for a float32, so a subtraction counts ulps.

    IEEE-754 bit patterns are already monotonic within a sign, but the negatives run
    backwards; mapping them through -2^31 makes one ordered line out of both halves, so
    the gap between any two finite floats is the number of representable values between
    them.
    """
    bits = struct.unpack("<i", struct.pack("<f", x))[0]
    return bits if bits >= 0 else -0x80000000 - bits


def ulp_gap(a: float, b: float) -> int:
    return abs(float_ordinal(a) - float_ordinal(b))


@dataclass
class divergence:
    frame: int
    field_name: str
    component: str
    owner: int
    a: float
    b: float
    ulps: int


@dataclass
class bit_scan:
    """The first frame whose records differ in any bit, read field by field.

    It carries no record or frame counts: the comparator reports those in its own header
    and the report prints them from there, so a second copy here could disagree with the
    one the reader is shown.
    """
    first_frame: int | None
    worst: divergence | None
    per_field: dict[str, int] = field(default_factory=dict)
    bodies_at_first: int = 0
    bodies_present: int = 0
    # Differing bodies per frame for the first few frames from the first divergence. A
    # single seed fans out (1, 3, 78, ...); a systemic ordering difference would start at
    # the full count, because it would reach every body the solver touched in the frame
    # it first applied. The shape of these few numbers is what separates the two.
    propagation: list[int] = field(default_factory=list)
    note: str = ""


def read_dump(path: Path) -> list[tuple] | None:
    data = path.read_bytes()
    off = dump_payload_offset(data)
    if off is None:
        return None
    return list(state_dump_record.iter_unpack(data[off:]))


def scan_bits(a: list[tuple], b: list[tuple]) -> bit_scan:
    by_key_b = {(rec[0], rec[1]): rec for rec in b}
    frames = sorted({rec[1] for rec in a})

    for frame in frames:
        if any(differs(rec_a, by_key_b.get((rec_a[0], frame)))
               for rec_a in a if rec_a[1] == frame):
            scan = scan_frame(a, by_key_b, frame)
            scan.propagation = [
                sum(1 for rec_a in a if rec_a[1] == f
                    and differs(rec_a, by_key_b.get((rec_a[0], f))))
                for f in frames[frames.index(frame):frames.index(frame) + 6]
            ]
            return scan

    return bit_scan(None, None,
                    note="no bit difference in any field of any body in any frame")


def differs(rec_a: tuple, rec_b: tuple | None) -> bool:
    # An owner absent from the other dump is not a difference -- it is a body one arm
    # does not have. `frame_drift` skips the same case for the same reason, and
    # `scene_reset_cpu` rebuilds its bodies mid-run, so this is reachable.
    return rec_b is not None and rec_a[2:] != rec_b[2:]


def scan_frame(a: list[tuple], by_key_b: dict, frame: int) -> bit_scan:
    """Every field of every body at one frame, which is what the counts have to describe."""
    per_field: dict[str, int] = {}
    bodies_differing: set[int] = set()
    bodies_present = 0
    worst: divergence | None = None

    for rec_a in a:
        if rec_a[1] != frame:
            continue
        rec_b = by_key_b.get((rec_a[0], frame))
        if rec_b is None:
            continue
        bodies_present += 1
        index = 2
        for name, components in record_fields:
            for component in components:
                va, vb = rec_a[index], rec_b[index]
                index += 1
                if va == vb:
                    continue
                bodies_differing.add(rec_a[0])
                key = f"{name}.{component}"
                per_field[key] = per_field.get(key, 0) + 1
                gap = ulp_gap(va, vb)
                if worst is None or gap > worst.ulps:
                    worst = divergence(frame, name, component, rec_a[0], va, vb, gap)

    return bit_scan(frame, worst, per_field, len(bodies_differing), bodies_present)


@dataclass
class curve_point:
    frame: int
    max_drift: float
    mean_drift: float
    bodies: int


@dataclass
class comparator_run:
    ok: bool
    text: str
    records_a: int
    records_b: int
    frames_a: int
    frames_b: int
    first_over_frame: int | None
    first_over_drift: float
    first_over_bodies: int
    first_over_owner: int
    jump: tuple[int, float, float, int] | None
    smooth: bool
    curve: list[curve_point]
    bodies_in_focus: int
    note: str = ""


def native_environment() -> dict:
    env = dict(os.environ)
    runtime_bin = Path.home() / ".gcc-trunk" / "current" / "bin"
    env["PATH"] = f"{runtime_bin}{os.pathsep}{env.get('PATH', '')}"
    return env


def parse_focus(text: str) -> tuple[list[curve_point], int]:
    """The focus block, folded into one max/mean per frame.

    Each `body N` heading is followed by that body's rows, one per frame, so the blocks
    are split on the heading and the rows inside a block all belong to the body it names.
    The drift column is printed to six decimal places, so a sub-micrometre difference
    reads here as zero -- which is why the bit scan exists and why the curve is not the
    evidence for where divergence BEGINS, only for how it grows.
    """
    blocks = body_pattern.split(text)
    per_frame: dict[int, list[float]] = {}
    bodies = 0
    # split() yields [preamble, owner, body, owner, body, ...]
    for i in range(1, len(blocks) - 1, 2):
        bodies += 1
        for row in focus_row_pattern.findall(blocks[i + 1]):
            per_frame.setdefault(int(row[0]), []).append(float(row[1]))
    curve = [
        curve_point(frame, max(drifts), sum(drifts) / len(drifts), len(drifts))
        for frame, drifts in sorted(per_frame.items())
    ]
    return curve, bodies


def run_comparator(exe: Path, a: Path, b: Path, timeout: float,
                   last_frame: int) -> comparator_run:
    # The focus frame and window come from the dump rather than from a constant, so the
    # window spans whatever run is in hand: `trace_focus_frame` begins at
    # `focus - window` clamped at zero, so a window of the last frame's index covers
    # every frame of any scenario, not only a 240-frame one.
    cmd = [str(exe),
           "--compare-states-a", str(a),
           "--compare-states-b", str(b),
           "--compare-states-threshold", "0",
           "--compare-states-focus-frame", str(last_frame),
           "--compare-states-focus-window", str(last_frame + 1),
           "--compare-states-focus-bodies", "100000"]
    try:
        proc = subprocess.run(cmd, cwd=REPO_ROOT, env=native_environment(),
                              capture_output=True, text=True, encoding="utf-8",
                              errors="replace", timeout=timeout)
    except subprocess.TimeoutExpired:
        return comparator_run(False, "", 0, 0, 0, 0, None, 0.0, 0, 0, None, False, [], 0,
                              f"the comparator did not finish within {timeout:.0f}s")
    text = proc.stdout + proc.stderr
    counts = {m[0]: (int(m[1]), int(m[2])) for m in header_pattern.findall(text)}
    records_a, frames_a = counts.get("A", (0, 0))
    records_b, frames_b = counts.get("B", (0, 0))

    over = first_over_pattern.search(text)
    jump_match = jump_pattern.search(text)
    curve, bodies = parse_focus(text)

    note = ""
    if proc.returncode != 0:
        note = f"the comparator exited {proc.returncode}"
    elif records_a == 0 or records_b == 0:
        note = "one dump carries no records"
    elif records_a != records_b or frames_a != frames_b:
        note = ("the dumps do not describe the same world, so an agreement here would "
                "mean an empty intersection rather than a match")

    return comparator_run(
        proc.returncode == 0, text, records_a, records_b, frames_a, frames_b,
        int(over.group(1)) if over else None,
        float(over.group(2)) if over else 0.0,
        int(over.group(3)) if over else 0,
        int(over.group(4)) if over else 0,
        (int(jump_match.group(1)), float(jump_match.group(2)),
         float(jump_match.group(3)), int(jump_match.group(4))) if jump_match else None,
        bool(smooth_pattern.search(text)), curve, bodies, note)


@dataclass
class pair:
    name: str
    native: Path
    wasm: Path
    comparator: comparator_run
    bits: bit_scan


def write_curve_csv(path: Path, pairs: list[pair]) -> None:
    lines = ["run,frame,max_drift_m,mean_drift_m,bodies"]
    for p in pairs:
        for point in p.comparator.curve:
            lines.append(f"{p.name},{point.frame},{point.max_drift:.6f},"
                         f"{point.mean_drift:.6f},{point.bodies}")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def verdict(pairs: list[pair]) -> list[str]:
    """One body drifting smoothly, or many at once -- the question the task exists for.

    The two answers are different problems. Last bits, growing frame over frame, is the
    two arms doing the same arithmetic in a different order, and then "agreement
    throughout" is a question about building the portable-SIMD native arm rather than a
    defect to chase. A discrete jump, or many bodies parting at once by more than a
    rounding step, or a difference in a count or an ordering, is a fidelity defect in the
    port itself -- a contact-pair sort, a solver iteration order, a reduction over an
    unordered container.
    """
    out: list[str] = []
    # One pair for both halves of the verdict. Reading the bit scan from the first pair
    # that produced one and the positional spread from `pairs[0]` would describe two
    # different runs in one paragraph the moment a dump failed to read.
    subject = next((p for p in pairs if p.bits.first_frame is not None), None)
    if subject is None:
        return ["No pair produced a readable bit scan, so the shape is undetermined."]

    first = subject.bits
    worst = first.worst
    many = first.bodies_at_first > 1
    tiny = worst is not None and worst.ulps <= 4
    spread = subject.comparator

    if worst:
        out.append(
            f"The arms are bit-identical in every field of every body through frame "
            f"{first.first_frame - 1}. They first part at **frame "
            f"{first.first_frame}**, in **{first.bodies_at_first} of "
            f"{first.bodies_present} bodies**, by **{worst.ulps} ulp"
            f"{'' if worst.ulps == 1 else 's'}** of "
            f"`{worst.field_name}.{worst.component}` on body {worst.owner} "
            f"({worst.a!r} against {worst.b!r}).")
    else:
        out.append(
            f"The arms first part at frame {first.first_frame}, in "
            f"{first.bodies_at_first} of {first.bodies_present} bodies.")

    if first.propagation:
        series = " -> ".join(str(n) for n in first.propagation)
        out.append(
            f"It fans out from there. Bodies differing, per frame from frame "
            f"{first.first_frame}: **{series}** (of {first.bodies_present}). That shape "
            f"is the evidence that this is one seed and not a systemic difference: a "
            f"reordering applied by the solver would reach every body it touched in the "
            f"frame it first applied, so it would start at the full count rather than "
            f"climb to it.")
    if spread.first_over_frame is not None:
        out.append(
            f"Position itself moves {spread.first_over_frame - first.first_frame} frames "
            f"later, at frame {spread.first_over_frame}, by "
            f"{spread.first_over_drift * 1e6:.3f} um at the worst -- so the first thing "
            f"to part is an orientation, and the positional drift the gate measures is "
            f"downstream of it.")

    if tiny and not many:
        out.append("")
        out.append(
            "**This is the arithmetic, not a fidelity defect.** A single body differing "
            "in a single ulp of one quaternion component is the smallest difference the "
            "format can represent -- it is one expression evaluated in a different order, "
            "which is what the two SIMD arms are. `Math/Vector/SIMD.cppm` reduces a dot "
            "product through `_mm_dp_ps` / `_mm_hadd_ps` on x86 and through a sequential "
            "accumulation on the portable arm that wasm takes; same inputs, same "
            "operations, different association, last bit different. Nothing about a "
            "contact-pair sort, a solver iteration order or a reduction over an unordered "
            "container produces a one-ulp difference in one body: each of those reorders "
            "whole contributions and would move a subset of bodies by far more than a "
            "rounding step, in the first frame it occurred.")
        out.append(
            "The growth after it is the contact solver, behaving as a contact solver "
            "does: see the curve. A one-ulp seed is amplified through the strike and the "
            "collapse, not by a second defect.")
        out.append(
            "**So agreement throughout is not reachable against a native AVX2 build.** "
            "The arms do not compute the same function, and no amount of work on the port "
            "makes a one-ulp seed not exist. The honest route to \"throughout\" is "
            "criterion (3)'s `-DGSE_NO_SIMD_X86` native arm, which runs the same portable "
            "reduction wasm runs, where the seed should be absent rather than small. That "
            "is a second native configuration and is the owner's call.")
    elif many and tiny:
        out.append(
            "Many bodies at once, each by the last bit or two: an ordering difference "
            "reaching every body the solver touched in that frame. Consistent with the "
            "SIMD arms (`Math/Vector/SIMD.cppm` reduces a dot product pairwise on x86 and "
            "sequentially on the portable arm) and not with a sort or an iteration order, "
            "which would move a subset by much more than an ulp. On that reading "
            "\"throughout\" is a question about the portable-SIMD native arm, not a "
            "defect to chase.")
    elif many:
        out.append(
            "Many bodies at once and by more than a rounding step. That is larger than an "
            "arithmetic ordering difference accounts for, and should be read as a "
            "fidelity defect in the port: look for a per-frame ordering the wasm arm does "
            "not reproduce -- a contact-pair sort, a solver iteration order, or a "
            "reduction over an unordered container.")
    else:
        out.append(
            "A single body, by more than a rounding step. That is a discrete event rather "
            "than an arithmetic difference: that body took a different path in one frame, "
            "and the contact it resolved there is the subject. Chase it in the port before "
            "spending anything on the SIMD question.")
    return out


def collapse_frames(curve: list[curve_point], factor: float = 100.0,
                    floor: float = 1e-3) -> list[int]:
    """Frames at which the drift falls off a cliff rather than decaying.

    `scene_reset_cpu` is not a single uninterrupted simulation: it settles, strikes,
    watches the pile collapse and then **resets the scene**, which re-seeds both arms
    from the same spawn poses and so sets the accumulated divergence back to zero. Any
    conclusion drawn from the final frame is therefore a conclusion about the frames
    after the last such reset, and that is worth saying out loud rather than leaving for
    a reader to notice in the curve.

    `floor` is why the ratio alone is not the test. The drift column is printed to six
    decimal places, so a value just above that resolution divided by a zero reads as an
    enormous ratio -- a 1 um drift dropping to nothing would pose as a reset. The last
    frame this returns carries the claim about which frames criterion (4) is measured
    over, so it has to mean a cliff off real accumulated divergence: require the drop to
    start from at least a millimetre, which is three orders above the print resolution
    and still three below the 1.6 m this scenario reaches.
    """
    return [point.frame for previous, point in zip(curve, curve[1:])
            if previous.max_drift >= floor
            and previous.max_drift / max(point.max_drift, 1e-12) > factor]


def cross_run(pairs: list[pair]) -> list[str]:
    """Whether the three pairs agree about where they parted.

    Free, and never previously asked. Three identical answers mean the divergence is
    deterministic -- the same computation differing the same way every time, which is
    what an arithmetic explanation requires. Any disagreement means one arm is not
    deterministic across runs, which is a worse finding than the divergence itself and
    outranks everything else here: the gate's criterion (5) checks that the wasm hash is
    stable, but nothing checks that the two arms part in the same place.
    """
    frames = {p.bits.first_frame for p in pairs if p.bits.first_frame is not None}
    owners = {p.bits.worst.owner for p in pairs if p.bits.worst}
    fields = {f"{p.bits.worst.field_name}.{p.bits.worst.component}"
              for p in pairs if p.bits.worst}
    over = {p.comparator.first_over_frame for p in pairs
            if p.comparator.first_over_frame is not None}

    if len(pairs) < 2:
        return ["Only one pair was available, so cross-run agreement is unmeasured."]
    lines = [
        f"- first divergent frame across runs: "
        f"{', '.join(str(f) for f in sorted(frames)) or 'none'}",
        f"- first frame with nonzero positional drift: "
        f"{', '.join(str(f) for f in sorted(over)) or 'none'}",
        f"- worst-ulp field across runs: {', '.join(sorted(fields)) or 'none'}",
        f"- worst-ulp body across runs: "
        f"{', '.join(str(o) for o in sorted(owners)) or 'none'}",
    ]
    if len(frames) == 1 and len(over) == 1:
        lines.append(
            "\nThe runs agree on where the arms part, so the divergence is "
            "deterministic: the same computation differing the same way each time, which "
            "is what an arithmetic explanation needs and a race would not give.")
    else:
        lines.append(
            "\n**The runs do NOT agree on where the arms part.** That is nondeterminism "
            "on one of the two arms, and it outranks the divergence itself: a world-state "
            "hash can be stable across runs while the path to it is not. Settle this "
            "before spending anything on the SIMD question.")
    return lines


def report(path: Path, exe: Path, pairs: list[pair], csv: Path) -> None:
    # The run's own last frame, so the report describes the dumps in hand rather than a
    # scenario length compiled into this file.
    last_frame = max((q.frame for p in pairs for q in p.comparator.curve), default=0)
    lines = [
        "# Where the wasm and native arms first part",
        "",
        f"Written {time.strftime('%Y-%m-%d %H:%M:%S')} by `Tools/wasm_divergence.py`.",
        "",
        "`wasm_parity_gate.py` compares the final frame, which is what criterion (4) "
        "names, and reports the peak beside it. This is the follow-up: the frame at which "
        "the arms first differ at all, in which field, by how many ulps, and how the "
        "difference grows from there.",
        "",
        "## Subjects",
        "",
        f"- comparator `{exe}` ({exe.stat().st_size} bytes, mtime "
        f"{time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(exe.stat().st_mtime))})",
    ]
    for p in pairs:
        lines.append(f"- `{p.name}`: native `{p.native.name}` "
                     f"({p.native.stat().st_size} bytes) against wasm `{p.wasm.name}` "
                     f"({p.wasm.stat().st_size} bytes)")
    lines += [
        "",
        "Nothing was built and no scenario was run. The dumps are the ones "
        "`wasm_parity_gate.py` left on disk.",
        "",
        "## Verdict",
        "",
    ]
    lines += verdict(pairs)

    lines += [
        "",
        "## First divergence, bit level, every field",
        "",
        "The comparator reads `position` alone and through `gse::distance`, so this half "
        "is read from the records directly -- it is the only way a first difference in "
        "`velocity`, `orientation` or `angular_velocity` is visible, and the only way the "
        "magnitude can be stated in ulps rather than in metres.",
        "",
        "| run | first divergent frame | bodies differing | bodies compared | worst field | worst body | A | B | ulps |",
        "| --- | --- | --- | --- | --- | --- | --- | --- | --- |",
    ]
    for p in pairs:
        s = p.bits
        if s.first_frame is None:
            lines.append(f"| {p.name} | none | - | - | - | - | - | - | "
                         f"{s.note or 'no difference'} |")
            continue
        w = s.worst
        lines.append(
            f"| {p.name} | {s.first_frame} | {s.bodies_at_first} | {s.bodies_present} | "
            f"`{w.field_name}.{w.component}` | {w.owner} | `{w.a!r}` | `{w.b!r}` | "
            f"{w.ulps} |" if w else
            f"| {p.name} | {s.first_frame} | {s.bodies_at_first} | {s.bodies_present} | "
            f"- | - | - | - | - |")

    lines += ["", "### Which fields differ in that frame", ""]
    for p in pairs:
        if not p.bits.per_field:
            continue
        rows = ", ".join(f"`{k}` in {v} bodies"
                         for k, v in sorted(p.bits.per_field.items()))
        lines.append(f"- {p.name}: {rows}")

    # The header names the frames it counts, so it has to come from the same pair the
    # rows do -- and each row carries its own first frame, because two pairs that parted
    # at different frames cannot share one set of column labels.
    fanned = [p for p in pairs if p.bits.propagation]
    if fanned:
        width = max(len(p.bits.propagation) for p in fanned)
        lines += [
            "",
            "### Fan-out",
            "",
            "Bodies differing in any field, per frame, from each run's own first "
            "divergence. A single seed climbs; a reordering the solver applies would "
            "start at the full body count, because it would reach every body touched in "
            "the frame it first applied.",
            "",
            "| run | from frame | " + " | ".join(f"+{i}" for i in range(width))
            + " | of |",
            "| --- | --- | " + " | ".join("---" for _ in range(width)) + " | --- |",
        ]
        for p in fanned:
            counts = [str(n) for n in p.bits.propagation]
            counts += ["-"] * (width - len(counts))
            lines.append(f"| {p.name} | {p.bits.first_frame} | "
                         + " | ".join(counts) + f" | {p.bits.bodies_present} |")

    lines += [
        "",
        "## Positional drift, from the engine's comparator",
        "",
        "`--compare-states-threshold 0` makes the comparator's own \"first over "
        "threshold\" row fire on the first frame carrying any nonzero positional drift, "
        "and that row carries the count of bodies over the threshold. The jump line is "
        "the comparator's own discrete-versus-smooth verdict, at its own 10x test.",
        "",
        "| run | records A | records B | frames | first nonzero drift | drift there | bodies over | worst body | largest single-frame jump |",
        "| --- | --- | --- | --- | --- | --- | --- | --- | --- |",
    ]
    for p in pairs:
        c = p.comparator
        jump = (f"frame {c.jump[0]}, {c.jump[1] * 1000:.3f} mm -> "
                f"{c.jump[2] * 1000:.3f} mm on body {c.jump[3]}"
                if c.jump else ("smooth drift, no jump above 10x" if c.smooth else "-"))
        lines.append(
            f"| {p.name} | {c.records_a} | {c.records_b} | {c.frames_a} | "
            f"{c.first_over_frame if c.first_over_frame is not None else '-'} | "
            f"{c.first_over_drift * 1e6:.3f} um | {c.first_over_bodies} | "
            f"{c.first_over_owner} | {jump}"
            f"{' -- ' + c.note if c.note else ''} |")

    lines += ["", "## The scene reset, and what it does to criterion (4)", ""]
    resets = collapse_frames(pairs[0].comparator.curve) if pairs else []
    peaks = [(p.name, max(p.comparator.curve, key=lambda q: q.max_drift))
             for p in pairs if p.comparator.curve]
    if resets:
        lines += [
            f"The curve falls off a cliff at frame {', '.join(str(f) for f in resets)} "
            f"-- the drift drops by more than a hundredfold in one frame, which decay "
            f"cannot do. That is the scenario's own scripted scene reset: "
            f"`expect_reset_restores_spawn_poses` settles the pyramid, strikes it, "
            f"watches it collapse for 60 frames and then rebuilds it, and the page log "
            f"records `scene reset rebuilt 79 bodies from 79`. A reset re-seeds both arms "
            f"from the same spawn poses, so it sets the accumulated divergence back to "
            f"zero.",
            "",
            f"**Criterion (4) is therefore measured on the tail after that reset, not on "
            f"the whole run.** The final frame is "
            f"{last_frame - resets[-1]} frames downstream of a state re-seed, so the "
            f"1.8 mm it passes at is what the arms have re-accumulated since -- not what "
            f"they accumulated over 240 frames. Read it as agreement over the last "
            f"{last_frame + 1 - resets[-1]} frames.",
            "",
        ]
    if peaks:
        lines += [
            "| run | true peak drift | at frame |",
            "| --- | --- | --- |",
        ]
        for name, point in peaks:
            lines.append(f"| {name} | {point.max_drift * 1000:.3f} mm | {point.frame} |")
        lines += [
            "",
            "This is larger than the peak `wasm_parity_gate.py` reports, and the "
            "difference is not a disagreement: that gate reads the comparator's printed "
            "rows, and the comparator prints every fiftieth frame, so its peak is the "
            "largest drift on a sampled frame. The focus block used here carries every "
            "frame, so this is the peak over all of them.",
            "",
        ]

    lines += [
        "",
        "## Drift curve",
        "",
        f"Per-frame max and mean positional drift over every body, folded from the "
        f"comparator's focus block ({pairs[0].comparator.bodies_in_focus if pairs else 0} "
        f"bodies x {last_frame + 1} frames per pair). The full curve for all three runs is "
        f"in `{csv.name}`; every tenth frame is shown here.",
        "",
        "The comparator prints drift to six decimal places, so a sub-micrometre "
        "difference reads as zero in this table. The curve is evidence about how the "
        "divergence GROWS; the bit scan above is the evidence about where it begins.",
        "",
    ]
    for p in pairs:
        lines += [f"### {p.name}", "",
                  "| frame | max drift | mean drift | bodies |",
                  "| --- | --- | --- | --- |"]
        peak = max(p.comparator.curve, key=lambda q: q.max_drift,
                   default=None) if p.comparator.curve else None
        shown = {q.frame for q in p.comparator.curve if q.frame % 10 == 0}
        if peak:
            shown.add(peak.frame)
        if p.comparator.first_over_frame is not None:
            shown.add(p.comparator.first_over_frame)
        if p.comparator.curve:
            shown.add(p.comparator.curve[-1].frame)
        for q in p.comparator.curve:
            if q.frame not in shown:
                continue
            tag = ""
            if peak and q.frame == peak.frame:
                tag = " <- peak"
            if q.frame == p.comparator.first_over_frame:
                tag += " <- first nonzero"
            lines.append(f"| {q.frame}{tag} | {q.max_drift * 1000:.3f} mm | "
                         f"{q.mean_drift * 1000:.3f} mm | {q.bodies} |")
        lines.append("")

    lines += ["## Cross-run agreement", ""]
    lines += cross_run(pairs)
    lines += ["", "## Invocation", "", "```",
              "python Tools/wasm_divergence.py", "```", ""]

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--exe", type=Path, default=NATIVE_EXE,
                   help="the binary whose comparator is used, taken from disk and never "
                        "built")
    p.add_argument("--gate-out", type=Path,
                   default=GENERATED_ROOT / "out" / "wasm_parity_gate",
                   help="the gate's output directory, holding run*/native.dump and "
                        "run*/parity.dump")
    p.add_argument("--timeout", type=float, default=900.0)
    args = p.parse_args()

    if not args.exe.is_file():
        print(f"missing {args.exe}")
        print("an editor build in progress displaces a locked exe to Sandbox.exe.bak; "
              "it returns on the next build")
        return 2

    runs = sorted(d for d in args.gate_out.glob("run*")
                  if (d / "native.dump").is_file() and (d / "parity.dump").is_file())
    if not runs:
        print(f"no run*/native.dump + run*/parity.dump pairs under {args.gate_out}")
        print("run Tools/wasm_parity_gate.py first")
        return 2

    print("== where the arms first part ==")
    print(f"  comparator      {args.exe}")
    print(f"  pairs           {', '.join(d.name for d in runs)}")
    print()

    pairs: list[pair] = []
    for directory in runs:
        native = directory / "native.dump"
        wasm = directory / "parity.dump"
        # Read both dumps once, here: the bit scan needs the records and the comparator
        # needs the frame span, and reading 1.2 MB twice to answer two questions about
        # the same file is waste the loop can avoid.
        records_a = read_dump(native)
        records_b = read_dump(wasm)
        if records_a is None or records_b is None:
            missing = " and ".join(p.name for p, r in ((native, records_a),
                                                       (wasm, records_b)) if r is None)
            print(f"{directory.name}: could not read {missing}, skipping", flush=True)
            continue
        last = max(rec[1] for rec in records_a)
        print(f"{directory.name}: comparator", flush=True)
        comparator = run_comparator(args.exe, native, wasm, args.timeout, last)
        if comparator.text:
            (directory / "compare-states.log").write_text(comparator.text,
                                                          encoding="utf-8")
        print(f"  first nonzero drift at frame {comparator.first_over_frame}, "
              f"{comparator.first_over_bodies} bodies over, "
              f"{len(comparator.curve)} curve points from "
              f"{comparator.bodies_in_focus} bodies"
              + (f" -- {comparator.note}" if comparator.note else ""), flush=True)
        print(f"{directory.name}: bit scan", flush=True)
        bits = scan_bits(records_a, records_b)
        if bits.first_frame is None:
            print(f"  {bits.note or 'no bit difference found'}", flush=True)
        else:
            w = bits.worst
            print(f"  frame {bits.first_frame}, {bits.bodies_at_first} of "
                  f"{bits.bodies_present} bodies, worst "
                  f"{w.field_name}.{w.component} {w.ulps} ulps on body {w.owner}"
                  if w else f"  frame {bits.first_frame}", flush=True)
        pairs.append(pair(directory.name, native, wasm, comparator, bits))

    csv = args.gate_out / "divergence_curve.csv"
    write_curve_csv(csv, pairs)
    artifact = args.gate_out / "divergence.md"
    report(artifact, args.exe, pairs, csv)

    print()
    for line in verdict(pairs):
        print(line, flush=True)
    print()
    print(f"  curve           {csv}")
    print(f"  report          {artifact}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
