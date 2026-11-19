import argparse
import math
import struct
import sys
from pathlib import Path

RECORD = struct.Struct("<QI13f")
MAGIC = 0x44534753


def fnv1a64(s: str) -> int:
    h = 0xCBF29CE484222325
    for b in s.encode():
        h ^= b
        h = (h * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h


DOMINOES = {fnv1a64(f"Domino {i}"): f"Domino {i}" for i in range(1, 13)}

JOINTED = {}
for label in ("Stiff", "Medium", "Soft"):
    JOINTED[fnv1a64(f"Spring {label} Bob")] = f"Spring {label} Bob"
for i in range(5):
    JOINTED[fnv1a64(f"Spring Chain Link {i}")] = f"Spring Chain Link {i}"
    JOINTED[fnv1a64(f"Chain Link {i}")] = f"Chain Link {i}"
JOINTED[fnv1a64("Fixed Hanging Box")] = "Fixed Hanging Box"
JOINTED[fnv1a64("Distance Bob")] = "Distance Bob"
JOINTED[fnv1a64("Hinge Door")] = "Hinge Door"
JOINTED[fnv1a64("Slider Platform")] = "Slider Platform"
JOINTED[fnv1a64("Joint Test Sphere")] = "Joint Test Sphere"


def read_dump(path: Path):
    data = path.read_bytes()
    off = None
    for candidate in (12, 8, 16, 0):
        if len(data) > candidate and (len(data) - candidate) % RECORD.size == 0:
            off = candidate
            break
    if off is None:
        print(f"WARN {path}: no header offset makes payload a multiple of {RECORD.size}")
        off = 0
    payload = len(data) - off
    frames = {}
    for rec in RECORD.iter_unpack(data[off : off + payload - payload % RECORD.size]):
        owner, frame = rec[0], rec[1]
        pos = rec[2:5]
        vel = rec[5:8]
        frames.setdefault(owner, []).append((frame, pos, vel))
    for v in frames.values():
        v.sort(key=lambda t: t[0])
    return frames


def speed(v):
    return math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])


def dist(a, b):
    return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)))


def cascade(frames, label):
    rows = []
    for owner, name in sorted(DOMINOES.items(), key=lambda kv: int(kv[1].split()[1])):
        if owner not in frames:
            rows.append((name, None, None, None))
            continue
        series = frames[owner]
        final_y = series[-1][1][1]
        arrival = None
        for frame, pos, _ in series:
            if pos[1] < 0.7:
                arrival = frame
                break
        toppled = final_y < 0.7
        rows.append((name, final_y, toppled, arrival))
    toppled_count = sum(1 for r in rows if r[2])
    print(f"\n[{label}] cascade: {toppled_count}/12 toppled")
    for name, fy, top, arr in rows:
        if fy is None:
            print(f"  {name:<10} MISSING FROM DUMP")
        else:
            print(f"  {name:<10} final_y {fy:7.3f}  {'toppled' if top else 'STANDING'}  arrival {arr if arr is not None else '-'}")
    return rows


def jointed(frames, label):
    print(f"\n[{label}] jointed battery:")
    worst_disp = 0.0
    bad = 0
    for owner, name in sorted(JOINTED.items(), key=lambda kv: kv[1]):
        if owner not in frames:
            print(f"  {name:<22} not in dump")
            continue
        series = frames[owner]
        spawn = series[0][1]
        final_pos = series[-1][1]
        final_speed = speed(series[-1][2])
        max_disp = max(dist(p, spawn) for _, p, _ in series)
        nan = any(math.isnan(c) or math.isinf(c) for _, p, v in series for c in (*p, *v))
        worst_disp = max(worst_disp, max_disp)
        flag = ""
        if nan:
            flag = "  ** NaN/Inf **"
            bad += 1
        elif max_disp > 20.0:
            flag = "  ** TETHER BLOWN **"
            bad += 1
        print(f"  {name:<22} max_disp {max_disp:7.3f} m  final_speed {final_speed:6.3f}  final_y {final_pos[1]:7.3f}{flag}")
    print(f"  worst jointed displacement {worst_disp:.3f} m, {bad} failures")
    return bad


def global_invariants(frames, label):
    max_speed = 0.0
    max_r = 0.0
    nan_owners = 0
    for owner, series in frames.items():
        for _, p, v in series:
            if any(math.isnan(c) or math.isinf(c) for c in (*p, *v)):
                nan_owners += 1
                break
        s = speed(series[-1][2])
        r = math.sqrt(sum(c * c for c in series[-1][1]))
        max_speed = max(max_speed, s)
        max_r = max(max_r, r)
    print(f"\n[{label}] global: {len(frames)} bodies, final max speed {max_speed:.3f}, max |pos| {max_r:.2f} m, NaN bodies {nan_owners}")


def compare(cpu_rows, gpu_rows, gpu_label):
    print(f"\n[cpu vs {gpu_label}] per-domino final height delta:")
    worst = 0.0
    for (name, cy, _, ca), (_, gy, _, ga) in zip(cpu_rows, gpu_rows):
        if cy is None or gy is None:
            continue
        d = abs(cy - gy)
        worst = max(worst, d)
        da = "-" if ca is None or ga is None else str(ga - ca)
        print(f"  {name:<10} cpu {cy:7.3f} gpu {gy:7.3f} delta {d * 100:6.2f} cm  arrival_shift {da}")
    print(f"  worst height delta {worst * 100:.2f} cm")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cpu", type=Path)
    ap.add_argument("--gpu", type=Path, nargs="*", default=[])
    args = ap.parse_args()

    cpu_rows = None
    if args.cpu:
        frames = read_dump(args.cpu)
        cpu_rows = cascade(frames, "cpu")
        jointed(frames, "cpu")
        global_invariants(frames, "cpu")

    for i, dump in enumerate(args.gpu):
        label = f"gpu{i}" if len(args.gpu) > 1 else "gpu"
        frames = read_dump(dump)
        rows = cascade(frames, label)
        fails = jointed(frames, label)
        global_invariants(frames, label)
        if cpu_rows:
            compare(cpu_rows, rows, label)
        if fails:
            sys.exit(1)


if __name__ == "__main__":
    main()
