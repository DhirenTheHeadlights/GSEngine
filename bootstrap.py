import argparse
import os
import shutil
import stat
import subprocess
import sys
import tempfile
import urllib.request
import zipfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent

NINJA_VERSION = "v1.13.2"
NINJA_URL = f"https://github.com/ninja-build/ninja/releases/download/{NINJA_VERSION}/ninja-win.zip"
NINJA_DIR = Path.home() / ".gcc-trunk" / "ninja"
NINJA_WINGET_ID = "Ninja-build.Ninja"

GCC_BIN = Path.home() / ".gcc-trunk" / "current" / "bin"
DEFAULT_PRESET = "x64-mingw-gcc-Release"
EDITOR_TARGET = "Editor"
STALE_CACHE_KEYS = ("CMAKE_MAKE_PROGRAM:FILEPATH", "CMAKE_C_COMPILER:FILEPATH", "CMAKE_CXX_COMPILER:FILEPATH")

CPPREF_VERSION = "20250209"
CPPREF_URL = f"https://github.com/PeterFeicht/cppreference-doc/releases/download/v{CPPREF_VERSION}/cppreference-doc-{CPPREF_VERSION}.tar.xz"
CPPREF_INDEX = REPO_ROOT / "Editor" / "Resources" / "cppref.idx"
CPPREF_BUILDER = REPO_ROOT / "Editor" / "Tools" / "build_cppref_index.py"


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


def download_ninja(ninja_exe: Path) -> bool:
    print(f"Downloading Ninja {NINJA_VERSION} from {NINJA_URL}")
    try:
        with tempfile.TemporaryDirectory() as tmp:
            zip_path = Path(tmp) / "ninja-win.zip"
            with urllib.request.urlopen(NINJA_URL) as response, open(zip_path, "wb") as out:
                shutil.copyfileobj(response, out)
            with zipfile.ZipFile(zip_path) as archive:
                archive.extract("ninja.exe", NINJA_DIR)
    except Exception as exc:
        print(f"Direct Ninja download failed: {exc}")
        return False
    return ninja_exe.exists()


def winget_install_ninja() -> None:
    winget = shutil.which("winget")
    if winget is None:
        print("winget is not available on this machine")
        return
    print(f"Installing Ninja via winget ({NINJA_WINGET_ID})")
    subprocess.run([
        winget, "install", "--id", NINJA_WINGET_ID, "--exact", "--source", "winget",
        "--accept-package-agreements", "--accept-source-agreements", "--disable-interactivity",
    ])


def locate_ninja() -> Path | None:
    local_app_data = os.environ.get("LOCALAPPDATA")
    if local_app_data:
        packages = Path(local_app_data) / "Microsoft" / "WinGet" / "Packages"
        for candidate in sorted(packages.glob(f"{NINJA_WINGET_ID}_*/ninja.exe")):
            return candidate
    on_path = shutil.which("ninja")
    if on_path:
        return Path(on_path)
    return None


def ensure_ninja(force=False):
    ninja_exe = NINJA_DIR / "ninja.exe"
    if ninja_exe.exists() and not force:
        print(f"Ninja already installed: {ninja_exe}")
        return
    NINJA_DIR.mkdir(parents=True, exist_ok=True)
    if download_ninja(ninja_exe):
        print(f"Installed Ninja: {ninja_exe}")
        return
    winget_install_ninja()
    found = locate_ninja()
    if found is None:
        raise SystemExit(f"Could not install Ninja: the direct download failed and winget produced no ninja.exe. Place one at {ninja_exe} manually.")
    shutil.copy2(found, ninja_exe)
    if not ninja_exe.exists():
        raise SystemExit(f"Ninja install did not produce {ninja_exe}")
    print(f"Installed Ninja from {found}: {ninja_exe}")


def ensure_cppref_index(force=False):
    if CPPREF_INDEX.exists() and not force:
        print(f"cppreference hover index already present: {CPPREF_INDEX}")
        return
    print(f"Downloading cppreference package {CPPREF_VERSION} from {CPPREF_URL}")
    try:
        with tempfile.TemporaryDirectory() as tmp:
            tarball = Path(tmp) / "cppreference-doc.tar.xz"
            with urllib.request.urlopen(CPPREF_URL) as response, open(tarball, "wb") as out:
                shutil.copyfileobj(response, out)
            result = subprocess.run([sys.executable, str(CPPREF_BUILDER), "--tarball", str(tarball), "--out", str(CPPREF_INDEX)])
    except Exception as exc:
        print(f"WARNING: could not build cppreference hover index ({exc}); the editor will fall back to header doc-comments")
        return
    if result.returncode != 0 or not CPPREF_INDEX.exists():
        print("WARNING: cppreference hover index build failed; the editor will fall back to header doc-comments")
        return
    print(f"Built cppreference hover index: {CPPREF_INDEX}")


