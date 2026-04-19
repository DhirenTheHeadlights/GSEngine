import argparse
import os
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent
VCPKG_DIR = REPO_ROOT / "Engine" / "External" / "vcpkg"

DEPENDENCIES = [
    "msdfgen[core,extensions]:x64-windows",
    "freetype[core]:x64-windows",
    "glfw3:x64-windows",
    "stb:x64-windows",
    "miniaudio:x64-windows",
    "shader-slang:x64-windows",
    "tbb:x64-windows",
]


def run(cmd, cwd=None, capture=False):
    display = " ".join(str(c) for c in cmd)
    print(f"$ {display}")
    result = subprocess.run(
        cmd,
        cwd=cwd,
        stdout=subprocess.PIPE if capture else None,
        stderr=subprocess.STDOUT if capture else None,
        text=True,
    )
    if result.returncode != 0:
        if capture and result.stdout:
            print(result.stdout)
        raise SystemExit(f"Command failed ({result.returncode}): {display}")
    return result.stdout if capture else None


def vcpkg_exe():
    return VCPKG_DIR / ("vcpkg.exe" if os.name == "nt" else "vcpkg")


def update_submodules():
    if (REPO_ROOT / ".gitmodules").exists():
        run(["git", "submodule", "update", "--init", "--recursive"], cwd=REPO_ROOT)


def bootstrap_vcpkg(update_vcpkg):
    if update_vcpkg:
        run(["git", "pull", "origin", "master"], cwd=VCPKG_DIR)
    bootstrap = VCPKG_DIR / ("bootstrap-vcpkg.bat" if os.name == "nt" else "bootstrap-vcpkg.sh")
    run([str(bootstrap)], cwd=VCPKG_DIR)


def install_deps():
    run(
        [str(vcpkg_exe()), "install", "--recurse", "--triplet", "x64-windows", *DEPENDENCIES],
        cwd=VCPKG_DIR,
    )


def verify_deps():
    listed = run([str(vcpkg_exe()), "list"], cwd=VCPKG_DIR, capture=True) or ""
    installed = {line.split(":")[0] for line in listed.splitlines() if line}
    simple = [d.split("[")[0].split(":")[0] for d in DEPENDENCIES]
    missing = [d for d in simple if d not in installed]
    if missing:
        raise SystemExit("Missing dependencies: " + ", ".join(missing))
    print("All dependencies installed.")


def main():
    parser = argparse.ArgumentParser(description="vcpkg bootstrap + engine dependencies")
    parser.add_argument(
        "--update-vcpkg",
        action="store_true",
        help="Pull latest vcpkg before bootstrapping (default: use the submodule-pinned SHA)",
    )
    args = parser.parse_args()

    update_submodules()
    bootstrap_vcpkg(args.update_vcpkg)
    install_deps()
    verify_deps()


if __name__ == "__main__":
    main()
