"""Where the repository and everything it generates live.

Every build tree, download cache, toolchain workspace and packaging stage sits
under GENERATED_ROOT, so the repo root holds only tracked sources and one
ignored folder.
"""
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

GENERATED_ROOT = REPO_ROOT / ".generated"

BUILD_ROOT = GENERATED_ROOT / "out" / "build"

DIST_ROOT = GENERATED_ROOT / "dist"
