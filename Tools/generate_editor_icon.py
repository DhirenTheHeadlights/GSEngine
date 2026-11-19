import re
import struct
import zlib
import xml.etree.ElementTree as xml
from pathlib import Path


def outlines(path):
    result = {}
    for element in xml.parse(path).getroot():
        role = element.attrib["id"]
        tokens = iter(re.findall(r"[MLQZ]|-?[\d.]+", element.attrib["d"]))
        points = []
        position = None
        for command in tokens:
            if command in ("M", "L"):
                position = (float(next(tokens)), float(next(tokens)))
                points.append(position)
            elif command == "Q":
                control = (float(next(tokens)), float(next(tokens)))
                end = (float(next(tokens)), float(next(tokens)))
                start = position
                for step in range(1, 5):
                    t = step / 4
                    points.append(tuple(round((1 - t) ** 2 * start[i] + 2 * (1 - t) * t * control[i] + t ** 2 * end[i], 6) for i in range(2)))
                position = end
            elif command != "Z":
                raise ValueError(f"Unsupported Sweep path command: {command}")
        result[role] = points
    if set(result) != {"accent", "foreground", "secondary"}:
        raise ValueError("Sweep.svg must define the three named blades")
    return result


def cross(a, b, c):
    return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])


def triangulate(points):
    remaining = list(range(len(points)))
    triangles = []
    while len(remaining) > 2:
        for i, b in enumerate(remaining):
            a = remaining[i - 1]
            c = remaining[(i + 1) % len(remaining)]
            if cross(points[a], points[b], points[c]) <= 1e-10:
                continue
            occupied = any(
                p not in (a, b, c)
                and cross(points[a], points[b], points[p]) >= -1e-10
                and cross(points[b], points[c], points[p]) >= -1e-10
                and cross(points[c], points[a], points[p]) >= -1e-10
                for p in remaining
            )
            if not occupied:
                triangles.append((a, b, c))
                del remaining[i]
                break
        else:
            raise ValueError("Sweep outline cannot be triangulated")
    outline_area = sum(a[0] * b[1] - a[1] * b[0] for a, b in zip(points, points[1:] + points[:1]))
    mesh_area = sum(cross(points[a], points[b], points[c]) for a, b, c in triangles)
    if abs(outline_area - mesh_area) > 1e-6:
        raise ValueError("Sweep mesh does not cover its outline")
    return triangles


def module(geometry):
    lines = [
        "export module gse.ide.app:brand_shape;", "", "import gse.math;", "import std;", "",
        "export namespace gse::ide::brand_shape {", "\tstruct blade {",
        "\t\tstd::span<const vec2f> outline;",
        "\t\tstd::span<const std::array<std::uint32_t, 3>> triangles;", "\t};", "",
    ]
    for role in geometry:
        lines.extend([f"\tauto {role}() -> blade;", ""])
    lines[-1] = "}"
    for role, points in geometry.items():
        triangles = triangulate(points)
        lines.extend(["", f"auto gse::ide::brand_shape::{role}() -> blade {{", f"\tstatic constexpr std::array<vec2f, {len(points)}> outline{{"])
        for x, y in points:
            lines.append(f"\t\tvec2f{{ {x:.6f}f, {y:.6f}f }},")
        lines.extend(["\t};", f"\tstatic constexpr std::array<std::array<std::uint32_t, 3>, {len(triangles)}> triangles{{ {{"])
        for a, b, c in triangles:
            lines.append(f"\t\t{{ {a}, {b}, {c} }},")
        lines.extend(["\t} };", "\treturn {", "\t\t.outline = outline,", "\t\t.triangles = triangles,", "\t};", "}"])
    return "\n".join(lines) + "\n"


def midnight(source):
    body = source.split("style::midnight() -> style {", 1)[1].split("style::eclipse()", 1)[0]
    fields = {
        "accent": r"constexpr vec4f accent\{ (.*?) \}",
        "foreground": r"\.color_text = \{ (.*?) \}",
        "secondary": r"\.color_icon = \{ (.*?) \}",
        "background": r"\.color_title_bar = \{ (.*?) \}",
    }
    return {
        role: tuple(int(float(value.removesuffix("f")) * 255 + 0.5) for value in re.search(pattern, body)[1].split(", ")[:3])
        for role, pattern in fields.items()
    }


def intervals(points, y):
    crossings = sorted(
        a[0] + (y - a[1]) * (b[0] - a[0]) / (b[1] - a[1])
        for a, b in zip(points, points[1:] + points[:1])
        if (a[1] > y) != (b[1] > y)
    )
    return list(zip(crossings[::2], crossings[1::2]))


def png(extent, geometry, palette):
    rows = bytearray()
    for y in range(extent):
        total = [[0, 0, 0, 0] for _ in range(extent)]
        for sy in range(4):
            py = (y + (sy + 0.5) / 4) * 100 / extent
            dy = max(abs(py - 50) - 33, 0)
            spans = [(intervals(points, py), palette[role]) for role, points in geometry.items()]
            for x in range(extent):
                for sx in range(4):
                    px = (x + (sx + 0.5) / 4) * 100 / extent
                    dx = max(abs(px - 50) - 33, 0)
                    if dx * dx + dy * dy > 225:
                        continue
                    color = palette["background"]
                    for ranges, paint in spans:
                        if any(left <= px < right for left, right in ranges):
                            color = paint
                            break
                    total[x][3] += 1
                    for channel in range(3):
                        total[x][channel] += color[channel]
        rows.append(0)
        for r, g, b, coverage in total:
            rows.extend([*(int(value / coverage + 0.5) if coverage else 0 for value in (r, g, b)), int(255 * coverage / 16 + 0.5)])

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">2I5B", extent, extent, 8, 6, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b"")


def main():
    root = Path(__file__).resolve().parents[1]
    source = root / "Editor/Resources/Icons/Sweep.svg"
    geometry = outlines(source)
    mesh = module(geometry)
    (root / "Editor/Editor/Source/App/BrandShape.cppm").write_text(mesh)
    palette = midnight((root / "Engine/Engine/Source/Graphics/2D/Gui/Styles.cppm").read_text())
    destination = root / "Editor/Resources/Icons"
    destination.mkdir(parents=True, exist_ok=True)
    sizes = (16, 24, 32, 48, 64, 128, 256)
    images = [png(size, geometry, palette) for size in sizes]
    offset = 6 + 16 * len(images)
    entries = []
    for size, data in zip(sizes, images):
        entries.append(struct.pack("<4B2H2I", size % 256, size % 256, 0, 0, 1, 32, len(data), offset))
        offset += len(data)
    (destination / "GSEditor.ico").write_bytes(struct.pack("<3H", 0, 1, len(images)) + b"".join(entries) + b"".join(images))
    (destination / "GSEditor.png").write_bytes(images[-1])
    paths = []
    for role, points in geometry.items():
        color = "#" + "".join(f"{value:02x}" for value in palette[role])
        coordinates = " ".join(f"{x:.6f},{y:.6f}" for x, y in points)
        paths.append(f'<polygon points="{coordinates}" fill="{color}"/>')
    background = "#" + "".join(f"{value:02x}" for value in palette["background"])
    (destination / "GSEditor.svg").write_text(
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100">\n'
        + f'<rect x="2" y="2" width="96" height="96" rx="15" fill="{background}"/>\n'
        + "\n".join(paths) + "\n</svg>\n"
    )
    print(destination / "GSEditor.ico")


if __name__ == "__main__":
    main()
