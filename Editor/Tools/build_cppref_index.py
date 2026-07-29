#!/usr/bin/env python3
"""Build the GSEditor cppreference hover index (Resources/cppref.idx).

Reads the official offline cppreference package directly (no extraction — the
package contains filenames that are invalid on Windows, so we stream it):
  https://github.com/PeterFeicht/cppreference-doc/releases
  -> cppreference-doc-YYYYMMDD.tar.xz

It parses the bundled index-functions-cpp.xml (cppreference's native symbol
index) for qualified-name -> (kind, page) mappings, then scrapes each page's
lead description for a brief.

Output: a TSV where each line is
    <qualified-name>\t<kind>\t<page>\t<brief>
with \n \t \\ escaped in the brief. <page> is the cppreference page path
(the editor builds https://en.cppreference.com/w/<page> for the link).
GSEditor loads it into gse::ide::docs::cppref_index. Symbols whose page yields
no usable lead description are omitted (the editor falls back to the header
doc-comment).

bootstrap.py fetches the package and calls this automatically; run by hand to
regenerate from a downloaded package:

  python build_cppref_index.py --tarball cppreference-doc-20250209.tar.xz \
                               --out ../Resources/cppref.idx
"""

import argparse
import html
import os
import re
import sys
import tarfile
import xml.etree.ElementTree as ET
from html.parser import HTMLParser

MIN_BRIEF_CHARS = 40
MAX_BRIEF_CHARS = 400
INDEX_NAME = "index-functions-cpp.xml"
HTML_SUBDIR = "reference/en.cppreference.com/w/"
MARKER_RE = re.compile(r"^\s*\(?\d+\)\s*")

KIND_BY_TAG = {
    "class": "class",
    "struct": "class",
    "function": "function",
    "overload": "function",
    "typedef": "type alias",
    "const": "constant",
    "variable": "variable",
    "enum": "enum",
    "namespace": "namespace",
}


class LeadBlock(HTMLParser):
    """Capture the first substantial <p> or <div class="t-li1"> in the body.

    cppreference puts a class's one-line summary in a numbered t-li1 div, and a
    function's summary in a leading <p>; whichever comes first is the brief.
    Content inside <table>/<script>/<style> (declaration synopsis, etc.) is
    ignored.
    """

    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.skip = 0
        self.cap_tag = None
        self.cap_depth = 0
        self.buf = []
        self.result = ""
        self.done = False

    def handle_starttag(self, tag, attrs):
        if self.done:
            return
        if tag in ("script", "style", "table"):
            self.skip += 1
            return
        if self.skip > 0:
            return
        if self.cap_tag is not None:
            if tag == self.cap_tag:
                self.cap_depth += 1
            return
        if tag == "p":
            self.cap_tag, self.cap_depth, self.buf = "p", 1, []
        elif tag == "div" and "t-li1" in dict(attrs).get("class", "").split():
            self.cap_tag, self.cap_depth, self.buf = "div", 1, []

    def handle_endtag(self, tag):
        if self.done:
            return
        if tag in ("script", "style", "table"):
            if self.skip > 0:
                self.skip -= 1
            return
        if self.cap_tag is not None and tag == self.cap_tag:
            self.cap_depth -= 1
            if self.cap_depth == 0:
                text = "".join(self.buf).strip()
                if len(re.sub(r"[^A-Za-z]", "", text)) >= MIN_BRIEF_CHARS:
                    self.result = text
                    self.done = True
                else:
                    self.cap_tag, self.buf = None, []

    def handle_data(self, data):
        if not self.done and self.cap_tag is not None and self.skip == 0:
            self.buf.append(data)


def clean_brief(text):
    text = html.unescape(text)
    text = re.sub(r"\s+", " ", text).strip()
    text = MARKER_RE.sub("", text)
    if len(text) > MAX_BRIEF_CHARS:
        cut = text.rfind(".", 0, MAX_BRIEF_CHARS)
        text = text[: cut + 1] if cut > MIN_BRIEF_CHARS else text[:MAX_BRIEF_CHARS] + "..."
    return text


def is_abs_link(link):
    return link.startswith("cpp/") or link.startswith("c/")


