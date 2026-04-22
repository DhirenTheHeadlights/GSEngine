import argparse
import os
import shutil
import subprocess
import sys
import tempfile
import urllib.request
import zipfile
from pathlib import Path

REPO_URL = "https://github.com/bloomberg/clang-p2996.git"
DEFAULT_BRANCH = "p2996"
NINJA_URL = "https://github.com/ninja-build/ninja/releases/latest/download/ninja-win.zip"

REPO_ROOT = Path(__file__).resolve().parent.parent


def run(cmd, cwd=None, env=None):
    display = " ".join(str(c) for c in cmd)
    print(f"$ {display}")
    result = subprocess.run(cmd, cwd=cwd, env=env)
    if result.returncode != 0:
        raise SystemExit(f"Command failed: {display}")


VS_SEARCH_ROOTS = [
    r"C:\Program Files\Microsoft Visual Studio\2022",
    r"C:\Program Files (x86)\Microsoft Visual Studio\2022",
    r"C:\Program Files\Microsoft Visual Studio\2019",
    r"C:\Program Files (x86)\Microsoft Visual Studio\2019",
]

VS_EDITIONS = ["Enterprise", "Professional", "Community", "BuildTools", "Preview"]


def find_vswhere():
    roots = [
        os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"),
        os.environ.get("ProgramFiles", r"C:\Program Files"),
    ]
    for root in roots:
        path = Path(root) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
        if path.exists():
            return path
    return None


def find_vs_install():
    candidates = []
    vswhere = find_vswhere()
    if vswhere:
        result = subprocess.run(
            [
                str(vswhere), "-all", "-prerelease", "-products", "*",
                "-property", "installationPath", "-format", "value",
            ],
            capture_output=True, text=True, check=False,
        )
        for line in result.stdout.splitlines():
            line = line.strip()
            if line:
                candidates.append(Path(line))
    for root in VS_SEARCH_ROOTS:
        root_path = Path(root)
        if not root_path.exists():
            continue
        for edition in VS_EDITIONS:
            ed_path = root_path / edition
            if ed_path.exists() and ed_path not in candidates:
                candidates.append(ed_path)
    for install in candidates:
        if (install / "VC" / "Auxiliary" / "Build" / "vcvars64.bat").exists():
            return install
    if candidates:
        print("Found VS installs but none had VC/Auxiliary/Build/vcvars64.bat:")
        for c in candidates:
            print(f"  {c}")
    return None


def load_vcvars_env(install_path):
    vcvars = install_path / "VC" / "Auxiliary" / "Build" / "vcvars64.bat"
    if not vcvars.exists():
        raise SystemExit(f"vcvars64.bat not found at {vcvars}")
    print(f"Loading MSVC environment from {vcvars}")
    marker = "__GSENGINE_VCVARS_MARKER__"
    cmd_line = f'call "{vcvars}" && echo {marker} && set'
    result = subprocess.run(
        cmd_line,
        shell=True,
        capture_output=True, text=True, errors="replace", check=False,
    )
    if result.returncode != 0 or marker not in result.stdout:
        print("--- vcvars64.bat stdout ---")
        print(result.stdout)
        print("--- vcvars64.bat stderr ---")
        print(result.stderr)
        raise SystemExit(f"vcvars64.bat failed (exit {result.returncode})")
    env = {}
    in_env = False
    for line in result.stdout.splitlines():
        if not in_env:
            if line.strip() == marker:
                in_env = True
            continue
        if "=" in line:
            k, _, v = line.partition("=")
            if k:
                env[k] = v
    if not env:
        raise SystemExit("vcvars64.bat succeeded but no environment captured")
    return env


def ensure_msvc_env():
    if sys.platform != "win32":
        return os.environ.copy()
    if "VCINSTALLDIR" in os.environ:
        return os.environ.copy()
    install = find_vs_install()
    if not install:
        raise SystemExit(
            "MSVC build tools not found. Install via:\n"
            '  winget install Microsoft.VisualStudio.2022.BuildTools --override '
            '"--quiet --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"\n'
            "Or install Visual Studio 2022 with the \"Desktop development with C++\" workload."
        )
    print(f"Found Visual Studio at {install}")
    return load_vcvars_env(install)


