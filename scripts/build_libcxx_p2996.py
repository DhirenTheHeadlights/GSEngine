import argparse
import os
import shutil
import subprocess
from pathlib import Path

REPO_URL = "https://github.com/bloomberg/clang-p2996.git"
DEFAULT_BRANCH = "p2996"


def run(cmd, cwd=None):
    display = " ".join(str(c) for c in cmd)
    print(f"$ {display}")
    result = subprocess.run(cmd, cwd=cwd)
    if result.returncode != 0:
        raise SystemExit(f"Command failed: {display}")


def ensure_source(src, branch):
    if (src / "runtimes" / "CMakeLists.txt").exists():
        return
    print(f"Source not found at {src}, cloning {REPO_URL} (branch {branch})...")
    src.parent.mkdir(parents=True, exist_ok=True)
    run(["git", "clone", "--branch", branch, "--depth", "1", REPO_URL, str(src)])


def configure(src, build, dist, clang_cl):
    build.mkdir(parents=True, exist_ok=True)
    run([
        "cmake",
        "-S", str(src / "runtimes"),
        "-B", str(build),
        "-G", "Ninja",
        f"-DCMAKE_C_COMPILER={clang_cl}",
        f"-DCMAKE_CXX_COMPILER={clang_cl}",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL",
        "-DLLVM_ENABLE_RUNTIMES=libcxx",
        "-DLIBCXX_CXX_ABI=vcruntime",
        "-DLIBCXX_ENABLE_SHARED=ON",
        "-DLIBCXX_ENABLE_STATIC=ON",
        "-DLIBCXX_INSTALL_MODULES=ON",
        f"-DCMAKE_INSTALL_PREFIX={dist}",
    ])


def build_targets(build):
    run([
        "cmake", "--build", str(build),
        "--target", "cxx", "generate-cxx-headers", "generate-cxx-modules",
    ])


def install(build):
    run([
        "cmake", "--build", str(build),
        "--target", "install-cxx", "install-cxx-headers", "install-cxx-modules",
    ])


def main():
    parser = argparse.ArgumentParser(
        description="Build libc++ from clang-p2996 source targeting MSVC ABI and install alongside clang-p2996",
    )
    parser.add_argument("--src", type=Path, default=Path("External") / "clang-p2996",
                        help="clang-p2996 source tree (auto-cloned if missing)")
    parser.add_argument("--build", type=Path, default=Path("build") / "libcxx-p2996")
    parser.add_argument("--dist", type=Path, default=None,
                        help="Install prefix (default: derived from --clang as <clang-root>)")
    parser.add_argument("--clang", type=Path, default=None,
                        help="Path to clang-cl.exe (default: $CLANG_P2996_ROOT/bin/clang-cl.exe)")
    parser.add_argument("--branch", default=DEFAULT_BRANCH, help="git branch when cloning source")
    parser.add_argument("--clean", action="store_true", help="Wipe the build dir before configuring")
    parser.add_argument("--skip-configure", action="store_true")
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument("--skip-install", action="store_true")
    args = parser.parse_args()

    clang = args.clang
    if clang is None:
        root = os.environ.get("CLANG_P2996_ROOT")
        if not root:
            raise SystemExit("CLANG_P2996_ROOT not set and --clang not provided")
        clang = Path(root) / "bin" / "clang-cl.exe"
    if not clang.exists():
        raise SystemExit(f"clang-cl not found at {clang}")

    dist = args.dist if args.dist is not None else clang.parent.parent

    ensure_source(args.src, args.branch)

    if args.clean and args.build.exists():
        shutil.rmtree(args.build)

    if not args.skip_configure:
        configure(args.src, args.build, dist, clang)
    if not args.skip_build:
        build_targets(args.build)
    if not args.skip_install:
        install(args.build)

    print()
    print(f"Installed libc++ into: {dist}")
    print("Headers:  include/c++/v1/")
    print("Libs:     lib/c++.lib, bin/c++.dll, lib/libc++.lib (etc.)")
    print("Modules:  share/libc++/v1/std.cppm")


if __name__ == "__main__":
    main()
