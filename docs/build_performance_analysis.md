# GCC Module Build Performance Notes

Date: 2026-06-06

This summarizes the GCC profile build and module import traces taken after the partition implementation-unit pass.

## Baseline

Clean profile build:

```text
cmake --build --preset x64-mingw-gcc-Profile --target GoonSquad --clean-first
```

Result:

```text
wall time: 461.8s
compiled TUs: 385
serial compile wall: 2401.2s
```

Main GCC timing buckets:

```text
phase parsing:             1495.4s
module import:             1309.9s
template instantiation:     844.8s
phase opt and generate:     656.2s
```

The previous serial compile sum was about 2755s, so converting primary-module implementation units to partition implementation units saved roughly 13% of serial compile work. The wall clock stayed close to the old clean build because the remaining cost is concentrated on dependency chains and repeated imports.

## Biggest Import Costs

These were measured with import-only temporary TUs against a copied build snapshot, using `-fno-module-lazy` so GCC actually loads the module CMIs.

```text
gse.runtime      5.56s/import
gse.examples     5.54s/import
gse.graphics     5.48s/import
gse.physics      5.20s/import
gse.network      5.09s/import
gse.gpu          4.97s/import
gse.vulkan       4.37s/import
gse.os           2.91s/import
gse.glfw         2.51s/import
```

Weighted by direct fan-in, the practical repeated-import offenders are:

```text
gse.gpu          4.97s x 82 imports = 407.5
gse.os           2.91s x 67 imports = 195.0
gse.concurrency  0.78s x 141 imports = 110.0
gse.core         0.48s x 214 imports = 102.7
gse.ecs          0.82s x 118 imports = 96.8
```

`gse.gpu` and `gse.os` are the important ones because they have large per-import cost and high fan-in. The others are widely used foundations with smaller per-import cost.

## gse.gpu

The primary issue is `gse.gpu:aliases`.

Source-order cumulative trace for `gse.gpu` partitions:

```text
+ gse.gpu:aliases          +4.50s  total 4.50s
+ gse.gpu:render_graph     +0.29s  total 4.83s
+ gse.gpu:shader_codegen   +0.16s  total 5.02s
+ gse.gpu:pipeline_builder +0.09s  total 5.03s
```

Everything else is noise once `gse.gpu:aliases` is loaded.

`gse.gpu:aliases` imports `gse.vulkan`, then aliases many `vulkan::*` types into `gse::gpu`. Since nearly every GPU partition imports `:aliases`, most GPU imports pay the Vulkan CMI cost.

Important observed costs:

```text
gse.gpu:aliases      4.60s/import
gse.vulkan           4.37s/import
gse.gpu              4.97s/import
```

Recommended cut:

Split `gse.gpu:aliases` into a light aliases/types partition that does not import `gse.vulkan`, plus a heavy Vulkan-backed aliases partition for the code that truly needs backend concrete types.

## gse.os

The `gse.os` import cost has the same shape, but smaller.

Cumulative trace:

```text
+ gse.os:app       +0.40s  total 0.40s
+ gse.os:window    +2.69s  total 3.09s
```

Everything after `:window` barely moves the cumulative total.

`gse.os:window` imports both `vulkan` and `gse.glfw`. `gse.glfw` also imports `vulkan`.

Recommended cut:

Split light window data/events/settings from GLFW/Vulkan backend details. Also consider splitting `gse.glfw` into a core GLFW declaration module and a Vulkan-specific GLFW interop module.

## Raw Compile Buckets

The biggest root compile bucket is still `gse.graphics`:

```text
gse.graphics  1045.5s total  624.0s interface  421.5s impl
gse.vulkan     216.6s total  198.6s interface   18.0s impl
gse.gpu        166.3s total  107.5s interface   58.8s impl
gse.physics    133.7s total   86.4s interface   47.3s impl
gse.runtime     99.0s total   38.6s interface   60.5s impl
```

This means `gse.gpu` and `gse.os` are the best repeated-import fixes, while `gse.graphics` remains the largest raw compile-time area to revisit after the backend import leaks are reduced.

## Suggested Order

1. Split `gse.gpu:aliases` so most GPU users do not import `gse.vulkan`.
2. Split `gse.os:window` and `gse.glfw` so most OS users do not import Vulkan/GLFW backend surface.
3. Rerun a clean `x64-mingw-gcc-Profile` build and the non-lazy import traces.
4. If import cost drops as expected, inspect `gse.graphics` interface structure next.

## Trace Artifacts

The analysis used a copied snapshot so the main tree could keep changing:

```text
C:\tmp\gse_module_trace_snapshot_20260606_071857
```

Useful outputs:

```text
C:\tmp\gse_module_trace_snapshot_20260606_071857\trace_run_nonlazy\aggregate.csv
C:\tmp\gse_module_trace_snapshot_20260606_071857\gpu_partition_trace2_source_order\cumulative_source_order.csv
C:\tmp\gse_module_trace_snapshot_20260606_071857\os_partition_trace\cumulative.csv
```
