import subprocess
import sys
import tempfile
from pathlib import Path

import time_report

header = "\n%-35s%16s%14s\n" % ("Time variable", "wall", "GGC")
body = (" phase setup                        :   0.01 (  0%)  1234 kB (  0%)\n"
        " template instantiation             :   1.53 ( 41%) 98765 kB ( 22%)\n"
        " TOTAL                              :   3.71        456789 kB\n")
warning = "Foo.cppm:12:5: warning: unused variable 'x' [-Wunused-variable]\n"

stub = ("import sys\n"
        "sys.stderr.write(sys.argv[1])\n"
        "sys.exit(int(sys.argv[2]))\n")


def run(stderr_text, code, out):
    command = [sys.executable, "-c", stub, stderr_text, str(code), "-o", str(out)]
    done = subprocess.run([sys.executable, time_report.__file__, *command],
                          capture_output=True, text=True)
    return done.returncode, done.stderr, Path(str(out) + time_report.report_suffix)


def main():
    failures = []
    with tempfile.TemporaryDirectory() as directory:
        out = Path(directory) / "Foo.cppm.obj"

        code, err, path = run(warning + header + body, 0, out)
        if code != 0:
            failures.append(f"success exit code was {code}")
        if err != warning:
            failures.append(f"warning did not pass through byte-exact: {err!r}")
        if not path.read_text().startswith("Time variable"):
            failures.append(f"report file wrong: {path.read_text()[:40]!r}")
        if "TOTAL" in err:
            failures.append("report leaked into the build panel")

        code, err, path = run("Foo.cppm:3:1: error: boom\n", 1, out)
        if code != 1:
            failures.append(f"failure exit code was {code}")
        if "error: boom" not in err:
            failures.append("diagnostics were swallowed on failure")
        if path.read_text() != "":
            failures.append("stale report survived a failed compile")

        code, err, path = run(header + body, 0, out)
        if err != "":
            failures.append(f"clean compile emitted {err!r}")

    print("FAILED: " + "; ".join(failures) if failures else "all checks pass")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
