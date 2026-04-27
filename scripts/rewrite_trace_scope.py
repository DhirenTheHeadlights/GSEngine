#!/usr/bin/env python3
"""Rewrite trace::scope(EXPR, [&]{ BODY }); call sites to:
       { trace::scope_guard sg{EXPR}; BODY }
   And the 3-arg form trace::scope(EXPR, [&]{ BODY }, PARENT); to:
       { trace::scope_guard sg{EXPR, PARENT}; BODY }
"""

import re
import sys
from pathlib import Path

ROOT = Path("/home/dhiren/GSEngine")

SKIP = {
    ROOT / "Engine/Engine/Source/Diag/Trace.cppm",
    ROOT / "Engine/Engine/Source/Diag/Trace.cpp",
    ROOT / "scripts/rewrite_trace_scope.py",
}

# Find every "trace::scope(" occurrence that begins a statement.
START_RE = re.compile(r'(?P<prefix>(?:gse::)?trace::scope\s*\(\s*)')

def find_lambda_open(text, i):
    """Given i is just after the opening '(' of trace::scope, find the
    position of the '{' that opens the lambda body, plus the EXPR that
    came before it. Returns (expr_start, expr_end, brace_pos) or None.
    The lambda is expected to be `[&] {` or `[&]{` (possibly with whitespace).
    """
    depth = 1
    j = i
    n = len(text)
    expr_start = i
    # Walk until we find ', [' at top-level paren depth (depth==1).
    while j < n:
        c = text[j]
        if c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
            if depth == 0:
                return None  # closed without finding lambda
        elif c == ',' and depth == 1:
            # Look ahead past whitespace for '[&]'
            k = j + 1
            while k < n and text[k] in ' \t\n':
                k += 1
            if text[k:k+3] == '[&]':
                expr_end = j
                # Skip past '[&]'
                k += 3
                # Skip whitespace
                while k < n and text[k] in ' \t\n':
                    k += 1
                if k < n and text[k] == '{':
                    return (expr_start, expr_end, k)
                return None
        j += 1
    return None

def find_brace_close(text, brace_pos):
    """Find matching '}' for '{' at brace_pos."""
    depth = 1
    j = brace_pos + 1
    n = len(text)
    while j < n:
        c = text[j]
        if c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                return j
        j += 1
    return None

def rewrite(text):
    """Rewrite all trace::scope(...) calls in text. Returns (new_text, count)."""
    out = []
    i = 0
    n = len(text)
    count = 0
    while i < n:
        m = START_RE.search(text, i)
        if not m:
            out.append(text[i:])
            break
        # Emit untouched up to match
        out.append(text[i:m.start()])
        prefix_end = m.end()  # position right after the '('
        result = find_lambda_open(text, prefix_end)
        if not result:
            # Not a lambda-taking call; leave alone
            out.append(text[m.start():prefix_end])
            i = prefix_end
            continue
        expr_start, expr_end, brace_pos = result
        close = find_brace_close(text, brace_pos)
        if close is None:
            out.append(text[m.start():prefix_end])
            i = prefix_end
            continue
        # After close, expect optional ',', expr, ')', ';'
        k = close + 1
        # Skip whitespace
        while k < n and text[k] in ' \t\n':
            k += 1
        parent_expr = None
        if k < n and text[k] == ',':
            # 3-arg form; capture parent expression up to matching ')'
            k += 1
            depth = 1
            parent_start = k
            while k < n and depth > 0:
                c = text[k]
                if c == '(':
                    depth += 1
                elif c == ')':
                    depth -= 1
                    if depth == 0:
                        break
                k += 1
            if k >= n:
                out.append(text[m.start():prefix_end])
                i = prefix_end
                continue
            parent_expr = text[parent_start:k].strip()
            # k is at ')'
            close_paren = k
        elif k < n and text[k] == ')':
            close_paren = k
        else:
            out.append(text[m.start():prefix_end])
            i = prefix_end
            continue
        # Expect ';' after ')'
        s = close_paren + 1
        while s < n and text[s] in ' \t':
            s += 1
        if s < n and text[s] == ';':
            stmt_end = s + 1
        else:
            stmt_end = close_paren + 1

        expr = text[expr_start:expr_end].strip()
        body = text[brace_pos+1:close]
        # Determine indentation of original statement
        line_start = text.rfind('\n', 0, m.start()) + 1
        indent = text[line_start:m.start()]

        if parent_expr is not None:
            head = f"{{\n{indent}\ttrace::scope_guard sg{{{expr}, {parent_expr}}};"
        else:
            head = f"{{\n{indent}\ttrace::scope_guard sg{{{expr}}};"

        # The body content is between the lambda's opening `{` and its closing `}`.
        # Strip leading/trailing newlines but keep its inner whitespace intact.
        body_stripped = body.rstrip()
        # Remove a leading newline if present
        if body_stripped.startswith('\n'):
            body_stripped = body_stripped[1:]

        replacement = head + "\n" + body_stripped + f"\n{indent}}}"
        out.append(replacement)
        count += 1
        i = stmt_end
    return ''.join(out), count

def main():
    paths = []
    for ext in ("*.cppm", "*.cpp"):
        paths.extend((ROOT / "Engine").rglob(ext))
        paths.extend((ROOT / "Game").rglob(ext))
    total = 0
    for p in paths:
        if p in SKIP:
            continue
        text = p.read_text()
        if "trace::scope(" not in text:
            continue
        new_text, count = rewrite(text)
        if count > 0:
            p.write_text(new_text)
            print(f"{p.relative_to(ROOT)}: {count} call sites rewritten")
            total += count
    print(f"Total: {total}")

if __name__ == "__main__":
    main()
