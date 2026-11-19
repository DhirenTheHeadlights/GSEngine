import argparse
import itertools
import json
import math
import struct
import sys
import zlib
from dataclasses import asdict, dataclass
from pathlib import Path

pixel_threshold = 8
tolerance_max_mean_error = 0.01
tolerance_max_over_fraction = 0.0


@dataclass
class image:
    width: int
    height: int
    rgb: bytes


@dataclass
class comparison:
    a: str
    b: str
    width: int
    height: int
    max_error: int
    mean_error: float
    over_fraction: float
    psnr: float
    within_tolerance: bool
    note: str = ""


def paeth(a: int, b: int, c: int) -> int:
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    return b if pb <= pc else c


def read_png(path: Path) -> image:
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        sys.exit(f"{path}: not a PNG")
    pos = 8
    idat = bytearray()
    width = height = color_type = 0
    while pos < len(data):
        length, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b"IHDR":
            width, height, depth, color_type, _, _, interlace = struct.unpack(">IIBBBBB", body)
            if depth != 8 or color_type not in (2, 6) or interlace != 0:
                sys.exit(f"{path}: only 8-bit non-interlaced RGB/RGBA is supported")
        elif kind == b"IDAT":
            idat += body
        elif kind == b"IEND":
            break
    channels = 4 if color_type == 6 else 3
    stride = width * channels
    raw = zlib.decompress(bytes(idat))
    out = bytearray(stride * height)
    prev = bytearray(stride)
    for row in range(height):
        base = row * (stride + 1)
        mode = raw[base]
        line = bytearray(raw[base + 1:base + 1 + stride])
        for i in range(stride):
            left = line[i - channels] if i >= channels else 0
            up = prev[i]
            corner = prev[i - channels] if i >= channels else 0
            if mode == 1:
                line[i] = (line[i] + left) & 0xFF
            elif mode == 2:
                line[i] = (line[i] + up) & 0xFF
            elif mode == 3:
                line[i] = (line[i] + ((left + up) >> 1)) & 0xFF
            elif mode == 4:
                line[i] = (line[i] + paeth(left, up, corner)) & 0xFF
        out[row * stride:(row + 1) * stride] = line
        prev = line
    if channels == 4:
        rgb = bytearray(width * height * 3)
        rgb[0::3] = out[0::4]
        rgb[1::3] = out[1::4]
        rgb[2::3] = out[2::4]
        return image(width, height, bytes(rgb))
    return image(width, height, bytes(out))


def crop(source: image, region: tuple[int, int, int, int]) -> image:
    x0, y0, x1, y1 = region
    x0, x1 = max(0, x0), min(source.width, x1)
    y0, y1 = max(0, y0), min(source.height, y1)
    if x0 >= x1 or y0 >= y1:
        sys.exit(f"region {region} is empty inside a {source.width}x{source.height} frame")
    stride = source.width * 3
    rows = [source.rgb[y * stride + x0 * 3:y * stride + x1 * 3] for y in range(y0, y1)]
    return image(x1 - x0, y1 - y0, b"".join(rows))


def compare(a_path: Path, b_path: Path, region: tuple[int, int, int, int] | None = None) -> comparison:
    a = read_png(a_path)
    b = read_png(b_path)
    if (a.width, a.height) != (b.width, b.height):
        return comparison(str(a_path), str(b_path), a.width, a.height, 255, 255.0, 1.0, 0.0, False, f"size mismatch: {a.width}x{a.height} vs {b.width}x{b.height}")
    if region is not None:
        a = crop(a, region)
        b = crop(b, region)
    max_error = 0
    total = 0
    squared = 0
    over = 0
    for i in range(0, len(a.rgb), 3):
        e0 = abs(a.rgb[i] - b.rgb[i])
        e1 = abs(a.rgb[i + 1] - b.rgb[i + 1])
        e2 = abs(a.rgb[i + 2] - b.rgb[i + 2])
        worst = max(e0, e1, e2)
        max_error = max(max_error, worst)
        total += e0 + e1 + e2
        squared += e0 * e0 + e1 * e1 + e2 * e2
        if worst > pixel_threshold:
            over += 1
    pixels = a.width * a.height
    mean_error = total / (pixels * 3)
    mse = squared / (pixels * 3)
    psnr = math.inf if mse == 0 else 10.0 * math.log10(255.0 * 255.0 / mse)
    over_fraction = over / pixels
    return comparison(
        str(a_path),
        str(b_path),
        a.width,
        a.height,
        max_error,
        mean_error,
        over_fraction,
        psnr,
        mean_error <= tolerance_max_mean_error and over_fraction <= tolerance_max_over_fraction,
    )


