import subprocess
import sys
from pathlib import Path

report_flag = "-ftime-report"
report_header = b"Time variable"
report_suffix = ".time-report.txt"


def report_path(command):
    for index, argument in enumerate(command):
        if argument == "-o" and index + 1 < len(command):
            return Path(command[index + 1] + report_suffix)
    raise SystemExit("time_report: the compile command carries no -o, so the report has nowhere to go")


def split_at_report(text):
    lines = text.splitlines(keepends=True)
    for index, line in enumerate(lines):
        if not line.startswith(report_header):
            continue
        start = index
        while start > 0 and not lines[start - 1].strip():
            start -= 1
        return b"".join(lines[:start]), b"".join(lines[index:])
    return text, b""


def main(command):
    path = report_path(command)
    completed = subprocess.run([*command, report_flag], stderr=subprocess.PIPE)
    diagnostics, report = split_at_report(completed.stderr)
    sys.stderr.buffer.write(diagnostics)
    path.write_bytes(report)
    return completed.returncode


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
