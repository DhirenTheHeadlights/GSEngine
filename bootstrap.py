import argparse
import json
import os
import subprocess
import sys
import urllib.request
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent
LATEST_RELEASE_URL = "https://api.github.com/repos/DhirenTheHeadlights/GSEngine/releases/latest"
# Use home dir (not AppData) so Windows Store Python's VFS sandbox redirection
# of %LOCALAPPDATA% doesn't cause cmake to disagree with Python on what exists.
CLANG_INSTALL_ROOT = Path.home() / ".clang-p2996"


def resolve_latest_clang_tag():
    print(f"Resolving latest clang-p2996 release tag from {LATEST_RELEASE_URL}")
    req = urllib.request.Request(LATEST_RELEASE_URL, headers={"Accept": "application/vnd.github+json"})
    with urllib.request.urlopen(req) as response:
        data = json.load(response)
    tag = data["tag_name"]
    print(f"Latest release: {tag}")
    return tag


def resolve_clang_root(tag):
    # Prefer the canonical home-dir install location first — it is never VFS-redirected.
    canonical = CLANG_INSTALL_ROOT / tag
    if (canonical / "bin" / "clang-cl.exe").exists():
        return canonical
    # Fall back to CLANG_P2996_ROOT if set and valid.
    env_root = os.environ.get("CLANG_P2996_ROOT")
    if env_root:
        candidate = Path(env_root)
        if (candidate / "bin" / "clang-cl.exe").exists():
            return candidate
        print(f"warning: CLANG_P2996_ROOT={candidate} has no bin/clang-cl.exe, falling back to default")
    return canonical


def run(cmd, cwd=None, env=None):
    display = " ".join(str(c) for c in cmd)
    print(f"$ {display}")
    result = subprocess.run(cmd, cwd=cwd, env=env)
    if result.returncode != 0:
        raise SystemExit(f"Command failed ({result.returncode}): {display}")


def update_submodules():
    if (REPO_ROOT / ".gitmodules").exists():
        run(["git", "submodule", "update", "--init", "--recursive"], cwd=REPO_ROOT)


def update_vcpkg():
    run(["git", "pull", "origin", "master"], cwd=REPO_ROOT / "Engine" / "External" / "vcpkg")


def libcxx_already_built(clang_root):
    return (clang_root / "lib" / "c++.lib").exists() and (clang_root / "share" / "libc++" / "v1" / "std.cppm").exists()


def compiler_rt_already_built(clang_root):
    lib_dir = clang_root / "lib" / "clang"
    if not lib_dir.exists():
        return False
    for ver_dir in lib_dir.iterdir():
        if (ver_dir / "lib" / "x86_64-pc-windows-msvc" / "clang_rt.asan_dynamic.lib").exists():
            return True
    return False


def main():
    parser = argparse.ArgumentParser(description="Full environment bootstrap: submodules + clang-p2996 + libc++ + compiler-rt")
    parser.add_argument("--skip-submodules", action="store_true", help="Skip git submodule init/update")
    parser.add_argument("--skip-clang", action="store_true", help="Skip clang-p2996 install")
    parser.add_argument("--skip-libcxx", action="store_true", help="Skip libc++ build")
    parser.add_argument("--skip-compiler-rt", action="store_true", help="Skip compiler-rt (ASAN runtime) build")
    parser.add_argument("--update-vcpkg", action="store_true", help="Pull latest vcpkg master after submodule init")
    parser.add_argument("--clang-tag", default=None, help="clang-p2996 release tag (default: latest GitHub release)")
    parser.add_argument("--persist", action="store_true", help="Persist CLANG_P2996_ROOT via setx")
    parser.add_argument("--force-libcxx", action="store_true", help="Rebuild libc++ even if already installed")
    parser.add_argument("--force-compiler-rt", action="store_true", help="Rebuild compiler-rt even if already installed")
    args = parser.parse_args()

    if not args.skip_submodules:
        update_submodules()
        if args.update_vcpkg:
            update_vcpkg()

    if args.clang_tag is None:
        args.clang_tag = resolve_latest_clang_tag()

    clang_root = resolve_clang_root(args.clang_tag)

    if not args.skip_clang:
        clang_args = ["--tag", args.clang_tag]
        if args.persist:
            clang_args.append("--persist")
        run([sys.executable, str(REPO_ROOT / "scripts" / "install_clang_p2996.py"), *clang_args])

    if not args.skip_libcxx:
        if libcxx_already_built(clang_root) and not args.force_libcxx:
            print(f"libc++ already built at {clang_root} (use --force-libcxx to rebuild)")
        else:
            env = {**os.environ, "CLANG_P2996_ROOT": str(clang_root)}
            run([sys.executable, str(REPO_ROOT / "scripts" / "build_libcxx_p2996.py")], env=env)

    if not args.skip_compiler_rt:
        if compiler_rt_already_built(clang_root) and not args.force_compiler_rt:
            print(f"compiler-rt already built at {clang_root} (use --force-compiler-rt to rebuild)")
        else:
            env = {**os.environ, "CLANG_P2996_ROOT": str(clang_root)}
            run([sys.executable, str(REPO_ROOT / "scripts" / "build_compiler_rt_p2996.py")], env=env)

    print("\nBootstrap complete. CMake configure will auto-install vcpkg deps from vcpkg.json.")


if __name__ == "__main__":
    main()
