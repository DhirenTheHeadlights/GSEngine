"""Diff the float libm results of the wasm and mingw arms, bit for bit.

Builds Tools/libm_probe/probe.cpp twice: for wasm32 through `build_gcc_wasm.py --probe
libm_parity` (the existing probe machinery, run under node), and for the host with the
gcc-trunk g++ the native Sandbox is built with, at the portable-scalar preset's codegen
flags (-O3, no -mavx2). Both write `libm <fn> <a> <b> <result>` lines; results are paired
by (fn, a, b) and every mismatch is counted in ulps. The native compile is one standalone
translation unit into out/libm_probe/ and touches no CMake build directory or module cache.

Usage, from the repo root:

  python Tools/libm_probe/run.py [--coremath]

--coremath builds both arms against the vendored CORE-MATH routines that gse::libm
forwards to (Engine/External/coremath) instead of each platform's libm, and writes
report_coremath.md; zero differences is the expected result.

Writes out/libm_probe/report.md and exits 1 if any result differs.
"""

import os
import struct
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from repo_paths import BUILD_ROOT, REPO_ROOT

PROBE = REPO_ROOT / "Tools" / "libm_probe" / "probe.cpp"
OUT = BUILD_ROOT.parent / "libm_probe"
GXX = Path.home() / ".gcc-trunk" / "current" / "bin" / "g++.exe"
NATIVE_FLAGS = ("-std=c++26", "-fno-exceptions", "-fno-rtti", "-O3", "-static")
SEED_PREFIX = "seed_"
COREMATH = "--coremath" in sys.argv[1:]


def results(text: str) -> dict[tuple[str, str, str], str]:
    table = {}
    for line in text.splitlines():
        parts = line.split()
        if len(parts) == 5 and parts[0] == "libm":
            table[(parts[1], parts[2], parts[3])] = parts[4]
    return table


def ordinal(hex_bits: str) -> int:
    signed = struct.unpack("<i", struct.pack("<I", int(hex_bits, 16)))[0]
    return signed if signed >= 0 else -0x80000000 - signed


def as_float(hex_bits: str) -> float:
    return struct.unpack("<f", struct.pack("<I", int(hex_bits, 16)))[0]


def run_wasm() -> str:
    proc = subprocess.run([sys.executable, str(REPO_ROOT / "Tools" / "build_gcc_wasm.py"),
                           "--probe", "libm_parity_coremath" if COREMATH else "libm_parity"],
                          cwd=REPO_ROOT, capture_output=True, text=True)
    (OUT / "wasm.log").write_text(proc.stdout + proc.stderr, encoding="utf-8")
    if proc.returncode != 0:
        raise SystemExit(f"wasm probe failed ({proc.returncode}); see {OUT / 'wasm.log'}")
    return proc.stdout


def run_native() -> str:
    exe = OUT / "probe_native.exe"
    env = dict(os.environ, PATH=f"{GXX.parent}{os.pathsep}{os.environ.get('PATH', '')}")
    coremath_flags = ["-DGSE_LIBM_COREMATH", f"-I{REPO_ROOT / 'Engine' / 'External'}"] if COREMATH else []
    compile_line = [str(GXX), *NATIVE_FLAGS, *coremath_flags, "-o", str(exe), str(PROBE)]
    built = subprocess.run(compile_line, capture_output=True, text=True, env=env)
    if built.returncode != 0:
        raise SystemExit(f"native compile failed:\n{built.stdout}{built.stderr}")
    proc = subprocess.run([str(exe)], capture_output=True, text=True, env=env)
    (OUT / "native.log").write_text(proc.stdout + proc.stderr, encoding="utf-8")
    if proc.returncode != 0:
        raise SystemExit(f"native probe exited {proc.returncode}")
    return proc.stdout


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    wasm = results(run_wasm())
    native = results(run_native())
    shared = sorted(set(wasm) & set(native))

    per_fn: dict[str, list] = defaultdict(list)
    counts: dict[str, int] = defaultdict(int)
    for key in shared:
        counts[key[0]] += 1
        if wasm[key] != native[key]:
            per_fn[key[0]].append((key, native[key], wasm[key],
                                   abs(ordinal(native[key]) - ordinal(wasm[key]))))

    title = ("vendored CORE-MATH on both arms" if COREMATH
             else "mingw-w64 (native) against wasi-libc (wasm)")
    lines = [f"# libm parity: {title}", "",
             f"Native: `{GXX} {' '.join(NATIVE_FLAGS)}`  ",
             f"Wasm: `build_gcc_wasm.py --probe {'libm_parity_coremath' if COREMATH else 'libm_parity'}` (CXXFLAGS -O3)", "",
             f"Results paired: {len(shared)} (wasm {len(wasm)}, native {len(native)})", "",
             "| fn | compared | differ | max ulps |", "|---|---|---|---|"]
    for fn in sorted(counts):
        diffs = per_fn.get(fn, [])
        worst = max((d[3] for d in diffs), default=0)
        lines.append(f"| {fn} | {counts[fn]} | {len(diffs)} | {worst} |")
    lines.append("")
    for fn in sorted(per_fn):
        lines.append(f"## {fn}: first mismatches")
        lines.append("")
        for (name, a, b, ), n, w, ulps in per_fn[fn][:10]:
            arg = f"{a} ({as_float(a):.9g})" + ("" if b == "00000000" else
                                               f", {b} ({as_float(b):.9g})")
            lines.append(f"- `{fn}({arg})`: native `{n}` ({as_float(n):.9g}), "
                         f"wasm `{w}` ({as_float(w):.9g}), {ulps} ulp")
        lines.append("")
    seed = [k for k in shared if k[0].startswith(SEED_PREFIX)]
    seed_diffs = [k for k in seed if wasm[k] != native[k]]
    lines.append(f"Seed body (frame 31 state) inputs: {len(seed)} compared, "
                 f"{len(seed_diffs)} differ.")
    total = sum(len(v) for v in per_fn.values())
    verdict = (f"libm DIFFERS: {total} results across {len(per_fn)} functions"
               if total else "libm REFUTED: every result identical in every bit")
    lines += ["", f"**Verdict:** {verdict}", ""]
    (OUT / ("report_coremath.md" if COREMATH else "report.md")).write_text("\n".join(lines), encoding="utf-8")
    print("\n".join(lines))
    return 1 if total else 0


if __name__ == "__main__":
    sys.exit(main())
