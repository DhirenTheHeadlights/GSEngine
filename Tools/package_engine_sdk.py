import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

COMPILE_FLAGS = [
    "-std=c++26", "-freflection", "-mavx2", "-Wno-changes-meaning", "-Wno-error=changes-meaning",
    "-fmodules-ts", "-O2", "-DNDEBUG",
]

HELLO_SOURCE = """import std;
import gse;

auto main() -> int {
	std::println("sdk ok: {}", gse::meters(6.f));
	return 0;
}
"""


def manifest_value(text: str, key: str) -> str:
    for line in text.splitlines():
        name, sep, value = line.partition("=")
        if sep and name.strip() == key:
            return value.strip()
    return ""


def project_name(build: Path) -> str:
    project = manifest_value((build / "gse.manifest").read_text(), "project")
    if not project:
        raise SystemExit(f"{build / 'gse.manifest'} has no project line; is this a configured build tree?")
    return Path(project).name


def copy_file(src: Path, dst: Path) -> None:
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)


def copy_tree(src: Path, dst: Path) -> int:
    count = 0
    for file in src.rglob("*"):
        if file.is_file():
            copy_file(file, dst / file.relative_to(src))
            count += 1
    return count


def stage_modules(build: Path, stage: Path, modules: dict[str, str]) -> int:
    for path in set(modules.values()):
        copy_file(build / path, stage / path)
    return len(set(modules.values()))


def stage_libs(build: Path, stage: Path) -> int:
    libs = sorted((build / "Engine").glob("lib*.a"))
    for lib in libs:
        copy_file(lib, stage / lib.relative_to(build))
    return len(libs)


def stage_bin(build: Path, project: str, stage: Path) -> int:
    source = build / project
    count = 0
    for dll in source.glob("*.dll"):
        copy_file(dll, stage / "Bin" / dll.name)
        count += 1
    if (source / "D3D12").is_dir():
        count += copy_tree(source / "D3D12", stage / "Bin" / "D3D12")
    return count


def module_listing(build: Path, stage: Path) -> dict[str, str]:
    prefix = build.as_posix() + "/"
    modules: dict[str, str] = {}
    for listing in sorted((build / "Engine" / "CMakeFiles").glob("*.dir/CXXModules.json")):
        for name, entry in json.loads(listing.read_text())["modules"].items():
            bmi = entry["bmi"]
            if not bmi.startswith(prefix):
                raise SystemExit(f"{name}: BMI {bmi} is outside the build tree")
            modules[name] = bmi[len(prefix):]
    lines = ["$root ."] + [f"{name} {path}" for name, path in sorted(modules.items())]
    write_lf(stage / "gse.modules", "\n".join(lines) + "\n")
    return modules


def write_lf(path: Path, text: str) -> None:
    with open(path, "w", newline="\n") as file:
        file.write(text)


def link_stanza(build: Path, project: str) -> tuple[list[str], list[str]]:
    target = f"build {project}/{project}.exe:"
    values: dict[str, str] = {}
    inside = False
    for line in (build / "build.ninja").read_text(encoding="utf-8").splitlines():
        if line.startswith(target):
            inside = True
            continue
        if inside:
            if not line.startswith("  "):
                break
            key, sep, value = line.strip().partition(" = ")
            if sep:
                values[key] = value
    if "LINK_LIBRARIES" not in values:
        raise SystemExit(f"no link stanza for {project}.exe in {build / 'build.ninja'}")
    return values["LINK_LIBRARIES"].split(), values.get("LINK_FLAGS", "").split()


