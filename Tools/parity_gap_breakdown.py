import argparse
import sys
from pathlib import Path

from frame_parity import pixel_threshold, read_png

grid = 8


def stats(errors: list[tuple[int, int, int]]) -> tuple[float, float]:
    if not errors:
        return 0.0, 0.0
    total = sum(abs(e0) + abs(e1) + abs(e2) for e0, e1, e2 in errors)
    over = sum(1 for e in errors if max(abs(c) for c in e) > pixel_threshold)
    return total / (len(errors) * 3), over / len(errors)


def is_sky(r: int, g: int, b: int) -> bool:
    return b > r and b >= g and b > 96


def main() -> int:
    parser = argparse.ArgumentParser(description="Splits the cross-backend frame error of a parity report into a global signed offset, sky vs geometry regions and a screen grid, so a localized overlay such as the HUD shows up as a hot cell.")
    parser.add_argument("candidate", type=Path)
    parser.add_argument("reference", type=Path)
    args = parser.parse_args()

    a = read_png(args.candidate)
    b = read_png(args.reference)
    if (a.width, a.height) != (b.width, b.height):
        print(f"size mismatch: {a.width}x{a.height} vs {b.width}x{b.height}")
        return 2

    errors = []
    sky = []
    geometry = []
    cells: dict[tuple[int, int], list[tuple[int, int, int]]] = {}
    for y in range(a.height):
        for x in range(a.width):
            i = (y * a.width + x) * 3
            e = (a.rgb[i] - b.rgb[i], a.rgb[i + 1] - b.rgb[i + 1], a.rgb[i + 2] - b.rgb[i + 2])
            errors.append(e)
            (sky if is_sky(b.rgb[i], b.rgb[i + 1], b.rgb[i + 2]) else geometry).append(e)
            cells.setdefault((x * grid // a.width, y * grid // a.height), []).append(e)

    offset = tuple(sum(e[c] for e in errors) / len(errors) for c in range(3))
    shift = tuple(round(o) for o in offset)
    shifted = [(e0 - shift[0], e1 - shift[1], e2 - shift[2]) for e0, e1, e2 in errors]

    mean, over = stats(errors)
    print(f"full frame: mean {mean:.3f}, over {pixel_threshold}: {over * 100:.2f}%")
    print(f"signed mean offset (candidate - reference) rgb: {offset[0]:+.3f} {offset[1]:+.3f} {offset[2]:+.3f}")
    mean, over = stats(shifted)
    print(f"after removing offset {shift}: mean {mean:.3f}, over {pixel_threshold}: {over * 100:.2f}%")
    for name, region in (("sky", sky), ("geometry", geometry)):
        mean, over = stats(region)
        signed = tuple(sum(e[c] for e in region) / max(len(region), 1) for c in range(3))
        print(f"{name}: {len(region) / len(errors) * 100:.1f}% of pixels, mean {mean:.3f}, over {pixel_threshold}: {over * 100:.2f}%, signed rgb {signed[0]:+.2f} {signed[1]:+.2f} {signed[2]:+.2f}")
    print(f"mean error per {grid}x{grid} cell (row = screen top to bottom):")
    for gy in range(grid):
        print("  " + " ".join(f"{stats(cells[(gx, gy)])[0]:6.2f}" for gx in range(grid)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