def ensure_ninja():
    existing = shutil.which("ninja")
    if existing:
        return Path(existing)
    local = REPO_ROOT / "External" / "ninja" / "ninja.exe"
    if local.exists():
        return local
    print(f"Ninja not found; downloading {NINJA_URL}")
    local.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        zip_path = Path(tmp) / "ninja.zip"
        with urllib.request.urlopen(NINJA_URL) as response, open(zip_path, "wb") as out:
            shutil.copyfileobj(response, out)
        with zipfile.ZipFile(zip_path) as z:
            z.extractall(local.parent)
    if not local.exists():
        raise SystemExit(f"Ninja extraction did not produce {local}")
    print(f"Ninja installed at {local}")
    return local


NEW_HANDLER_VCRUNTIME_MARKER = "UCRT's <new.h> doesn't declare std::get_new_handler"

NEW_HANDLER_OLD = """#if defined(_LIBCPP_ABI_VCRUNTIME)
#  include <new.h>
#else
_LIBCPP_BEGIN_UNVERSIONED_NAMESPACE_STD
typedef void (*new_handler)();
_LIBCPP_EXPORTED_FROM_ABI new_handler set_new_handler(new_handler) _NOEXCEPT;
_LIBCPP_EXPORTED_FROM_ABI new_handler get_new_handler() _NOEXCEPT;
_LIBCPP_END_UNVERSIONED_NAMESPACE_STD
#endif // _LIBCPP_ABI_VCRUNTIME
"""

NEW_HANDLER_NEW = """#if defined(_LIBCPP_ABI_VCRUNTIME)
#  include <new.h>
_LIBCPP_BEGIN_UNVERSIONED_NAMESPACE_STD
inline new_handler get_new_handler() _NOEXCEPT {
    new_handler __h = set_new_handler(nullptr);
    set_new_handler(__h);
    return __h;
}
_LIBCPP_END_UNVERSIONED_NAMESPACE_STD
#else
_LIBCPP_BEGIN_UNVERSIONED_NAMESPACE_STD
typedef void (*new_handler)();
_LIBCPP_EXPORTED_FROM_ABI new_handler set_new_handler(new_handler) _NOEXCEPT;
_LIBCPP_EXPORTED_FROM_ABI new_handler get_new_handler() _NOEXCEPT;
_LIBCPP_END_UNVERSIONED_NAMESPACE_STD
#endif // _LIBCPP_ABI_VCRUNTIME
"""


def patch_libcxx_get_new_handler(src):
    header = src / "libcxx" / "include" / "__new" / "new_handler.h"
    if not header.exists():
        return
    text = header.read_text(encoding="utf-8")
    if "inline new_handler get_new_handler()" in text:
        return
    if NEW_HANDLER_OLD not in text:
        print(f"WARNING: expected snippet not found in {header}; skipping patch")
        return
    header.write_text(text.replace(NEW_HANDLER_OLD, NEW_HANDLER_NEW), encoding="utf-8")
    print(f"Patched {header} to declare std::get_new_handler under vcruntime ABI")


def ensure_source(src, branch):
    if (src / "runtimes" / "CMakeLists.txt").exists():
        patch_libcxx_get_new_handler(src)
        return
    print(f"Source not found at {src}, cloning {REPO_URL} (branch {branch})...")
    src.parent.mkdir(parents=True, exist_ok=True)
    run(["git", "clone", "--branch", branch, "--depth", "1", REPO_URL, str(src)])
    patch_libcxx_get_new_handler(src)


def configure(src, build, dist, clang_cl, ninja, env):
    build.mkdir(parents=True, exist_ok=True)
    run([
        "cmake",
        "-S", str(src / "runtimes"),
        "-B", str(build),
        "-G", "Ninja",
        f"-DCMAKE_MAKE_PROGRAM={ninja}",
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
    ], env=env)


def build_targets(build, env):
    run([
        "cmake", "--build", str(build),
        "--target", "cxx", "generate-cxx-headers", "generate-cxx-modules",
    ], env=env)


def install(build, env):
    run([
        "cmake", "--build", str(build),
        "--target", "install-cxx", "install-cxx-headers", "install-cxx-modules",
    ], env=env)


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

    env = ensure_msvc_env()
    ninja = ensure_ninja()

    if not args.skip_configure:
        configure(args.src, args.build, dist, clang, ninja, env)
    if not args.skip_build:
        build_targets(args.build, env)
    if not args.skip_install:
        install(args.build, env)

    print()
    print(f"Installed libc++ into: {dist}")
    print("Headers:  include/c++/v1/")
    print("Libs:     lib/c++.lib, bin/c++.dll, lib/libc++.lib (etc.)")
    print("Modules:  share/libc++/v1/std.cppm")


if __name__ == "__main__":
    main()