@dataclass
class region_error:
    name: str
    pixels: int
    mean_error: float
    over_pixels: int
    over_fraction_of_frame: float


def write_png(path: Path, width: int, height: int, rgb: bytes) -> None:
    stride = width * 3
    raw = b"".join(b"\x00" + rgb[row * stride:(row + 1) * stride] for row in range(height))

    def chunk(kind: bytes, body: bytes) -> bytes:
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)

    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))


def region_breakdown(a_path: Path, b_path: Path, regions: dict[str, tuple[int, int, int, int]], heatmap: Path | None = None) -> list[region_error]:
    a = read_png(a_path)
    b = read_png(b_path)
    if (a.width, a.height) != (b.width, b.height):
        return []
    names = list(regions) + ["scene"]
    totals = {n: [0, 0, 0] for n in names}
    owner = [len(regions)] * (a.width * a.height)
    for index, (x0, y0, x1, y1) in enumerate(regions.values()):
        for y in range(max(0, y0), min(a.height, y1)):
            for x in range(max(0, x0), min(a.width, x1)):
                owner[y * a.width + x] = index
    heat = bytearray(a.width * a.height * 3) if heatmap else None
    for p in range(a.width * a.height):
        i = p * 3
        e0 = abs(a.rgb[i] - b.rgb[i])
        e1 = abs(a.rgb[i + 1] - b.rgb[i + 1])
        e2 = abs(a.rgb[i + 2] - b.rgb[i + 2])
        worst = max(e0, e1, e2)
        t = totals[names[owner[p]]]
        t[0] += 1
        t[1] += e0 + e1 + e2
        t[2] += worst > pixel_threshold
        if heat is not None:
            level = min(255, worst * 8)
            heat[i] = level
            heat[i + 1] = level if worst > pixel_threshold else 0
    if heat is not None:
        write_png(heatmap, a.width, a.height, bytes(heat))
    frame_pixels = a.width * a.height
    return [region_error(n, t[0], t[1] / (t[0] * 3) if t[0] else 0.0, t[2], t[2] / frame_pixels) for n, t in totals.items()]


@dataclass
class sharpness_result:
    frame: str
    width: int
    height: int
    mean_gradient: float


def luma_of(source: image) -> list[float]:
    rgb = source.rgb
    return [(rgb[i] * 299 + rgb[i + 1] * 587 + rgb[i + 2] * 114) / 1000.0 for i in range(0, len(rgb), 3)]


def sharpness(path: Path, region: tuple[int, int, int, int] | None = None) -> sharpness_result:
    source = read_png(path)
    if region is not None:
        source = crop(source, region)
    if source.width < 2 or source.height < 2:
        sys.exit(f"{path}: sharpness needs at least a 2x2 frame")
    luma = luma_of(source)
    width = source.width
    total = 0.0
    for y in range(source.height - 1):
        row = y * width
        for x in range(width - 1):
            here = luma[row + x]
            total += math.hypot(luma[row + x + 1] - here, luma[row + width + x] - here)
    return sharpness_result(str(path), source.width, source.height, total / ((source.width - 1) * (source.height - 1)))


@dataclass
class lattice_result:
    frame: str
    width: int
    height: int
    period: int
    peak_to_peak: float
    phase_rms: float
    highpass_rms: float


