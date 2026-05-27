import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

sys.path.insert(0, str(REPO_ROOT / "scripts"))
from build_libcxx_p2996 import ensure_msvc_env, ensure_ninja, ensure_source  # noqa: E402


def detect_clang_version(clang_cl):
    result = subprocess.run([str(clang_cl), "--version"], capture_output=True, text=True, check=False)
    if result.returncode != 0:
        raise SystemExit(f"Failed to run {clang_cl} --version: {result.stderr}")
    m = re.search(r"clang version (\d+)", result.stdout)
    if not m:
        raise SystemExit(f"Could not parse clang version from: {result.stdout}")
    return m.group(1)


def run(cmd, cwd=None, env=None):
    display = " ".join(str(c) for c in cmd)
    print(f"$ {display}")
    result = subprocess.run(cmd, cwd=cwd, env=env)
    if result.returncode != 0:
        raise SystemExit(f"Command failed: {display}")


def configure(src, build, dist, clang_cl, ninja, env, clang_major):
    build.mkdir(parents=True, exist_ok=True)
    run([
        "cmake",
        "-S", str(src / "runtimes"),
        "-B", str(build),
        "-G", "Ninja",
        f"-DCMAKE_MAKE_PROGRAM={ninja}",
        f"-DCMAKE_C_COMPILER={clang_cl}",
        f"-DCMAKE_CXX_COMPILER={clang_cl}",
        "-DCMAKE_C_COMPILER_TARGET=x86_64-pc-windows-msvc",
        "-DCMAKE_CXX_COMPILER_TARGET=x86_64-pc-windows-msvc",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL",
        "-DLLVM_ENABLE_RUNTIMES=compiler-rt",
        "-DCOMPILER_RT_BUILD_BUILTINS=OFF",
        "-DCOMPILER_RT_BUILD_SANITIZERS=ON",
        "-DCOMPILER_RT_BUILD_LIBFUZZER=OFF",
        "-DCOMPILER_RT_BUILD_PROFILE=OFF",
        "-DCOMPILER_RT_BUILD_XRAY=OFF",
        "-DCOMPILER_RT_BUILD_MEMPROF=OFF",
        "-DCOMPILER_RT_BUILD_ORC=OFF",
        "-DCOMPILER_RT_BUILD_CRT=OFF",
        "-DCOMPILER_RT_BUILD_GWP_ASAN=OFF",
        "-DCOMPILER_RT_SANITIZERS_TO_BUILD=asan",
        "-DCOMPILER_RT_INCLUDE_TESTS=OFF",
        "-DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON",
        "-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=ON",
        f"-DCOMPILER_RT_INSTALL_PATH={dist}/lib/clang/{clang_major}",
        f"-DCMAKE_INSTALL_PREFIX={dist}",
    ], env=env)


def build_targets(build, env):
    run(["cmake", "--build", str(build)], env=env)


def install_targets(build, env):
    run(["cmake", "--install", str(build)], env=env)


def main():
    parser = argparse.ArgumentParser(
        description="Build compiler-rt (ASAN runtime) from clang-p2996 source and install into CLANG_P2996_ROOT",
    )
    parser.add_argument("--src", type=Path, default=REPO_ROOT / "External" / "clang-p2996",
                        help="clang-p2996 source tree (auto-cloned if missing)")
    parser.add_argument("--build", type=Path, default=REPO_ROOT / "build" / "compiler-rt-p2996")
    parser.add_argument("--dist", type=Path, default=None,
                        help="Install prefix (default: $CLANG_P2996_ROOT)")
    parser.add_argument("--clang", type=Path, default=None,
                        help="Path to clang-cl.exe (default: $CLANG_P2996_ROOT/bin/clang-cl.exe)")
    parser.add_argument("--branch", default="p2996", help="git branch when cloning source")
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

    env = ensure_msvc_env()
    ninja = ensure_ninja()
    clang_major = detect_clang_version(clang)

    if not args.skip_configure:
        configure(args.src, args.build, dist, clang, ninja, env, clang_major)
    if not args.skip_build:
        build_targets(args.build, env)
    if not args.skip_install:
        install_targets(args.build, env)

    print()
    print(f"Installed compiler-rt runtime into: {dist}/lib/clang/{clang_major}/lib/x86_64-pc-windows-msvc/")


if __name__ == "__main__":
    main()
