import argparse
import hashlib
import shutil
import subprocess
from pathlib import Path

REPO_URL = "https://github.com/bloomberg/clang-p2996.git"
DEFAULT_BRANCH = "p2996"

KEEP_BINARIES = {
    "clang.exe",
    "clang-cl.exe",
    "clang++.exe",
    "clangd.exe",
    "clang-format.exe",
    "clang-tidy.exe",
    "lld-link.exe",
    "lld.exe",
    "llvm-ar.exe",
    "llvm-rc.exe",
    "llvm-lib.exe",
    "clang-scan-deps.exe",
}

FULL_BUILD_TARGETS = [
    "clang", "lld",
    "llvm-ar", "llvm-lib", "llvm-rc", "clang-scan-deps",
    "clangd", "clang-format", "clang-tidy",
]

CLANGD_ONLY_TARGETS = ["clangd"]

GSE_TIDY_CHECKS_SRC = Path("scripts") / "gse_tidy_checks"
GSE_TIDY_DST_SUBDIR = Path("clang-tools-extra") / "clang-tidy" / "gse"
GSE_FORCE_LINKER_ANCHOR = """
// This anchor is used to force the linker to link the GseModule.
extern volatile int GseModuleAnchorSource;
static int LLVM_ATTRIBUTE_UNUSED GseModuleAnchorDestination =
    GseModuleAnchorSource;
"""


def run(cmd, cwd=None):
    display = " ".join(str(c) for c in cmd)
    print(f"$ {display}")
    result = subprocess.run(cmd, cwd=cwd)
    if result.returncode != 0:
        raise SystemExit(f"Command failed: {display}")


def ensure_clone(src, branch, sha):
    if not src.exists():
        run(["git", "clone", "--branch", branch, REPO_URL, str(src)])
    if sha:
        run(["git", "fetch", "origin", sha], cwd=src)
        run(["git", "checkout", sha], cwd=src)


def current_sha(src):
    result = subprocess.run(
        ["git", "rev-parse", "--short", "HEAD"],
        cwd=src,
        capture_output=True,
        text=True,
        check=True,
    )
    return result.stdout.strip()


def install_gse_tidy_checks(src):
    repo_root = Path(__file__).resolve().parent.parent
    gse_src = repo_root / GSE_TIDY_CHECKS_SRC
    if not gse_src.exists():
        raise SystemExit(f"GSE tidy check sources not found at {gse_src}")

    gse_dst = src / GSE_TIDY_DST_SUBDIR
    if gse_dst.exists():
        shutil.rmtree(gse_dst)
    gse_dst.mkdir(parents=True)
    for entry in gse_src.iterdir():
        if entry.name == ".clangd":
            continue
        if entry.is_file():
            shutil.copy2(entry, gse_dst / entry.name)

    tidy_cmake = src / "clang-tools-extra" / "clang-tidy" / "CMakeLists.txt"
    text = tidy_cmake.read_text()
    changed = False
    if "add_subdirectory(gse)" not in text:
        marker = "add_subdirectory(zircon)"
        if marker not in text:
            raise SystemExit(f"Couldn't find '{marker}' in {tidy_cmake}")
        text = text.replace(marker, f"{marker}\nadd_subdirectory(gse)", 1)
        changed = True
    if "clangTidyGseModule" not in text:
        marker = "clangTidyZirconModule"
        if marker not in text:
            raise SystemExit(f"Couldn't find '{marker}' in {tidy_cmake}")
        text = text.replace(
            marker, f"{marker}\n  clangTidyGseModule", 1
        )
        changed = True
    if changed:
        tidy_cmake.write_text(text)

    force_linker = src / "clang-tools-extra" / "clang-tidy" / "ClangTidyForceLinker.h"
    text = force_linker.read_text()
    if "GseModuleAnchorSource" not in text:
        marker = "} // namespace clang::tidy"
        if marker not in text:
            raise SystemExit(f"Couldn't find namespace close in {force_linker}")
        text = text.replace(marker, f"{GSE_FORCE_LINKER_ANCHOR}\n{marker}", 1)
        force_linker.write_text(text)


