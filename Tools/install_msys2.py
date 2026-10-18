"""Install MSYS2 into <repo>/msys64 with the GCC build dependencies.

Local equivalent of the msys2/setup-msys2 step in .github/workflows/build-gcc-trunk.yml,
so build_gcc_trunk.py and build_gcc_wasm.py have the same package closure locally as in
CI. The package list is read out of that workflow rather than duplicated here.

Uses the self-extracting base archive, not the Qt installer .exe: it is a plain extract
with no registry keys, no shortcuts and no unattended-install incantation, and <repo>/msys64
is already gitignored.

    python Tools/install_msys2.py
"""
import argparse
import re
import subprocess
import urllib.request
from pathlib import Path

from msys2 import DEFAULT_MSYS_ROOT, REPO_ROOT, bash

SFX_URL = ("https://github.com/msys2/msys2-installer/releases/download/"
           "nightly-x86_64/msys2-base-x86_64-latest.sfx.exe")

WORKFLOW = REPO_ROOT / ".github" / "workflows" / "build-gcc-trunk.yml"


def workflow_packages(workflow: Path) -> list:
    """Read the `install:` block out of the CI workflow's setup-msys2 step.

    Keeping CI as the single source of truth means a dependency added there cannot
    silently fail to apply locally, which is the failure mode that produced a
    half-built toolchain before.
    """
    text = workflow.read_text(encoding="utf-8")
    m = re.search(r"^(\s+)install:\s*>-\s*\n((?:\1\s+\S.*\n)+)", text, re.M)
    if not m:
        raise SystemExit(f"could not find an `install: >-` block in {workflow}")
    pkgs = m.group(2).split()
    if not pkgs:
        raise SystemExit(f"`install:` block in {workflow} is empty")
    return pkgs


def download(url: str, dest: Path) -> Path:
    if dest.exists():
        print(f"Using cached {dest}")
        return dest
    dest.parent.mkdir(parents=True, exist_ok=True)
    print(f"Downloading {url}")
    with urllib.request.urlopen(url) as r, open(dest, "wb") as f:
        f.write(r.read())
    print(f"  -> {dest} ({dest.stat().st_size} bytes)")
    return dest


def main() -> None:
    p = argparse.ArgumentParser(description="Install MSYS2 + GCC build deps into <repo>/msys64")
    p.add_argument("--msys-root", type=Path, default=DEFAULT_MSYS_ROOT)
    p.add_argument("--url", default=SFX_URL, help="MSYS2 self-extracting base archive")
    p.add_argument("--cache", type=Path, default=REPO_ROOT / ".gcc-dl", help="Download cache dir")
    p.add_argument("--force", action="store_true", help="Re-extract even if the root exists")
    args = p.parse_args()

    root = args.msys_root.resolve()
    if root.exists() and not args.force:
        print(f"MSYS2 already present at {root}; updating packages only (--force to re-extract)")
    else:
        sfx = download(args.url, args.cache / Path(args.url).name)
        # The archive contains a top-level msys64/, so extract into the parent.
        print(f"Extracting to {root.parent}")
        subprocess.run([str(sfx), "-y", f"-o{root.parent}"], check=True)
        if not (root / "usr" / "bin" / "bash.exe").exists():
            raise SystemExit(f"extraction did not produce {root}/usr/bin/bash.exe")
        # First launch generates /etc/passwd and the user's home; it exits non-zero on
        # some versions while still having done the setup, so do not check it.
        bash(root, "true", check=False, msystem="MSYS")

    # pacman's own packages can be part of the update, and upgrading them mid-transaction
    # terminates the shell. The documented sequence is to run it until it settles.
    for attempt in range(3):
        print(f"\n== pacman -Syu (pass {attempt + 1}) ==")
        res = bash(root, "pacman -Syuu --noconfirm --needed", check=False, msystem="MSYS")
        if res.returncode == 0:
            break
    else:
        raise SystemExit("pacman -Syu did not converge after 3 passes")

    pkgs = workflow_packages(WORKFLOW)
    print(f"\n== installing {len(pkgs)} packages from {WORKFLOW.name} ==")
    bash(root, f"pacman -S --noconfirm --needed {' '.join(pkgs)}", msystem="MSYS")

    bash(root, "gcc --version | head -1 && make --version | head -1")
    print(f"\nMSYS2 ready at {root}")


if __name__ == "__main__":
    main()