def verify(stage: Path, build: Path, project: str, toolchain: Path) -> None:
    gxx = toolchain / "bin" / "g++.exe"
    if not gxx.exists():
        raise SystemExit(f"toolchain compiler missing: {gxx}")
    work = stage / ".verify"
    if work.exists():
        shutil.rmtree(work)
    work.mkdir()
    write_lf(work / "hello.cpp", HELLO_SOURCE)

    env = dict(os.environ)
    env["PATH"] = os.pathsep.join([str(toolchain / "bin"), str(stage / "Bin"), env.get("PATH", "")])

    def run(args: list[str], cwd: Path) -> str:
        result = subprocess.run(args, cwd=cwd, env=env, capture_output=True, text=True)
        if result.returncode != 0:
            raise SystemExit(f"verify failed ({result.returncode}): {' '.join(args)}\n{result.stdout}{result.stderr}")
        return result.stdout

    print("verify: compiling hello.cpp against the staged image")
    run([str(gxx), *COMPILE_FLAGS, "-fmodule-mapper=gse.modules", "-c", ".verify/hello.cpp", "-o", ".verify/hello.obj"], stage)

    libraries, link_flags = link_stanza(build, project)
    resolved: list[str] = []
    for token in libraries:
        if token.startswith(f"{project}/"):
            continue
        if token.startswith("Engine/"):
            resolved.append((stage / token).as_posix())
        elif token.startswith("vcpkg_installed/"):
            resolved.append((build / token).as_posix())
        else:
            resolved.append(token)
    print(f"verify: linking hello.exe ({len(resolved)} link inputs)")
    run([str(gxx), ".verify/hello.obj", "-o", ".verify/hello.exe", *link_flags, *resolved], stage)

    output = run([str(work / "hello.exe")], work)
    result = next((line for line in output.splitlines() if line.startswith("sdk ok")), "")
    if not result:
        raise SystemExit(f"verify failed: unexpected output\n{output}")
    print(f"verify: {result}")
    shutil.rmtree(work)


def main() -> None:
    repo_root = Path(__file__).resolve().parent.parent
    p = argparse.ArgumentParser(description="Stage a relocatable engine SDK image from a configured build tree")
    p.add_argument("--build-dir", type=Path, default=repo_root / "out" / "build" / "x64-mingw-gcc-RelWithDebInfo",
                   help="Build tree holding the engine BMIs, libs and baked resources")
    p.add_argument("--engine-root", type=Path, default=repo_root,
                   help="Engine source tree (default: this repo)")
    p.add_argument("--stage", type=Path, default=repo_root / "dist" / "engine-sdk",
                   help="Directory to stage the image into")
    p.add_argument("--toolchain", type=Path, default=Path.home() / ".gcc-trunk" / "current",
                   help="GCC toolchain root used by --verify")
    p.add_argument("--out", type=Path, default=None,
                   help="Also produce this .zip of the staged image")
    p.add_argument("--verify", action="store_true",
                   help="Compile, link and run a hello TU importing std and gse against the staged image")
    args = p.parse_args()

    build = args.build_dir.resolve()
    engine = args.engine_root.resolve()
    stage = args.stage.resolve()
    if not (build / "gse.manifest").exists():
        raise SystemExit(f"{build} is not a configured build tree (no gse.manifest)")
    project = project_name(build)
    print(f"build tree: {build}\nengine:     {engine}\nproject:    {project}\nstage:      {stage}")

    if stage.exists():
        shutil.rmtree(stage)
    stage.mkdir(parents=True)

    modules = module_listing(build, stage)
    print(f"listing:   {len(modules)} modules -> gse.modules")
    print(f"modules:   {stage_modules(build, stage, modules)} BMIs")
    print(f"libraries: {stage_libs(build, stage)} archives")
    print(f"resources: {copy_tree(engine / 'Engine' / 'Resources', stage / 'Engine' / 'Resources')} files")
    print(f"source:    {copy_tree(engine / 'Engine' / 'Engine', stage / 'Engine' / 'Source')} files")
    print(f"baked:     {copy_tree(build / 'Engine' / 'Resources', stage / 'Engine' / 'Baked')} files")
    print(f"bin:       {stage_bin(build, project, stage)} files")
    write_lf(stage / "gse.manifest", "mode = installed\n")

    if args.verify:
        verify(stage, build, project, args.toolchain.resolve())

    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        if args.out.exists():
            args.out.unlink()
        print(f"zipping -> {args.out}")
        shutil.make_archive(str(args.out.with_suffix("")), "zip", root_dir=stage)

    print(f"\nSDK image: {stage}")


if __name__ == "__main__":
    main()