def configure(src, build, link_jobs):
    build.mkdir(parents=True, exist_ok=True)
    cmake_args = [
        "cmake", "-S", str(src / "llvm"), "-B", str(build),
        "-G", "Ninja",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DLLVM_ENABLE_PROJECTS=clang;clang-tools-extra;lld",
        "-DLLVM_TARGETS_TO_BUILD=X86",
        "-DLLVM_ENABLE_ASSERTIONS=OFF",
        "-DLLVM_ENABLE_DIA_SDK=OFF",
    ]
    if link_jobs is not None:
        cmake_args.append(f"-DLLVM_PARALLEL_LINK_JOBS={link_jobs}")
    run(cmake_args)


def build_targets(build, targets):
    run(["cmake", "--build", str(build), "--target", *targets])


def stage(build, dist, *, full):
    if full and dist.exists():
        shutil.rmtree(dist)
    (dist / "bin").mkdir(parents=True, exist_ok=True)
    bin_src = build / "bin"
    for name in KEEP_BINARIES:
        src = bin_src / name
        if src.exists():
            shutil.copy2(src, dist / "bin" / name)

    if full:
        lib_src = build / "lib" / "clang"
        if lib_src.exists():
            shutil.copytree(lib_src, dist / "lib" / "clang")


def stage_clangd_only(build, dist):
    (dist / "bin").mkdir(parents=True, exist_ok=True)
    src = build / "bin" / "clangd.exe"
    if not src.exists():
        raise SystemExit(f"clangd.exe not found at {src} — did the build succeed?")
    shutil.copy2(src, dist / "bin" / "clangd.exe")
    print(f"Staged clangd into {dist / 'bin' / 'clangd.exe'}")


def zip_dist(dist, out_zip):
    if out_zip.exists():
        out_zip.unlink()
    out_zip.parent.mkdir(parents=True, exist_ok=True)
    shutil.make_archive(str(out_zip.with_suffix("")), "zip", root_dir=dist)


def sha256_of(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main():
    parser = argparse.ArgumentParser(description="Build and package bloomberg/clang-p2996 for distribution")
    parser.add_argument("--src", type=Path, default=Path("External") / "clang-p2996")
    parser.add_argument("--build", type=Path, default=Path("build") / "clang-p2996")
    parser.add_argument("--dist", type=Path, default=Path("dist") / "clang-p2996")
    parser.add_argument("--out", type=Path, default=Path("dist") / "clang-p2996-windows-x64.zip")
    parser.add_argument("--branch", default=DEFAULT_BRANCH)
    parser.add_argument("--sha", default=None, help="Pin clang-p2996 to a specific commit SHA")
    parser.add_argument("--skip-build", action="store_true", help="Only stage and zip an existing build")
    parser.add_argument(
        "--clangd-only",
        action="store_true",
        help="Build only clangd and graft it into an existing dist without wiping libc++/compiler-rt",
    )
    parser.add_argument(
        "--link-jobs",
        type=int,
        default=None,
        help="Set LLVM_PARALLEL_LINK_JOBS (limit concurrent linker procs to cap memory; useful on CI)",
    )
    args = parser.parse_args()

    if args.clangd_only:
        ensure_clone(args.src, args.branch, args.sha)
        install_gse_tidy_checks(args.src)
        configure(args.src, args.build, args.link_jobs)
        build_targets(args.build, CLANGD_ONLY_TARGETS)
        stage_clangd_only(args.build, args.dist)
        print(f"\nSource SHA: {current_sha(args.src)}")
        return

    if not args.skip_build:
        ensure_clone(args.src, args.branch, args.sha)
        install_gse_tidy_checks(args.src)
        configure(args.src, args.build, args.link_jobs)
        build_targets(args.build, FULL_BUILD_TARGETS)

    stage(args.build, args.dist, full=True)
    zip_dist(args.dist, args.out)

    print()
    print(f"Source SHA: {current_sha(args.src)}")
    print(f"Artifact:   {args.out}")
    print(f"SHA256:     {sha256_of(args.out)}")


if __name__ == "__main__":
    main()