def walk_index(elem, parent_name, parent_link, sink, sink_alias):
    if elem.tag == "specialization":
        return
    name = elem.get("name")
    link = elem.get("link")
    alias = elem.get("alias")
    cur_name, cur_link = parent_name, parent_link

    if name is not None:
        qualified = name if "::" in name else (f"{parent_name}::{name}" if parent_name else name)
        page = None
        if link:
            page = link if is_abs_link(link) else (f"{parent_link}/{link}" if parent_link else None)
        elif not alias:
            page = f"{parent_link}/{name}" if parent_link else None
        if page:
            sink(qualified, page, KIND_BY_TAG.get(elem.tag, ""))
        elif alias:
            sink_alias(qualified, alias, KIND_BY_TAG.get(elem.tag, ""))
        cur_name, cur_link = qualified, (page or parent_link)
    elif elem.tag in ("constructor", "destructor") and parent_name and parent_link:
        base = parent_name.split("::")[-1]
        if elem.tag == "constructor":
            sink(f"{parent_name}::{base}", f"{parent_link}/{base}", "constructor")
        else:
            sink(f"{parent_name}::~{base}", f"{parent_link}/~{base}", "destructor")

    for child in elem:
        walk_index(child, cur_name, cur_link, sink, sink_alias)


def parse_index(index_bytes):
    symbols = []
    seen = set()
    aliases = []
    page_by_name = {}

    def sink(qualified, page, kind):
        if qualified not in seen:
            seen.add(qualified)
            symbols.append((qualified, page, kind))
            page_by_name[qualified] = page

    def sink_alias(qualified, target, kind):
        aliases.append((qualified, target, kind))

    walk_index(ET.fromstring(index_bytes), "", "", sink, sink_alias)

    for qualified, target, kind in aliases:
        page = page_by_name.get(target)
        if page and qualified not in seen:
            seen.add(qualified)
            symbols.append((qualified, page, kind))
    return symbols


def read_index_and_root(tarball):
    with tarfile.open(tarball, "r:xz") as tf:
        for member in tf:
            if member.isfile() and member.name.endswith(INDEX_NAME):
                data = tf.extractfile(member).read()
                root = member.name[: -len(INDEX_NAME)]
                return data, root
    return None, None


def scrape_briefs(tarball, wanted_members):
    briefs = {}
    with tarfile.open(tarball, "r:xz") as tf:
        for member in tf:
            page = wanted_members.get(member.name)
            if page is None or page in briefs or not member.isfile():
                continue
            handle = tf.extractfile(member)
            if handle is None:
                continue
            parser = LeadBlock()
            parser.feed(handle.read().decode("utf-8", "replace"))
            brief = clean_brief(parser.result)
            if brief:
                briefs[page] = brief
    return briefs


def main():
    ap = argparse.ArgumentParser(description="Build GSEditor cppreference hover index.")
    ap.add_argument("--tarball", required=True, help="cppreference-doc-YYYYMMDD.tar.xz")
    default_out = os.path.join(os.path.dirname(__file__), "..", "Resources", "cppref.idx")
    ap.add_argument("--out", default=default_out, help="output .idx path")
    ap.add_argument("--limit", type=int, default=0, help="stop after N entries (debug)")
    args = ap.parse_args()

    if not os.path.isfile(args.tarball):
        sys.exit(f"tarball not found: {args.tarball}")

    index_bytes, root = read_index_and_root(args.tarball)
    if index_bytes is None:
        sys.exit(f"{INDEX_NAME} not found in {args.tarball}")

    symbols = parse_index(index_bytes)
    html_prefix = root + HTML_SUBDIR
    wanted = {}
    for _, page, _ in symbols:
        wanted.setdefault(html_prefix + page + ".html", page)

    print(f"indexed {len(symbols)} symbols across {len(wanted)} pages; scraping...", file=sys.stderr)
    briefs = scrape_briefs(args.tarball, wanted)

    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    written = 0
    with open(args.out, "w", encoding="utf-8", newline="\n") as out:
        for qualified, page, kind in symbols:
            brief = briefs.get(page)
            if not brief:
                continue
            escaped = brief.replace("\\", "\\\\").replace("\t", "\\t").replace("\n", "\\n")
            out.write(f"{qualified}\t{kind}\t{page}\t{escaped}\n")
            written += 1
            if args.limit and written >= args.limit:
                break

    print(f"wrote {written} entries to {args.out}", file=sys.stderr)


if __name__ == "__main__":
    main()
