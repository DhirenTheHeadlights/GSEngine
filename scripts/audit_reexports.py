import json, glob, os, collections, sys

BUILD = sys.argv[1] if len(sys.argv) > 1 else r"out/build/x64-clang-p2996-libcxx-Release-trace"

module_of = {}
sources = {}
requires = collections.defaultdict(set)
exports_reexports = collections.defaultdict(set)

for ddi in glob.glob(os.path.join(BUILD, "**", "*.ddi"), recursive=True):
    try:
        d = json.load(open(ddi))
    except Exception:
        continue
    for rule in d.get("rules", []):
        reqs = [r.get("logical-name") for r in rule.get("requires", []) if r.get("logical-name")]
        for p in rule.get("provides", []):
            name = p.get("logical-name")
            if not name:
                continue
            module_of[name] = name
            sources[name] = p.get("source-path", "").replace("\\", "/")
            requires[name].update(reqs)

import re

for name, src in sources.items():
    try:
        text = open(src, "r", encoding="utf-8", errors="replace").read()
    except Exception:
        continue
    for m in re.finditer(r'^\s*export\s+import\s+([\w\.:]+)\s*;', text, re.MULTILINE):
        tok = m.group(1)
        if tok.startswith(":"):
            parent = name.split(":", 1)[0]
            full = f"{parent}:{tok[1:]}"
        else:
            full = tok
        exports_reexports[name].add(full)

importers = collections.defaultdict(set)
for name, reqs in requires.items():
    for r in reqs:
        importers[r].add(name)

umbrellas = [n for n in module_of if ":" not in n and n.startswith("gse")]

def same_umbrella(a, b):
    au = a.split(":", 1)[0]
    bu = b.split(":", 1)[0]
    return au == bu

print("="*80)
print("Partition audit: which are imported ONLY within their own umbrella?")
print("="*80)
print()

for umb in sorted(umbrellas):
    parts = [n for n in module_of if n.startswith(umb + ":")]
    if not parts:
        continue
    internal_only = []
    external_users = []
    reexported = exports_reexports.get(umb, set())
    for p in parts:
        imps = importers.get(p, set())
        outside = [i for i in imps if not same_umbrella(i, p)]
        if p in reexported:
            if outside:
                external_users.append((p, outside))
            else:
                internal_only.append(p)
    if not parts:
        continue
    print(f"--- {umb} ({len(parts)} partitions, {len(reexported)} re-exported) ---")
    if internal_only:
        print(f"  Re-exported but used ONLY inside the umbrella (safe to demote to plain 'import'):")
        for p in sorted(internal_only):
            print(f"    {p}")
    if external_users:
        print(f"  Re-exported AND consumed externally (need to keep OR require consumers to import directly):")
        for p, users in sorted(external_users)[:15]:
            u_short = ", ".join(sorted(users)[:3])
            extra = "" if len(users) <= 3 else f" (+{len(users)-3} more)"
            print(f"    {p}   used by: {u_short}{extra}")
    print()
