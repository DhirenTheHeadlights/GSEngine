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
    "lld-link.exe",
    "lld.exe",
    "llvm-ar.exe",
    "llvm-rc.exe",
    "llvm-lib.exe",
    "clang-scan-deps.exe",
}


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


def configure(src, build, link_jobs):
    build.mkdir(parents=True, exist_ok=True)
    cmake_args = [
        "cmake", "-S", str(src / "llvm"), "-B", str(build),
        "-G", "Ninja",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DLLVM_ENABLE_PROJECTS=clang;lld",
        "-DLLVM_TARGETS_TO_BUILD=X86",
        "-DLLVM_ENABLE_ASSERTIONS=OFF",
        "-DLLVM_USE_CRT_RELEASE=MT",
    ]
    if link_jobs is not None:
        cmake_args.append(f"-DLLVM_PARALLEL_LINK_JOBS={link_jobs}")
    run(cmake_args)


def build_targets(build):
    run(["cmake", "--build", str(build), "--target", "clang", "clang-cl", "lld-link"])


def stage(build, dist):
    if dist.exists():
        shutil.rmtree(dist)
    (dist / "bin").mkdir(parents=True)
    bin_src = build / "bin"
    for name in KEEP_BINARIES:
        src = bin_src / name
        if src.exists():
            shutil.copy2(src, dist / "bin" / name)

    lib_src = build / "lib" / "clang"
    if lib_src.exists():
        shutil.copytree(lib_src, dist / "lib" / "clang")


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
    parser.add_argument("--src", type=Path, default=Path("external") / "clang-p2996")
    parser.add_argument("--build", type=Path, default=Path("build") / "clang-p2996")
    parser.add_argument("--dist", type=Path, default=Path("dist") / "clang-p2996")
    parser.add_argument("--out", type=Path, default=Path("dist") / "clang-p2996-windows-x64.zip")
    parser.add_argument("--branch", default=DEFAULT_BRANCH)
    parser.add_argument("--sha", default=None, help="Pin clang-p2996 to a specific commit SHA")
    parser.add_argument("--skip-build", action="store_true", help="Only stage and zip an existing build")
    parser.add_argument(
        "--link-jobs",
        type=int,
        default=None,
        help="Set LLVM_PARALLEL_LINK_JOBS (limit concurrent linker procs to cap memory; useful on CI)",
    )
    args = parser.parse_args()

    if not args.skip_build:
        ensure_clone(args.src, args.branch, args.sha)
        configure(args.src, args.build, args.link_jobs)
        build_targets(args.build)

    stage(args.build, args.dist)
    zip_dist(args.dist, args.out)

    print()
    print(f"Source SHA: {current_sha(args.src)}")
    print(f"Artifact:   {args.out}")
    print(f"SHA256:     {sha256_of(args.out)}")


if __name__ == "__main__":
    main()
