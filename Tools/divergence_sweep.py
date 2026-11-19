import argparse
import itertools
import statistics
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

HEADER_BYTES = 12
RECORD_BYTES = 64
PAYLOAD_OFFSET = 12
PAYLOAD_BYTES = 52


def read_dump(path):
    raw = path.read_bytes()
    if len(raw) < HEADER_BYTES or (len(raw) - HEADER_BYTES) % RECORD_BYTES:
        raise ValueError(f"{path} is not a state dump ({len(raw)} bytes)")
    frames = {}
    count = (len(raw) - HEADER_BYTES) // RECORD_BYTES
    for i in range(count):
        base = HEADER_BYTES + i * RECORD_BYTES
        owner, frame = struct.unpack_from("<QI", raw, base)
        payload = raw[base + PAYLOAD_OFFSET:base + RECORD_BYTES]
        frames.setdefault(frame, {})[owner] = payload
    return frames


def first_divergence(a, b):
    for frame in sorted(a):
        left = a[frame]
        right = b.get(frame)
        if right is None:
            continue
        for owner, payload in left.items():
            other = right.get(owner)
            if other is not None and other != payload:
                return frame
    return None


def run_once(exe, scenario, frames, settings, dump_path, log_path):
    cmd = [
        str(exe),
        "--engine-bench-scenario", scenario,
        "--no-engine-persist-settings",
        "--engine-bench-warmup-frames", "0",
        "--engine-bench-frames", str(frames),
        "--engine-bench-state-dump-out", str(dump_path),
    ]
    for setting in settings:
        cmd += ["--engine-setting", setting]
    with log_path.open("wb") as log:
        result = subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT)
    if result.returncode != 0:
        raise RuntimeError(f"run failed with exit {result.returncode}; see {log_path}")


def p50_from_log(log_path):
    try:
        text = log_path.read_text(errors="ignore")
    except OSError:
        return None
    marker = "p50 "
    best = None
    for line in text.splitlines():
        at = line.find(marker)
        while at != -1:
            tail = line[at + len(marker):].split(" ms")
            if len(tail) > 1:
                try:
                    best = float(tail[0])
                except ValueError:
                    pass
            at = line.find(marker, at + 1)
    return best


def sweep(args):
    workdir = Path(args.workdir) if args.workdir else Path(tempfile.mkdtemp(prefix="divsweep-"))
    workdir.mkdir(parents=True, exist_ok=True)
    dumps = []
    p50s = []
    for index in range(args.runs):
        dump_path = workdir / f"{args.label}_{index}.dump"
        log_path = workdir / f"{args.label}_{index}.log"
        if not (args.reuse and dump_path.exists()):
            run_once(args.exe, args.scenario, args.frames, args.setting, dump_path, log_path)
        dumps.append(read_dump(dump_path))
        measured = p50_from_log(log_path)
        if measured is not None:
            p50s.append(measured)

    onsets = []
    identical = 0
    for left, right in itertools.combinations(range(args.runs), 2):
        frame = first_divergence(dumps[left], dumps[right])
        if frame is None:
            identical += 1
            onsets.append(args.frames)
        else:
            onsets.append(frame)

    onsets.sort()
    pairings = len(onsets)
    print(f"label            {args.label}")
    print(f"scenario         {args.scenario}  frames {args.frames}  runs {args.runs}")
    if args.setting:
        print(f"settings         {' '.join(args.setting)}")
    print(f"pairings         {pairings}")
    print(f"identical        {identical}/{pairings}")
    print(f"onset median     {statistics.median(onsets):.1f}   (censored at {args.frames} when identical)")
    print(f"onset min/max    {onsets[0]} / {onsets[-1]}")
    print(f"onset quartiles  {onsets[pairings // 4]} / {onsets[pairings // 2]} / {onsets[(3 * pairings) // 4]}")
    if p50s:
        print(f"p50 median       {statistics.median(p50s):.3f} ms")
    print(f"onsets           {onsets}")
    print(f"workdir          {workdir}")


def main():
    parser = argparse.ArgumentParser(
        description="Score solver determinism by first-divergence frame across all run pairings."
    )
    parser.add_argument("--exe", required=True, type=Path)
    parser.add_argument("--scenario", default="parity_stress_gpu")
    parser.add_argument("--runs", type=int, default=6)
    parser.add_argument("--frames", type=int, default=120)
    parser.add_argument("--setting", action="append", default=[])
    parser.add_argument("--label", default="sweep")
    parser.add_argument("--workdir")
    parser.add_argument("--reuse", action="store_true")
    args = parser.parse_args()
    try:
        sweep(args)
    except Exception as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