def lattice(path: Path, period: int, region: tuple[int, int, int, int] | None = None) -> lattice_result:
    source = read_png(path)
    if region is not None:
        source = crop(source, region)
    width, height = source.width, source.height
    if period < 2 or width < 2 * period or height < 2 * period:
        sys.exit(f"{path}: lattice needs a period of at least 2 and a frame of at least two periods on each side")
    luma = luma_of(source)
    stride = width + 1
    integral = [0.0] * (stride * (height + 1))
    for y in range(height):
        running = 0.0
        above = y * stride
        here = above + stride
        row = y * width
        for x in range(width):
            running += luma[row + x]
            integral[here + x + 1] = integral[above + x + 1] + running
    before = period // 2
    after = period - before
    area = period * period
    sums = [0.0] * area
    counts = [0] * area
    squared = 0.0
    for y in range(before, height - after + 1):
        top = (y - before) * stride
        bottom = (y + after) * stride
        row = y * width
        phase_row = (y % period) * period
        for x in range(before, width - after + 1):
            left = x - before
            right = x + after
            box = (integral[bottom + right] - integral[top + right] - integral[bottom + left] + integral[top + left]) / area
            residual = luma[row + x] - box
            phase = phase_row + x % period
            sums[phase] += residual
            counts[phase] += 1
            squared += residual * residual
    means = [s / c for s, c in zip(sums, counts)]
    centre = sum(means) / area
    return lattice_result(
        str(path),
        width,
        height,
        period,
        max(means) - min(means),
        math.sqrt(sum((m - centre) * (m - centre) for m in means) / area),
        math.sqrt(squared / sum(counts)),
    )


def write_report(path: Path | None, results: list[comparison]) -> None:
    if path is None:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps({
        "tolerance": {
            "pixel_threshold": pixel_threshold,
            "max_mean_error": tolerance_max_mean_error,
            "max_over_fraction": tolerance_max_over_fraction,
        },
        "comparisons": [asdict(r) | {"psnr": None if math.isinf(r.psnr) else r.psnr} for r in results],
    }, indent=2))


def print_result(r: comparison) -> None:
    psnr = "inf" if math.isinf(r.psnr) else f"{r.psnr:.2f} dB"
    verdict = "PASS" if r.within_tolerance else "FAIL"
    if r.note:
        print(f"{verdict} {Path(r.a).name} vs {Path(r.b).name}: {r.note}")
        return
    print(f"{verdict} {Path(r.a).name} vs {Path(r.b).name}: {r.width}x{r.height} max {r.max_error} mean {r.mean_error:.4f} over {r.over_fraction:.5f} psnr {psnr}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Compare presented frames within the parity tolerance.")
    sub = parser.add_subparsers(dest="command", required=True)
    diff = sub.add_parser("diff")
    diff.add_argument("a", type=Path)
    diff.add_argument("b", type=Path)
    diff.add_argument("--report", type=Path)
    diff.add_argument("--region", type=int, nargs=4, metavar=("X0", "Y0", "X1", "Y1"), help="compare only this pixel rectangle; X1 and Y1 are exclusive")
    noise = sub.add_parser("noise")
    noise.add_argument("frames", type=Path, nargs="+")
    noise.add_argument("--report", type=Path)
    sharp = sub.add_parser("sharpness")
    sharp.add_argument("frames", type=Path, nargs="+")
    sharp.add_argument("--region", type=int, nargs=4, metavar=("X0", "Y0", "X1", "Y1"), help="measure only this pixel rectangle; X1 and Y1 are exclusive")
    grid = sub.add_parser("lattice")
    grid.add_argument("frames", type=Path, nargs="+")
    grid.add_argument("--period", type=int, default=8, help="side of the repeating pixel block to look for")
    grid.add_argument("--region", type=int, nargs=4, metavar=("X0", "Y0", "X1", "Y1"), help="measure only this pixel rectangle; X1 and Y1 are exclusive")
    args = parser.parse_args()

    if args.command == "lattice":
        for frame in args.frames:
            g = lattice(frame, args.period, tuple(args.region) if args.region else None)
            print(f"{Path(g.frame).parent.name}/{Path(g.frame).name}: {g.width}x{g.height} period {g.period} peak-to-peak {g.peak_to_peak:.4f} phase rms {g.phase_rms:.4f} highpass rms {g.highpass_rms:.4f}")
        return 0

    if args.command == "sharpness":
        for frame in args.frames:
            s = sharpness(frame, tuple(args.region) if args.region else None)
            print(f"{Path(s.frame).parent.name}/{Path(s.frame).name}: {s.width}x{s.height} mean gradient {s.mean_gradient:.4f}")
        return 0

    if args.command == "diff":
        results = [compare(args.a, args.b, tuple(args.region) if args.region else None)]
    else:
        if len(args.frames) < 2:
            sys.exit("noise needs at least two frames")
        results = [compare(a, b) for a, b in itertools.combinations(args.frames, 2)]

    for r in results:
        print_result(r)
    write_report(args.report, results)
    return 0 if all(r.within_tolerance for r in results) else 1


if __name__ == "__main__":
    sys.exit(main())
