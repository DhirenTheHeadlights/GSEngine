"""Build the native-Windows GCC trunk toolchain from an official weekly snapshot,
using the vendored MSYS2 mingw-w64-gcc recipe (Tools/gcc-toolchain/recipe).

Drives MSYS2 (makepkg + pacman) via the shared helpers in Tools/msys2.py, then hands
the resulting mingw64 prefix to package_gcc_toolchain.py to produce
.generated/dist/gcc-trunk-windows-x64.zip.

Runs the same in CI (windows-latest + msys2/setup-msys2) and locally (repo .generated/msys64).
The vendored recipe carries MSYS2's Windows patch stack — a plain configure/make
of GCC trunk does NOT build on native Windows, which is why this goes through
makepkg rather than configuring GCC directly.

    python Tools/build_gcc_trunk.py --snapshot 20260601
"""
import argparse
import shutil
import subprocess
import sys
from pathlib import Path

from msys2 import DEFAULT_MSYS_ROOT, REPO_ROOT, bash, set_snapshot, stage_cached_source, to_msys
from repo_paths import DIST_ROOT, GENERATED_ROOT


def main() -> None:
    p = argparse.ArgumentParser(description="Build the GCC trunk toolchain via the vendored MSYS2 recipe")
    p.add_argument("--snapshot", required=True, help="GCC weekly snapshot date, e.g. 20260601")
    p.add_argument("--msys-root", type=Path, default=DEFAULT_MSYS_ROOT, help="MSYS2 root (default <repo>/.generated/msys64)")
    p.add_argument("--recipe", type=Path, default=REPO_ROOT / "Tools" / "gcc-toolchain" / "recipe")
    p.add_argument("--src-cache", type=Path, default=GENERATED_ROOT / ".gcc-src-cache",
                   help="Dir of cached gcc-<ver>-<snapshot>.tar(.xz) tarballs, used when GCC "
                        "has pruned the snapshot from its server (also checks <repo>/.generated/.gcc-build)")
    p.add_argument("--workdir", type=Path, default=GENERATED_ROOT / ".gcc-ci-build", help="Scratch build dir")
    p.add_argument("--out", type=Path, default=DIST_ROOT / "gcc-trunk-windows-x64.zip")
    p.add_argument("--jobs", type=int, default=4)
    args = p.parse_args()

    msys_root = args.msys_root.resolve()
    build_recipe = args.workdir / "mingw-w64-gcc"
    if build_recipe.exists():
        shutil.rmtree(build_recipe)
    build_recipe.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(args.recipe, build_recipe)
    set_snapshot(build_recipe / "PKGBUILD", args.snapshot)
    stage_cached_source(build_recipe, args.snapshot, [args.src_cache, GENERATED_ROOT / ".gcc-build"])

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
        [sys.executable, str(REPO_ROOT / "Tools" / "package_gcc_toolchain.py"),
         "--msys-root", str(msys_root),
         "--stage", str(args.out.parent / "gcc-trunk"),
         "--out", str(args.out)],
    )
    if rc.returncode != 0:
        raise SystemExit("packaging failed")
    print(f"\nArtifact: {args.out}")


if __name__ == "__main__":
    main()
