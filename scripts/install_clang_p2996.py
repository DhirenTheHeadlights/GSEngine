import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import urllib.request
import zipfile
from pathlib import Path

RELEASE_URL = "https://github.com/DhirenTheHeadlights/GSEngine/releases/download/{tag}/clang-p2996-windows-x64.zip"
LATEST_RELEASE_URL = "https://api.github.com/repos/DhirenTheHeadlights/GSEngine/releases/latest"
ENV_VAR = "CLANG_P2996_ROOT"


def resolve_latest_tag() -> str:
    print(f"Resolving latest clang-p2996 release tag from {LATEST_RELEASE_URL}")
    req = urllib.request.Request(LATEST_RELEASE_URL, headers={"Accept": "application/vnd.github+json"})
    with urllib.request.urlopen(req) as response:
        data = json.load(response)
    tag = data["tag_name"]
    print(f"Latest release: {tag}")
    return tag

# Install under the user home directory (not AppData) so Windows Store Python's
# VFS sandbox redirection of %LOCALAPPDATA% doesn't cause cmake to see a different
# filesystem view than the Python process does.
INSTALL_ROOT = Path.home() / ".clang-p2996"


def install_path(tag: str) -> Path:
    return INSTALL_ROOT / tag


def already_installed(tag: str) -> bool:
    return (install_path(tag) / "bin" / "clang-cl.exe").exists()


def download(url: str, dest: Path) -> None:
    print(f"Downloading {url}")
    with urllib.request.urlopen(url) as response, open(dest, "wb") as out:
        shutil.copyfileobj(response, out)


def verify_sha256(path: Path, expected: str) -> None:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    actual = h.hexdigest()
    if actual.lower() != expected.lower():
        raise SystemExit(f"SHA256 mismatch: got {actual}, expected {expected}")


def extract(zip_path: Path, dest: Path) -> None:
    if dest.exists():
        shutil.rmtree(dest)
    dest.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(zip_path) as z:
        z.extractall(dest)


def persist_env_var(name: str, value: str) -> None:
    if os.name != "nt":
        print(f"Add to your shell rc: export {name}={value}")
        return
    subprocess.run(["setx", name, str(value)], check=True)
    print(f"Persisted {name} (effective in new shells)")


def main() -> None:
    parser = argparse.ArgumentParser(description="Download and install prebuilt clang-p2996 toolchain")
    parser.add_argument("--tag", default=None, help="Release tag to install (default: latest GitHub release)")
    parser.add_argument("--sha256", help="Expected SHA256 of the zip")
    parser.add_argument("--persist", action="store_true", help=f"Persist {ENV_VAR} via setx")
    parser.add_argument("--force", action="store_true", help="Reinstall even if already present")
    parser.add_argument("--url", help="Override the release URL entirely")
    args = parser.parse_args()

    if args.tag is None:
        args.tag = resolve_latest_tag()

    target = install_path(args.tag)
    if already_installed(args.tag) and not args.force:
        print(f"Already installed: {target}")
    else:
        url = args.url or RELEASE_URL.format(tag=args.tag)
        with tempfile.TemporaryDirectory() as tmp:
            zip_path = Path(tmp) / "toolchain.zip"
            download(url, zip_path)
            if args.sha256:
                verify_sha256(zip_path, args.sha256)
            extract(zip_path, target)
        print(f"Installed: {target}")

    if args.persist:
        persist_env_var(ENV_VAR, str(target))
    else:
        print(f"\nTo use this toolchain:")
        print(f"  set {ENV_VAR}={target}    (cmd)")
        print(f"  $env:{ENV_VAR} = '{target}'  (PowerShell)")
        print(f"Or re-run with --persist to set it permanently on Windows.")


if __name__ == "__main__":
    main()
