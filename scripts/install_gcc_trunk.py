import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import tempfile
import urllib.request
import zipfile
from pathlib import Path

REPO = "DhirenTheHeadlights/GSEngine"
RELEASE_URL = f"https://github.com/{REPO}/releases/download/{{tag}}/gcc-trunk-windows-x64.zip"
RELEASES_API = f"https://api.github.com/repos/{REPO}/releases?per_page=100"
ENV_VAR = "MINGW_ROOT"
TAG_RE = re.compile(r"^gcc-trunk-v(\d+)$")

# Install under the user home directory (not AppData) so Windows Store Python's
# VFS sandbox redirection of %LOCALAPPDATA% doesn't cause cmake to see a different
# filesystem view than the Python process does.
INSTALL_ROOT = Path.home() / ".gcc-trunk"


def resolve_latest_tag() -> str:
    # Filter to gcc-trunk-v* and pick the highest N. Do NOT use /releases/latest:
    # this repo also publishes clang-p2996-* releases, and "latest" is across all
    # release kinds, so it could hand back a Clang tag.
    print(f"Resolving latest gcc-trunk release tag from {RELEASES_API}")
    req = urllib.request.Request(RELEASES_API, headers={"Accept": "application/vnd.github+json"})
    with urllib.request.urlopen(req) as response:
        data = json.load(response)
    best = None
    for rel in data:
        m = TAG_RE.match(rel.get("tag_name", ""))
        if m and not rel.get("draft", False):
            n = int(m.group(1))
            if best is None or n > best[0]:
                best = (n, rel["tag_name"])
    if best is None:
        raise SystemExit("No gcc-trunk-v* release found. Publish one or pass --tag.")
    print(f"Latest release: {best[1]}")
    return best[1]


def install_path(tag: str) -> Path:
    return INSTALL_ROOT / tag


def already_installed(tag: str) -> bool:
    return (install_path(tag) / "bin" / "g++.exe").exists()


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


def link_current(tag: str) -> Path:
    # Maintain a stable, version-independent path (~/.gcc-trunk/current) that the
    # CMake preset points at, so a toolchain version bump never requires editing
    # the preset or any environment variable. Re-pointed to the freshly installed
    # tag on every run.
    link = INSTALL_ROOT / "current"
    target = install_path(tag)
    if os.name == "nt":
        # rmdir removes a junction (or empty dir) without touching the target.
        if link.exists() or os.path.islink(link):
            subprocess.run(["cmd", "/c", "rmdir", str(link)], check=False)
        subprocess.run(["cmd", "/c", "mklink", "/J", str(link), str(target)], check=True)
    else:
        if link.is_symlink() or link.exists():
            link.unlink()
        os.symlink(target, link, target_is_directory=True)
    print(f"Pointed {link} -> {target}")
    return link


def persist_env_var(name: str, value: str) -> None:
    if os.name != "nt":
        print(f"Add to your shell rc: export {name}={value}")
        return
    subprocess.run(["setx", name, str(value)], check=True)
    print(f"Persisted {name} (effective in new shells)")


def main() -> None:
    parser = argparse.ArgumentParser(description="Download and install the prebuilt native-Windows GCC trunk toolchain")
    parser.add_argument("--tag", default=None, help="Release tag to install (default: latest gcc-trunk-v* release)")
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

    current = link_current(args.tag)

    print(
        "\nThe CMake presets resolve the toolchain via "
        "$env{USERPROFILE}/.gcc-trunk/current, so no environment variable is required."
    )
    if args.persist:
        persist_env_var(ENV_VAR, str(current))
    else:
        print(f"\nOptional (manual command-line use only): re-run with --persist, or")
        print(f'  setx {ENV_VAR} "{current}"')


if __name__ == "__main__":
    main()
