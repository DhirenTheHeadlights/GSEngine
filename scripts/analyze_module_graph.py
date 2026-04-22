import json, os, sys, glob, collections

BUILD = sys.argv[1] if len(sys.argv) > 1 else r"out/build/x64-clang-p2996-libcxx-Release-trace"

module_provides = {}
module_requires = collections.defaultdict(set)
tu_for_module = {}
source_for_module = {}

for ddi in glob.glob(os.path.join(BUILD, "**", "*.ddi"), recursive=True):
    try:
        with open(ddi, "r", encoding="utf-8") as f:
            d = json.load(f)
    except Exception:
        continue
    for rule in d.get("rules", []):
        primary = rule.get("primary-output", "").replace("\\", "/")
        provides = rule.get("provides", [])
        reqs = [r.get("logical-name") for r in rule.get("requires", []) if r.get("logical-name")]
        for p in provides:
            name = p.get("logical-name")
            src = p.get("source-path", "").replace("\\", "/")
            if name:
                module_provides[name] = primary
                source_for_module[name] = src
                for r in reqs:
                    module_requires[name].add(r)

tu_time = {}
for path in glob.glob(os.path.join(BUILD, "**", "*.cppm.json"), recursive=True) + \
            glob.glob(os.path.join(BUILD, "**", "*.cpp.json"), recursive=True):
    if "CompilerIdCXX" in path:
        continue
    try:
        with open(path, "r", encoding="utf-8") as f:
            data = json.load(f)
    except Exception:
        continue
    exec_us = 0
    for e in data.get("traceEvents", []):
        if e.get("ph") == "X" and e.get("name") == "ExecuteCompiler":
            exec_us = max(exec_us, e.get("dur", 0))
    rel = os.path.relpath(path, BUILD).replace("\\", "/")
    obj_key = rel.removesuffix(".json")
    tu_time[obj_key] = exec_us

def time_for_module(m):
    obj = module_provides.get(m)
    if not obj:
        return 0
    key = obj.replace("\\", "/").removesuffix(".obj")
    return tu_time.get(key, 0)

fanin = collections.Counter()
for m, reqs in module_requires.items():
    for r in reqs:
        fanin[r] += 1

memo = {}
path_memo = {}
def longest_path_to(m):
    if m in memo:
        return memo[m], path_memo[m]
    best = 0
    best_path = []
    for dep in module_requires.get(m, ()):
        t, p = longest_path_to(dep)
        if t > best:
            best = t
            best_path = p
    total = best + time_for_module(m)
    memo[m] = total
    path_memo[m] = best_path + [m]
    return total, path_memo[m]

worst = 0
worst_m = None
for m in module_provides:
    t, _ = longest_path_to(m)
    if t > worst:
        worst = t
        worst_m = m

print(f"Modules discovered: {len(module_provides)}")
print(f"Critical-path TU: {worst_m}")
print(f"Critical-path sum: {worst/1_000_000:.2f}s")
print()
print("Critical path (bottom-up, each with its own compile time):")
_, path = longest_path_to(worst_m)
cum = 0
for m in path:
    t = time_for_module(m)
    cum += t
    src = source_for_module.get(m, "?").rsplit("/", 2)
    print(f"  {t/1_000_000:5.2f}s  cum {cum/1_000_000:6.2f}s   {m}")
print()

print("=== Top 25 modules by fan-in (times imported) ===")
for m, n in fanin.most_common(25):
    t = time_for_module(m)
    print(f"  imported by {n:3}   own_time={t/1_000_000:4.2f}s   {m}")
print()

cost_weighted = []
for m, n in fanin.items():
    t = time_for_module(m)
    cost_weighted.append((m, n, t, n * t))
cost_weighted.sort(key=lambda x: x[3], reverse=True)
print("=== Top 25 modules by (fan-in x own compile time) — 'splitting these helps most' ===")
for m, n, t, w in cost_weighted[:25]:
    print(f"  fanin={n:3}  own={t/1_000_000:4.2f}s  weight={w/1_000_000:6.2f}s-imports  {m}")
