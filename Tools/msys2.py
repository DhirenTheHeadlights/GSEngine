"""Shared MSYS2 driving layer for the toolchain build scripts.

Extracted from build_gcc_trunk.py so build_gcc_wasm.py can reuse it rather than
copy it. The non-obvious parts worth keeping in one place: the sanitised
environment `bash -lc` has to run under, the LF-only PKGBUILD write, and the
cached-source fallback for snapshots GCC has pruned from its server.
"""
import re
import shutil
import subprocess
from pathlib import Path

from repo_paths import GENERATED_ROOT, REPO_ROOT

DEFAULT_MSYS_ROOT = GENERATED_ROOT / "msys64"


def to_msys(p: Path) -> str:
    s = str(Path(p).resolve())
    if len(s) > 1 and s[1] == ":":
        return "/" + s[0].lower() + s[2:].replace("\\", "/")
    return s.replace("\\", "/")


def bash(msys_root: Path, script: str, check: bool = True, msystem: str = "MINGW64") -> subprocess.CompletedProcess:
    bash_exe = msys_root / "usr" / "bin" / "bash.exe"
    if not bash_exe.exists():
        raise SystemExit(f"bash not found at {bash_exe} (wrong --msys-root?)")
    print(f"$ (msys2) {script}", flush=True)
    res = subprocess.run(
        [str(bash_exe), "-lc", script],
        env={"MSYSTEM": msystem, "CHERE_INVOKING": "1", "PATH": r"C:\Windows\System32;C:\Windows"},
    )
    if check and res.returncode != 0:
        raise SystemExit(f"msys2 command failed ({res.returncode}): {script}")
    return res


def set_snapshot(pkgbuild: Path, snapshot: str) -> None:
    text = pkgbuild.read_text()
    text, n = re.subn(r"^_snapshot=.*$", f"_snapshot={snapshot}", text, count=1, flags=re.M)
    if n != 1:
        raise SystemExit("could not set _snapshot in PKGBUILD")
    # makepkg refuses to source a PKGBUILD containing CRLF. Path.write_text() on
    # Windows translates every "\n" -> "\r\n", which would corrupt the recipe, so
    # write LF explicitly. The explicit newline="\n" on open() also normalises any
    # CRLF a Windows git checkout (autocrlf) introduced into the source file.
    with open(pkgbuild, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    print(f"Set _snapshot={snapshot}")


def stage_cached_source(build_recipe: Path, snapshot: str, cache_dirs: list) -> None:
    """Make makepkg use a locally cached GCC source tarball instead of downloading.

    GCC removes old weekly snapshots from its server, so building a pinned older
    snapshot 404s. If a matching tarball is cached locally, copy it into the recipe dir
    (makepkg checks startdir before downloading). A plain .tar is supported by pointing
    the PKGBUILD source at it, which avoids a slow recompression to .tar.xz.
    """
    for cache in cache_dirs:
        if not cache or not Path(cache).is_dir():
            continue
        for xz in sorted(Path(cache).glob(f"gcc-*-{snapshot}.tar.xz")):
            print(f"Using cached source {xz} (skipping download)")
            shutil.copy2(xz, build_recipe / xz.name)
            return
        for plain in sorted(Path(cache).glob(f"gcc-*-{snapshot}.tar")):
            print(f"Using cached source {plain} (skipping download; uncompressed)")
            shutil.copy2(plain, build_recipe / plain.name)
            pk = build_recipe / "PKGBUILD"
            text = pk.read_text(encoding="utf-8").replace("${_sourcedir}.tar.xz", "${_sourcedir}.tar", 1)
            with open(pk, "w", encoding="utf-8", newline="\n") as f:
                f.write(text)
            return
    print(f"No cached source for snapshot {snapshot}; makepkg will download it")
