# Runtime-configurable VBD limits — scope

Written 2026-09-08 after the 512-env locomotion run tripped `max_motors = 1024`
(`Physics/VBD/GpuUpload.cpp:135`). The limit was raised to 4096 as a stopgap;
this note scopes making the capacities settable without a rebuild.

## What the limits are today

`gse::vbd::vbd_limits` (`Physics/VBD/Constraints.cppm:12-88`) is a
`[[= shaders::shader_constant_block]]` struct with ~70 `uint32_t` members, and
`constexpr vbd_limits limits{};` at line 89 is the only instance. Every use is a
constant expression, on both sides of the GPU boundary:

- **C++**: buffer sizes in `gpu_solver::create_buffers` (`GpuSolver.cpp:333-700`),
  `reserve`/`assign` of the upload vectors (`:725-728, 802, 818, 905, 927`),
  asserts (`GpuSolver.cpp:740-750, 920`, `GpuUpload.cpp:135-186`,
  `System.cpp:395`), dispatch math (`:1362-1392, 1693-1721`), and three push
  constants that already ship a limit to the GPU at runtime
  (`:1292 contact_count = max_contacts`, `:1770, 1879 max_colors`).
- **Shaders**: `emit_slang_constants<T>()` (`Gpu/Shader/ShaderCodegen.cppm:355`)
  reflects over the members of a *default-constructed* `T{}` and emits
  `public static const uint <name> = <literal>;` into the generated wrapper
  source (`GpuRecord/PipelineBuilder.cpp:305-345`). Shaders are compiled from
  that string at pipeline-build time in `initialize_compute`
  (`GpuSolver.cpp:1186`), which also calls `create_buffers` once; nothing is
  ever rebuilt or resized afterwards.

The members fall into two classes that must be treated differently:

| class | members | runtime? |
|---|---|---|
| capacities / tuning | `max_bodies`, `max_contacts`, `max_collision_pairs`, `max_joints`, `max_islands`, `max_impulses`, `max_motors`, `grid_table_size`, `grid_max_entries`, `max_colors`, `coloring_rounds`, `sleep_threshold`, `solve_sweep_workgroups`, `sweep_spin_*`, `max_narrow_phase_debug_records`, `iteration_trace_slots`, plus the derived `max_contact_adjacency`, `max_joint_adjacency`, `max_grounded_uints` | yes |
| layout / protocol | `state_*_index`, `*_uints`, `feature_*`, `sat_axis_*`, `shape_*`, `solve_state_float4s_per_body`, `workgroup_size`, `adjacency_workgroup_size` | no — C++/shader ABI |

## Hard compile-time sites (the actual blockers)

1. **`gpu::threads<limits.workgroup_size>` / `<limits.adjacency_workgroup_size>`**
   (`GpuSolver.cpp:183-294`, ten entries). Non-type template arguments baked into
   `compute_entry_pod` at `consteval` (`PipelineBuilder.cppm:380`) and emitted as
   the `[numthreads(...)]` literal (`PipelineBuilder.cpp:355`). Keep these
   compile-time; they are tuning for the kernel, not scene capacity.
2. **`std::array<uint32_t, limits.max_colors>`** (`GpuSolver.cppm:68`,
   `GpuSolver.cpp:1072`) and **`std::array<uint32_t, limits.max_grounded_uints>`**
   on the stack in `System.cpp:1510` (640 uints today, scales with `max_bodies`).
3. **`constexpr` size expressions** — `GpuSolver.cpp:338-346, 517, 926, 1365,
   1541-1543, 1975-1976`. Mechanical: drop `constexpr`.
4. **Shader `groupshared` arrays sized by limits** —
   `collision_build_coloring.slang:4-6, 12-14` (`gs_color_count[max_colors]`,
   `gs_wave_color_count[max_waves_per_group * max_colors]`). Group-shared extents
   must be link-time constants, so `max_colors` cannot be a plain runtime value
   unless the shader source is generated with the runtime value (route A below)
   or a spec constant is proven to work for groupshared sizing under Slang.

