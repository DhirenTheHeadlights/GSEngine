"""Assemble a portable, self-contained native-Windows GCC toolchain from an MSYS2
mingw64 prefix.

The staged tree's root IS the mingw64 prefix (so `<root>/bin/g++.exe` works) and
contains exactly the recursive runtime dependency closure of
`mingw-w64-x86_64-gcc` (resolved with `pactree`), minus docs/man/locale. This is
the same closure whether run against a fresh CI MSYS2 or a local one, so the CI
release artifact and a locally-staged copy are byte-for-byte equivalent.

Used by both `.github/workflows/build-gcc-trunk.yml` (--out a zip for release) and
local relocation (--stage straight into ~/.gcc-trunk/<tag>).
"""
import argparse
import shutil
import subprocess
import sys
from pathlib import Path

# pactree reports gcc-libs under its `cc-libs` provides name; map it back. lto-dump
# is a sibling package (not in gcc's runtime closure) we ship for -flto diagnostics.
PROVIDES_ALIAS = {"mingw-w64-x86_64-cc-libs": "mingw-w64-x86_64-gcc-libs"}
EXTRA_PACKAGES = ["mingw-w64-x86_64-gcc-lto-dump"]

# Trim non-functional bulk. Paths are relative to the mingw64 prefix root.
PRUNE_PREFIXES = ("share/man/", "share/doc/", "share/info/", "share/locale/", "share/gtk-doc/")


def bash_lc(msys_root: Path, script: str) -> str:
    bash = msys_root / "usr" / "bin" / "bash.exe"
    if not bash.exists():
        raise SystemExit(f"bash not found at {bash} (wrong --msys-root?)")
    res = subprocess.run(
        [str(bash), "-lc", script],
        capture_output=True, text=True,
        env={"MSYSTEM": "MINGW64", "CHERE_INVOKING": "1", "PATH": r"C:\Windows\System32;C:\Windows"},
    )
    if res.returncode != 0:
        raise SystemExit(f"bash failed: {script}\n{res.stderr}")
    return res.stdout


def resolve_closure(msys_root: Path) -> list[str]:
    out = bash_lc(msys_root, "pactree -u mingw-w64-x86_64-gcc")
    pkgs = []
    for line in out.splitlines():
        name = line.strip()
        if name.startswith("mingw-w64-x86_64-"):
            pkgs.append(PROVIDES_ALIAS.get(name, name))
    for extra in EXTRA_PACKAGES:
        if extra not in pkgs:
            pkgs.append(extra)
    return sorted(set(pkgs))


def package_files(msys_root: Path, pkgs: list[str]) -> set[str]:
    """Return mingw64-relative file paths owned by the given packages."""
    rel_files: set[str] = set()
    mingw_prefix = "/mingw64/"
    for pkg in pkgs:
        res = subprocess.run(
            [str(msys_root / "usr" / "bin" / "bash.exe"), "-lc", f"pacman -Qlq {pkg}"],
            capture_output=True, text=True,
            env={"MSYSTEM": "MINGW64", "PATH": r"C:\Windows\System32;C:\Windows"},
        )
        if res.returncode != 0:
            print(f"  warning: skipping {pkg} (not installed?)", file=sys.stderr)
            continue
        for path in res.stdout.splitlines():
            path = path.strip()
            if not path.startswith(mingw_prefix) or path.endswith("/"):
                continue  # foreign prefix or directory entry
            rel = path[len(mingw_prefix):]
            if rel.startswith(PRUNE_PREFIXES):
                continue
            rel_files.add(rel)
    return rel_files


def stage(msys_root: Path, rel_files: set[str], stage_dir: Path) -> int:
    src_root = msys_root / "mingw64"
    if stage_dir.exists():
        shutil.rmtree(stage_dir)
    copied = 0
    for rel in sorted(rel_files):
        src = src_root / rel
        if not src.is_file():
            continue
        dst = stage_dir / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
        copied += 1
    return copied


def main() -> None:
    repo_root = Path(__file__).resolve().parent.parent
    p = argparse.ArgumentParser(description="Stage a portable GCC toolchain from an MSYS2 mingw64 prefix")
    p.add_argument("--msys-root", type=Path, default=repo_root / "msys64",
                   help="MSYS2 install root containing usr/bin/bash.exe and mingw64/ (default: <repo>/msys64)")
    p.add_argument("--stage", type=Path, default=repo_root / "dist" / "gcc-trunk",
                   help="Directory to stage the toolchain into (its root becomes MINGW_ROOT)")
    p.add_argument("--out", type=Path, default=None,
                   help="Also produce this .zip of the staged tree (e.g. dist/gcc-trunk-windows-x64.zip)")
    args = p.parse_args()

    msys_root = args.msys_root.resolve()
    print(f"MSYS2 root: {msys_root}")
    pkgs = resolve_closure(msys_root)
    print(f"Toolchain closure ({len(pkgs)} packages): {', '.join(p.replace('mingw-w64-x86_64-', '') for p in pkgs)}")

    rel_files = package_files(msys_root, pkgs)
    print(f"Staging {len(rel_files)} files -> {args.stage}")
    copied = stage(msys_root, rel_files, args.stage)
    print(f"Staged {copied} files")

    gxx = args.stage / "bin" / "g++.exe"
    if not gxx.exists():
        raise SystemExit(f"ERROR: {gxx} missing after staging — closure resolution failed")

    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        if args.out.exists():
            args.out.unlink()
        print(f"Zipping -> {args.out}")
        shutil.make_archive(str(args.out.with_suffix("")), "zip", root_dir=args.stage)

    print(f"\nToolchain root: {args.stage}")
    print(f"  set MINGW_ROOT={args.stage}")


if __name__ == "__main__":
    main()
