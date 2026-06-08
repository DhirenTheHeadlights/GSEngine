import argparse
import shutil
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


def ensure_ninja(force=False):
    ninja_exe = NINJA_DIR / "ninja.exe"
    if ninja_exe.exists() and not force:
        print(f"Ninja already installed: {ninja_exe}")
        return
    NINJA_DIR.mkdir(parents=True, exist_ok=True)
    print(f"Downloading Ninja {NINJA_VERSION} from {NINJA_URL}")
    try:
        with tempfile.TemporaryDirectory() as tmp:
            zip_path = Path(tmp) / "ninja-win.zip"
            with urllib.request.urlopen(NINJA_URL) as response, open(zip_path, "wb") as out:
                shutil.copyfileobj(response, out)
            with zipfile.ZipFile(zip_path) as archive:
                archive.extract("ninja.exe", NINJA_DIR)
    except Exception as exc:
        raise SystemExit(f"Failed to install Ninja: {exc}")
    if not ninja_exe.exists():
        raise SystemExit(f"Ninja install did not produce {ninja_exe}")
    print(f"Installed Ninja: {ninja_exe}")


def main():
    parser = argparse.ArgumentParser(description="Full environment bootstrap: submodules + native-Windows GCC trunk toolchain")
    parser.add_argument("--skip-submodules", action="store_true", help="Skip git submodule init/update")
    parser.add_argument("--skip-gcc", action="store_true", help="Skip GCC trunk toolchain install")
    parser.add_argument("--skip-ninja", action="store_true", help="Skip Ninja install")
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

    print("\nBootstrap complete. CMake configure will auto-install vcpkg deps from vcpkg.json.")
    print("Configure with: cmake --preset x64-mingw-gcc-Release")


if __name__ == "__main__":
    main()
