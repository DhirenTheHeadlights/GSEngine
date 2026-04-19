import argparse
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent


def run_script(script, args):
    result = subprocess.run([sys.executable, str(script), *args])
    if result.returncode != 0:
        raise SystemExit(f"{script.name} failed ({result.returncode})")


def main():
    parser = argparse.ArgumentParser(description="Full environment bootstrap: vcpkg deps + clang-p2996")
    parser.add_argument("--skip-deps", action="store_true", help="Skip setup.py")
    parser.add_argument("--skip-clang", action="store_true", help="Skip clang-p2996 install")
    parser.add_argument("--update-vcpkg", action="store_true", help="Forwarded to setup.py")
    parser.add_argument("--clang-tag", default=None, help="Forwarded to install_clang_p2996.py")
    parser.add_argument("--persist", action="store_true", help="Persist CLANG_P2996_ROOT via setx")
    args = parser.parse_args()

    if not args.skip_deps:
        deps_args = ["--update-vcpkg"] if args.update_vcpkg else []
        run_script(REPO_ROOT / "setup.py", deps_args)

    if not args.skip_clang:
        clang_args = []
        if args.clang_tag:
            clang_args.extend(["--tag", args.clang_tag])
        if args.persist:
            clang_args.append("--persist")
        run_script(REPO_ROOT / "scripts" / "install_clang_p2996.py", clang_args)

    print("\nBootstrap complete.")


if __name__ == "__main__":
    main()
