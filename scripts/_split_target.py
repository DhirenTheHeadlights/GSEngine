import json, glob, os
BUILD = "out/build/x64-clang-p2996-libcxx-Release-trace"
totals = {"Engine":0, "Game":0, "std":0, "VulkanModule":0, "other":0}
for p in (glob.glob(os.path.join(BUILD, "**", "*.cppm.json"), recursive=True) +
          glob.glob(os.path.join(BUILD, "**", "*.cpp.json"), recursive=True)):
    if "CompilerIdCXX" in p: continue
    try: d = json.load(open(p))
    except: continue
    dur = 0
    for e in d.get("traceEvents", []):
        if e.get("ph")=="X" and e.get("name")=="ExecuteCompiler":
            dur = max(dur, e.get("dur",0))
    rel = os.path.relpath(p, BUILD).replace("\\","/")
    if "VulkanModule.dir" in rel: totals["VulkanModule"] += dur
    elif "__cmake_cxx" in rel: totals["std"] += dur
    elif "Engine.dir" in rel: totals["Engine"] += dur
    elif "GoonSquad" in rel: totals["Game"] += dur
    else: totals["other"] += dur
total = sum(totals.values())
for k,v in sorted(totals.items(), key=lambda x:-x[1]):
    print(f"  {k:15s}  {v/1_000_000:6.2f}s  {100*v/total:5.1f}%")
print(f"  {'TOTAL':15s}  {total/1_000_000:6.2f}s")