## Design: fix the values at solver init, not per tick

Because pipelines and buffers are created exactly once in `initialize_compute`,
"runtime" only needs to mean "chosen before the solver initialises". That
matches the existing `settings::restart_required{}` precedent on
`Physics.use_gpu_solver` (`Physics/System.cppm:208`) and rides the existing
override chain (`%APPDATA%/GSE/<Exe>.ini` → `<Project>/Config/settings.ini` →
scenario `settings` → `--engine-setting Physics.<key>=<value>`). No spec
constants, no live resize.

### Route A (recommended): generate shader constants from the runtime instance

1. Split `vbd_limits` into `vbd_layout` (protocol constants, stays
   `constexpr`, stays a `shader_constant_block`) and `vbd_capacities` (the
   runtime class above, plain struct with the same defaults). Derived members
   (`max_contact_adjacency`, `max_grounded_uints`, `grid_max_entries`) become a
   `derive()` step run once from the chosen values.
2. Add a `shader_runtime_constant_block` emission path: same reflection loop as
   `emit_slang_constants<T>()` but taking `const T&` instead of `T{}`. Plumb it
   through `shader_compile_inputs` as a `std::function<std::string()>` or a
   `(const void*) -> std::string` thunk plus a pointer set on the pod copy at
   build time (`build_compute_program(dev, pod, spec_data)` gains an optional
   `runtime_constants` argument; `PipelineBuilder.cpp:315` appends it after
   `emit_types`). Slang sees the same `public static const uint` lines, so all
   `.slang` files, including the `groupshared` sizes, compile unchanged.
3. `gpu_solver` gets a `vbd_capacities m_caps` member set from the Physics
   settings before `initialize_compute`; `create_buffers`, the asserts, the
   `reserve`/`assign` calls and the dispatch math read `m_caps` instead of
   `limits`. The two `std::array<…, max_colors>` become `std::vector` (or keep a
   compile-time *ceiling* `max_colors_ceiling = 16` for the array and clamp the
   runtime value to it — cheaper and `max_colors` is already ranged −1..16 in
   `gpu_color_cap`). `System.cpp:1510` uses a `std::vector` sized once.
4. Settings surface on `Physics::data` (`System.cppm:202`), all
   `restart_required`, with `settings::range` and `describe`:
   `gpu_max_bodies` (20480), `gpu_max_contacts` (262144),
   `gpu_max_collision_pairs` (262144), `gpu_max_joints` (8192),
   `gpu_max_islands` (512), `gpu_max_impulses` (4096), `gpu_max_motors` (4096),
   `gpu_grid_table_size` (32768). Everything else stays at its default; the
   locomotion trainer passes `--engine-setting Physics.gpu_max_motors=…` etc.,
   or better, the physics system derives motors/joints/bodies from the scene at
   init when the setting is 0 ("auto"): the trainer already knows its env
   count before the solver initialises, and `System.cpp:395` already counts
   bodies.
5. The rollback ring's separate caps (`GpuSolver.cppm:192-193`,
   `ring_body_capacity = 4096`, `ring_contact_capacity = 16384`) should be folded
   into the same struct while here; today an oversized scene silently disables
   the ring (`GpuSolverRing.cpp:95-98`).

Cost: one new emission thunk in the shader codegen, a struct split, a mechanical
`limits.` → `m_caps.` sweep over ~60 sites in `GpuSolver.cpp`/`GpuUpload.cpp`/
`System.cpp`, and the settings entries. Roughly a day including the determinism
re-check: the CPU smoke hash must stay `623ae8f24ba19aa2` and the GPU sync hash
`ee5613986ee8e0b1` at the default values, since only literals in the generated
source change.

### Route B: specialization constants

