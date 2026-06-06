"""Build the native-Windows GCC trunk toolchain from an official weekly snapshot,
using the vendored MSYS2 mingw-w64-gcc recipe (scripts/gcc-toolchain/recipe).

Drives MSYS2 (makepkg + pacman) via `bash -lc`, then hands the resulting mingw64
prefix to package_gcc_toolchain.py to produce dist/gcc-trunk-windows-x64.zip.

Runs the same in CI (windows-latest + msys2/setup-msys2) and locally (repo msys64).
The vendored recipe carries MSYS2's Windows patch stack — a plain configure/make
of GCC trunk does NOT build on native Windows, which is why this goes through
makepkg rather than configuring GCC directly.

    python scripts/build_gcc_trunk.py --snapshot 20260601 --msys-root C:/msys64
"""
import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent


def to_msys(p: Path) -> str:
    s = str(Path(p).resolve())
    if len(s) > 1 and s[1] == ":":
        return "/" + s[0].lower() + s[2:].replace("\\", "/")
    return s.replace("\\", "/")


def bash(msys_root: Path, script: str, check: bool = True) -> subprocess.CompletedProcess:
    bash_exe = msys_root / "usr" / "bin" / "bash.exe"
    if not bash_exe.exists():
        raise SystemExit(f"bash not found at {bash_exe} (wrong --msys-root?)")
    print(f"$ (msys2) {script}", flush=True)
    res = subprocess.run(
        [str(bash_exe), "-lc", script],
        env={"MSYSTEM": "MINGW64", "CHERE_INVOKING": "1", "PATH": r"C:\Windows\System32;C:\Windows"},
    )
    if check and res.returncode != 0:
        raise SystemExit(f"msys2 command failed ({res.returncode}): {script}")
    return res


def set_snapshot(pkgbuild: Path, snapshot: str) -> None:
    text = pkgbuild.read_text()
    text, n = re.subn(r"^_snapshot=.*$", f"_snapshot={snapshot}", text, count=1, flags=re.M)
    if n != 1:
        raise SystemExit("could not set _snapshot in PKGBUILD")
    # makepkg refuses to source a PKGBUILD containing CRLF. Path.write_text() on
    # Windows translates every "\n" -> "\r\n", which would corrupt the recipe, so
    # write LF explicitly. The explicit newline="\n" on open() also normalises any
    # CRLF a Windows git checkout (autocrlf) introduced into the source file.
    with open(pkgbuild, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    print(f"Set _snapshot={snapshot}")


def stage_cached_source(build_recipe: Path, snapshot: str, cache_dirs: list) -> None:
    """Make makepkg use a locally cached GCC source tarball instead of downloading.

    GCC removes old weekly snapshots from its server, so building a pinned older
    snapshot 404s. If a matching tarball is cached locally, copy it into the recipe dir
    (makepkg checks startdir before downloading). A plain .tar is supported by pointing
    the PKGBUILD source at it, which avoids a slow recompression to .tar.xz.
    """
    for cache in cache_dirs:
        if not cache or not Path(cache).is_dir():
            continue
        for xz in sorted(Path(cache).glob(f"gcc-*-{snapshot}.tar.xz")):
            print(f"Using cached source {xz} (skipping download)")
            shutil.copy2(xz, build_recipe / xz.name)
            return
        for plain in sorted(Path(cache).glob(f"gcc-*-{snapshot}.tar")):
            print(f"Using cached source {plain} (skipping download; uncompressed)")
            shutil.copy2(plain, build_recipe / plain.name)
            pk = build_recipe / "PKGBUILD"
            text = pk.read_text(encoding="utf-8").replace("${_sourcedir}.tar.xz", "${_sourcedir}.tar", 1)
            with open(pk, "w", encoding="utf-8", newline="\n") as f:
                f.write(text)
            return
    print(f"No cached source for snapshot {snapshot}; makepkg will download it")


def main() -> None:
    p = argparse.ArgumentParser(description="Build the GCC trunk toolchain via the vendored MSYS2 recipe")
    p.add_argument("--snapshot", required=True, help="GCC weekly snapshot date, e.g. 20260601")
    p.add_argument("--msys-root", type=Path, default=REPO_ROOT / "msys64", help="MSYS2 root (default <repo>/msys64)")
    p.add_argument("--recipe", type=Path, default=REPO_ROOT / "scripts" / "gcc-toolchain" / "recipe")
    p.add_argument("--src-cache", type=Path, default=REPO_ROOT / ".gcc-src-cache",
                   help="Dir of cached gcc-<ver>-<snapshot>.tar(.xz) tarballs, used when GCC "
                        "has pruned the snapshot from its server (also checks <repo>/.gcc-build)")
    p.add_argument("--workdir", type=Path, default=REPO_ROOT / ".gcc-ci-build", help="Scratch build dir")
    p.add_argument("--out", type=Path, default=REPO_ROOT / "dist" / "gcc-trunk-windows-x64.zip")
    p.add_argument("--jobs", type=int, default=4)
    args = p.parse_args()

    msys_root = args.msys_root.resolve()
    build_recipe = args.workdir / "mingw-w64-gcc"
    if build_recipe.exists():
        shutil.rmtree(build_recipe)
    build_recipe.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(args.recipe, build_recipe)
    set_snapshot(build_recipe / "PKGBUILD", args.snapshot)
    stage_cached_source(build_recipe, args.snapshot, [args.src_cache, REPO_ROOT / ".gcc-build"])

    recipe_msys = to_msys(build_recipe)
    # Build: fetch the official snapshot tarball, apply the vendored patch stack, build c/c++/lto.
    bash(msys_root,
         f"cd {recipe_msys} && MINGW_ARCH=mingw64 MAKEFLAGS=-j{args.jobs} "
         f"makepkg-mingw -sCLf --noconfirm --skippgpcheck --nocheck")

    # Install the freshly built packages into /mingw64 so the closure is complete.
    pkgs = sorted(to_msys(x) for x in build_recipe.glob("mingw-w64-x86_64-gcc*-any.pkg.tar.zst"))
    if not pkgs:
        raise SystemExit("no built packages found — makepkg failed?")
    bash(msys_root, f"pacman -U --noconfirm --overwrite '*' {' '.join(pkgs)}")

    # Stage the relocatable toolchain + zip it (shared with local relocation).
    print("\n== packaging toolchain ==")
    rc = subprocess.run(
        [sys.executable, str(REPO_ROOT / "scripts" / "package_gcc_toolchain.py"),
         "--msys-root", str(msys_root),
         "--stage", str(args.out.parent / "gcc-trunk"),
         "--out", str(args.out)],
    )
    if rc.returncode != 0:
        raise SystemExit("packaging failed")
    print(f"\nArtifact: {args.out}")


if __name__ == "__main__":
    main()