def stale_cache_reason(build_dir: Path) -> str | None:
    cache = build_dir / "CMakeCache.txt"
    if not cache.exists():
        return None
    for line in cache.read_text(errors="replace").splitlines():
        for key in STALE_CACHE_KEYS:
            prefix = key + "="
            if line.startswith(prefix):
                value = line[len(prefix):].strip()
                if value and not Path(value).exists():
                    return f"{key.split(':')[0]} is cached as {value}, which does not exist on this machine"
    return None


def _clear_readonly(func, path, _exc) -> None:
    os.chmod(path, stat.S_IWRITE)
    func(path)


def toolchain_env() -> dict:
    env = dict(os.environ)
    env["PATH"] = os.pathsep.join([str(GCC_BIN), str(NINJA_DIR), env.get("PATH", "")])
    return env


def build_editor(preset: str) -> None:
    cmake = shutil.which("cmake")
    if cmake is None:
        raise SystemExit("cmake was not found on PATH. Install CMake 3.28 or newer, then re-open the shell and re-run this script.")
    compiler = GCC_BIN / "g++.exe"
    if not compiler.exists():
        raise SystemExit(f"The gcc-trunk toolchain is missing: {compiler} does not exist. Re-run this script without --skip-gcc.")
    vulkan_sdk = os.environ.get("VULKAN_SDK")
    if not vulkan_sdk or not Path(vulkan_sdk).is_dir():
        raise SystemExit("VULKAN_SDK is not set to an existing directory. Install the Vulkan SDK 1.4 or newer from https://vulkan.lunarg.com/sdk/home, then re-open the shell and re-run this script.")
    print(f"Vulkan SDK: {vulkan_sdk}")

    build_dir = REPO_ROOT / "out" / "build" / preset
    reason = stale_cache_reason(build_dir)
    if reason is not None:
        print(f"Removing unusable build directory {build_dir}")
        print(f"  reason: {reason}")
        shutil.rmtree(build_dir, onexc=_clear_readonly)

    env = toolchain_env()
    print("\nConfiguring. The first run installs the vcpkg dependency tree and takes 30-60 minutes.")
    run([cmake, "--preset", preset], cwd=REPO_ROOT, env=env)
    print(f"\nBuilding target {EDITOR_TARGET}.")
    run([cmake, "--build", "--preset", preset, "--target", EDITOR_TARGET], cwd=REPO_ROOT, env=env)

    editor_exe = build_dir / "Editor" / f"{EDITOR_TARGET}.exe"
    if not editor_exe.exists():
        raise SystemExit(f"The build reported success but {editor_exe} is missing.")
    print(f"\nEditor built: {editor_exe}")


def main():
    parser = argparse.ArgumentParser(description="Full environment bootstrap: submodules, native-Windows GCC trunk toolchain, and an editor build")
    parser.add_argument("--skip-submodules", action="store_true", help="Skip git submodule init/update")
    parser.add_argument("--skip-gcc", action="store_true", help="Skip GCC trunk toolchain install")
    parser.add_argument("--skip-ninja", action="store_true", help="Skip Ninja install")
    parser.add_argument("--skip-cppref", action="store_true", help="Skip cppreference hover-index build")
    parser.add_argument("--skip-build", action="store_true", help="Skip the CMake configure and editor build")
    parser.add_argument("--preset", default=DEFAULT_PRESET, help=f"CMake preset to configure and build (default: {DEFAULT_PRESET})")
    parser.add_argument("--update-vcpkg", action="store_true", help="Pull latest vcpkg master after submodule init")
    parser.add_argument("--tag", default=None, help="gcc-trunk release tag (default: latest gcc-trunk-v* release)")
    parser.add_argument("--sha256", default=None, help="Expected SHA256 of the toolchain zip")
    parser.add_argument("--persist", action="store_true", help="Persist MINGW_ROOT via setx")
    parser.add_argument("--force", action="store_true", help="Reinstall the toolchain even if already present")
    args = parser.parse_args()

    if not args.skip_submodules:
        update_submodules()
        if args.update_vcpkg:
            update_vcpkg()

    if not args.skip_gcc:
        gcc_args = []
        if args.tag:
            gcc_args += ["--tag", args.tag]
        if args.sha256:
            gcc_args += ["--sha256", args.sha256]
        if args.persist:
            gcc_args.append("--persist")
        if args.force:
            gcc_args.append("--force")
        run([sys.executable, str(REPO_ROOT / "scripts" / "install_gcc_trunk.py"), *gcc_args])

    if not args.skip_ninja:
        ensure_ninja(force=args.force)

    if not args.skip_cppref:
        ensure_cppref_index(force=args.force)

    if args.skip_build:
        print("\nBootstrap complete. CMake configure will auto-install vcpkg deps from vcpkg.json.")
        print(f"Configure with: cmake --preset {args.preset}")
        return

    build_editor(args.preset)
    print("\nBootstrap complete.")


if __name__ == "__main__":
    main()