`emit_slang_specialization_constants<T>()` / `build_spec_constant_entries<T>()` /
`gpu::spec_constants<T>` / `as_spec_data` already exist
(`ShaderCodegen.cppm:167-406`, `PipelineBuilder.cppm:150-166, 394-403`) but have
zero call sites in the tree. It would be the first user, it needs the DX12
backend's spec-constant path verified as well as Vulkan's, and the
`groupshared` arrays in `collision_build_coloring.slang` are the open question.
Not worth it when the shader source is generated at init anyway; the only win
over route A is pipeline-cache reuse across different capacities, which
nothing needs.

### Route C: grow on demand

Re-create buffers when an upload exceeds capacity. Rejected: the resident
per-frame state (joint duals, warm-start λ, contact history, sleep counters)
would need a copy-and-migrate on every grow, and the trainer's env count is
known at startup, so there is no case that needs it.

## Out of scope

- `workgroup_size` / `adjacency_workgroup_size` stay compile-time (numthreads).
- The layout/protocol constants stay a `constexpr` `shader_constant_block`.
- The other `shader_constant_block`s (`cloud_limits`, `gi_probe_limits`,
  `light_culling_limits`, `exposure_limits`, `trace_quality_limits`) would
  benefit from the same runtime emission path once it exists, but are not
  part of this change.

## Review against `docs/CODE_REVIEW_GUIDE.md` (2026-09-08) and what was built

Route A stands. Four amendments, each from a review-guide rule:

1. **`max_colors` and `iteration_trace_slots` are layout, not capacity.** The
   state header bakes both in: colour populations occupy indices 59..74 (16
   slots) and the trace bases are `81 + 64 * 3`. Moving them out of the
   compile-time block would leave the header indices as a second, divergent
   source of truth (paired derivations). Keeping them compile-time also
   removes blocker 4 outright: the `groupshared` arrays and the two
   `std::array<…, max_colors>` stay as they are. Tuning knobs
   (`coloring_rounds`, `sleep_threshold`, `sweep_spin_*`,
   `solve_sweep_workgroups`, `max_narrow_phase_debug_records`) also stay
   compile-time; none of them is scene-size and every new setting is a new
   state to justify.
2. **No thunk, no `void*`, no `derive()`.** `compute_entry_pod` is a
   `consteval` constant and cannot carry a runtime pointer. The runtime block
   is rendered once by a `const T&` overload of `emit_slang_constants` and
   handed to `build_compute_program` as a `std::string_view`, the same shape
   as the existing `spec_data` span. The constant-block overload now delegates
   to it, so there is one emission loop. Derived members keep their
   default-member-initializer form: aggregate initialisation evaluates them
   from the already-set primaries, so a designated-init from settings yields
   correct derived values with no second phase and no representable
   half-initialised state.
3. **No "0 = auto".** `physics::init` runs before the scene exists, so sizing
   from the scene would mean deferring pipeline creation to the first upload
   and a new state machine around `buffers_created`. Explicit settings only;
   the trainer passes `--engine-setting Physics.gpu_max_motors=…`.
4. **The ring caps join the struct as their own members** (`ring_max_bodies`,
   `ring_max_contacts`) rather than being tied to `max_bodies`; tying them
   would multiply ring memory by five at the defaults.

Also dropped: the `max_bodies` assert in `build_body_states`, which sits on
the CPU path too and duplicated the check `gpu_solver::upload` already makes.

Shape as landed: `vbd_capacities` (plain struct, not a `shader_constant_block`,
not in `shader_types`) next to the slimmed `vbd_limits`; `gpu_solver`
receives it in `initialize_compute`, keeps it as `m_capacities`, exposes
`capacities()`; `physics::capacities_from_settings` mirrors
`solver_config_from_settings`; ten `Physics.gpu_*` settings, all
`restart_required`. The generated shader source is byte-identical at the
defaults, so the determinism hashes above must be unchanged.

## Immediate stopgap already applied

`max_motors` 1024 → 4096 (`Constraints.cppm:20`); sizes one upload buffer of
`max_motors * sizeof(velocity_motor_constraint)`, negligible. 512 envs need
1536; 1024 envs would need 3072.
