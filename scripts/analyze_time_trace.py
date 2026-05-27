import json, os, sys, glob, collections, re

BUILD = sys.argv[1] if len(sys.argv) > 1 else r"out/build/x64-clang-p2996-libcxx-Release-trace"

trace_files = glob.glob(os.path.join(BUILD, "**", "*.cppm.json"), recursive=True) + \
              glob.glob(os.path.join(BUILD, "**", "*.cpp.json"), recursive=True)
trace_files = [f for f in trace_files if "CompilerIdCXX" not in f]

per_tu = []
read_ast_totals = collections.Counter()
source_totals = collections.Counter()
source_counts = collections.Counter()
instantiate_totals = collections.Counter()
frontend_tot = 0
backend_tot = 0
total_wall = 0

for path in trace_files:
    try:
        with open(path, "r", encoding="utf-8") as f:
            data = json.load(f)
    except Exception as e:
        continue
    events = data.get("traceEvents", [])
    exec_dur = 0
    frontend = 0
    backend = 0
    top_reads = collections.Counter()
    top_sources = collections.Counter()
    for e in events:
        if e.get("ph") != "X":
            continue
        name = e.get("name", "")
        dur = e.get("dur", 0)
        detail = e.get("args", {}).get("detail", "")
        if name == "ExecuteCompiler":
            exec_dur = max(exec_dur, dur)
        elif name == "Frontend":
            frontend += dur
        elif name == "Backend":
            backend += dur
        elif name == "Read Loaded AST":
            read_ast_totals[detail] += dur
            top_reads[detail] += dur
        elif name == "Source":
            source_totals[detail] += dur
            source_counts[detail] += 1
            top_sources[detail] += dur
        elif name == "InstantiateFunction" or name == "InstantiateClass":
            instantiate_totals[detail] += dur

    rel = os.path.relpath(path, BUILD).replace("\\", "/")
    per_tu.append({
        "tu": rel,
        "exec_us": exec_dur,
        "frontend_us": frontend,
        "backend_us": backend,
        "top_reads": top_reads.most_common(5),
        "top_sources": top_sources.most_common(5),
    })
    frontend_tot += frontend
    backend_tot += backend
    total_wall += exec_dur

per_tu.sort(key=lambda x: x["exec_us"], reverse=True)

def us_to_s(x): return f"{x/1_000_000:6.2f}s"

print(f"=== Summary: {len(per_tu)} TUs ===")
print(f"Sum of compiler wall time (serial equivalent): {us_to_s(total_wall)}")
print(f"  Frontend: {us_to_s(frontend_tot)}  ({100*frontend_tot/max(1,frontend_tot+backend_tot):.0f}%)")
print(f"  Backend:  {us_to_s(backend_tot)}  ({100*backend_tot/max(1,frontend_tot+backend_tot):.0f}%)")
print()

print("=== Top 30 TUs by total compile time ===")
print(f"{'time':>8}  {'front':>7}  {'back':>7}  TU")
for row in per_tu[:30]:
    print(f"{us_to_s(row['exec_us'])}  {us_to_s(row['frontend_us'])}  {us_to_s(row['backend_us'])}  {row['tu']}")
print()

print("=== Top 30 imported modules by cumulative 'Read Loaded AST' time (summed across all TUs) ===")
print("(this is a proxy for 'how expensive is it when this module is imported')")
for mod, t in read_ast_totals.most_common(30):
    print(f"  {us_to_s(t)}  {mod!r}")
print()

print("=== Top 30 header Sources by cumulative parse time ===")
for src, t in source_totals.most_common(30):
    short = src.replace("\\", "/").rsplit("/", 3)
    short_path = "/".join(short[-3:]) if len(short) >= 3 else src
    print(f"  {us_to_s(t)}  x{source_counts[src]:<4}  {short_path}")
print()

print("=== Top 20 template instantiations by cumulative time ===")
for name, t in instantiate_totals.most_common(20):
    print(f"  {us_to_s(t)}  {name[:120]}")
