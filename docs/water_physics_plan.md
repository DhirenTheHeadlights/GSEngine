# Water Physics — hybrid PBF + heightfield rollout plan

## Status

Planning draft. Parked to revisit later.

This doc scopes physically-based water for GSEngine: rain that pools across terrain
and affects player movement, with rich local interaction (splashes, wakes, floating
bodies). It records the two architecture decisions already made, what the engine gives
us for free vs. what is greenfield, the target architecture, a phased roadmap, and the
known landmines. Read alongside `docs/rendering_plan.md` and `docs/restir_plan.md`.

## The goal

Physically-based **rain → pooling → player effect**, end to end:

- Rain falls and accumulates in terrain concavities (pools, puddles, flooded areas).
- Pooled water is physically based, not a scripted decal or a flat plane.
- The player wades / swims — water exerts drag, buoyancy, and a swim state.
- Rigid/soft bodies float, bob, and displace water (two-way coupling).

## Decisions made

Two forks were settled up front; they define the whole shape below.

| Fork | Choice | Implication |
|---|---|---|
| Coupling | **Two-way with VBD bodies** | Water and bodies exchange forces; player leaves wakes, crates float and displace water. Deepest solver integration. |
| Scale | **Terrain-wide / rain-fed (hybrid)** | Water accumulates world-wide. Pure PBF would overflow a fixed particle budget, so pair a cheap heightfield (bulk) with PBF particles (lively bits). |

## Why PBF (and why hybrid)

**PBF** (Position Based Fluids, Macklin & Müller 2013) was chosen over a pure heightfield
because it is in the **same position-based family as the existing VBD solver**: predict
positions → iterate constraint projection ×N → recover velocity from the position delta.
Incompressibility is just a different constraint type (a per-particle density constraint
with a Lagrange multiplier) bolted onto machinery we already have. The GPU dispatch loop,
bindless buffers, headless submit, and readback pattern all transfer.

**Hybrid** is forced by the terrain-wide + rain choice. Rain is a continuous source; PBF
has a fixed particle budget. Pure PBF overflows in seconds unless particles are recycled.
The resolution:

- **Heightfield** (cheap, terrain-wide) holds the *bulk* pooled water — where rain
  accumulates, where you wade. Most of the water lives here.
- **PBF particles** (expensive, local) exist only near the player / where it is lively —
  splashes, wakes, droplets, rich VBD interaction. Bounded budget.
- **Absorb / emit coupling** keeps the budget bounded: settled particles are absorbed into
  the heightfield (delete particle → add its volume to the cell); disturbed water (ledges,
  rain impacts, player vicinity) emits particles from the heightfield.

## What the engine gives us

The *plumbing* is excellent and battle-tested in the VBD code. The water *domain* is
entirely new.

### Free (proven in VBD / renderer)

| Capability | Where | Notes |
|---|---|---|
| Compute pass recording | `gpu::pass<Stage>(ctx).on(compute).in_chain<>()`, `RenderGraph.cppm` | Mirror VBD solver shape directly. |
| Typed dispatch | `rec.dispatch<Entry>(pc, bindings, groups)`, `rec.dispatch_indirect(...)` | `GpuSolver.cpp` |
| Automatic barriers | `recording_context::emit_intra_pass_barrier` + `append_prev_pass_barriers` | There is no manual barrier API — `rec.barrier`/`gpu::barrier_scope` were deleted 2026-07-28. Barriers are derived from bindless access; see `render_graph_bindless_barriers_plan.md`. |
| Bindless buffers + readback | `create_buffer({.bindless=true})`, `.slot()`, double-buffered `host_read()` | Same pattern VBD uses. |
| **Headless compute submit** | `physics::frame()` gates on `use_gpu_solver`, awaits `dispatch_compute()`, `System.cpp:1325` | Landed in Stage 3.0d (2026-06-16). The PBF prove-out rides this exact path. |
| Spatial-hash grid build | VBD broad phase `collision_grid_build_pipeline` | Neighbor-grid concept already lives in the codebase. |
| Two-way coupling seam | `impulse_request {id, vec3<impulse>}`, `MotionComponent.cppm` | Fluid → bodies via impulse channel; bodies → fluid via AABB+shape on `body_state` (`Constraints.cppm:158`). |
| Debug draw | instanced unit spheres, `PhysicsDebugRenderer.cppm` | Particle viz before real rendering. |
| Sky / clouds (wind) | `AtmosphereRenderer.cppm`, `CloudRenderer.cppm` | Rain source reads cloud coverage; no rain emitter yet. |
| Annotated-system + slang entries | `SystemAnno.cppm`, `compute_entry<>`/`graphics_entry<>`, `build_compute_program` | Slot a `gse::water` system in cleanly (mirror `CameraSystem.cpp`, `TonemapRenderer.cpp`). |
| Determinism harness | `--physics-parity` | Ready-made determinism check for the prove-out. |

### Greenfield (build all of it)

| Missing | Consequence |
|---|---|
| Terrain / heightfield | The bulk-water layer is from scratch. |
| Particle system | PBF particles, neighbor grid, all the solver passes. |
| Collision SDF | Only OBB/SAT `query_obb` (box/sphere/capsule), `NarrowPhaseCollisions.cppm`. Per-particle OBB queries are expensive; terrain-scale realistically wants a baked SDF volume. |
| Rain emitter | New compute system; can be driven by existing cloud coverage. |

## Architecture

Two coupled water representations plus a rain source, all GPU-resident:

```
        rain emitter (driven by cloud coverage)
                 │  add volume + spawn splash particles
                 ▼
        ┌─────────────────────┐   emit (ledges, impacts,
        │  heightfield grid    │   player vicinity)
        │  (shallow water,     │ ───────────────────────►  ┌──────────────────┐
        │   terrain-wide bulk) │ ◄───────────────────────  │  PBF particles    │
        └─────────┬───────────┘   absorb (settled particles│  (local, lively)  │
                  │                 → add volume to cell)   └────────┬─────────┘
       sample depth│                                                  │ impulse_request
                  ▼                                                   ▼  + body boundary
            player movement                                    VBD rigid/soft bodies
       (drag / buoyancy / swim)                              (float, bob, displace water)
```

- **Heightfield**: a 2D shallow-water / virtual-pipes grid `h(x,z)` over terrain. GPU
  stencil compute. Cheap; this is where rain accumulates and where the player wades.
- **PBF particles**: GPU particle solver for the lively layer; bounded budget.
- **Two-way coupling**: particles push bodies via the `impulse_request` channel; bodies
  act as moving boundaries for particles (use `body_state` AABB + collision shape). The
  player is a body too — it both samples local water and displaces it.

## The PBF step (per tick)

The core loop, ~10–20 sequential compute dispatches, each writing bindless buffers the
next reads:

1. Apply gravity, predict `x* = x + dt·v`.
2. Build neighbor grid from predicted positions (counting-sort into cells).
3. Solver loop (~3–5 iters):
   - density `ρ` via SPH poly6 kernel,
   - constraint `C = ρ/ρ₀ − 1`, Lagrange multiplier `λ`,
   - position correction `Δp` (incl. the `s_corr` artificial-pressure term that kills clumping),
   - boundary / collision handling.
4. Velocity `v = (x* − x)/dt`.
5. XSPH viscosity + vorticity confinement.
6. Commit `x = x*`; absorb/emit against heightfield; emit impulses to coupled bodies.

## Roadmap

Risk is front-loaded. Each phase is independently validatable.

| # | Phase | Proves |
|---|---|---|
| **0** | **Tank prove-out** — pure PBF in a box, debug spheres, headless + deterministic | The ~15-pass barrier dance + neighbor grid + solver stability, in isolation |
| 1 | World collision — particles vs OBB world (or baked SDF) | Water pools in a basin |
| 2 | Two-way bodies — crate floats/bobs, displaces water | The impulse-channel ↔ body-boundary loop |
| 3 | Player movement — wade/swim drag/buoyancy; player displaces water | "Affects player movement" |
| 4 | Heightfield + hybrid coupling — bulk terrain-wide water; absorb/emit | Terrain-wide coverage at budget |
| 5 | Rain — compute emitter driven by cloud coverage → heightfield fill + splash particles | Closes the original loop |
| 6 | Rendering — screen-space fluid (depth splat → bilateral blur → normals → refract/reflect) + transparent heightfield surface + wetness materials | The look; deferred because it is orthogonal to correctness |

### Phase 0 detail (the recommended starting point)

A `gse::water` system + a handful of slang compute passes + debug-sphere viz, runnable
headless and checked against `--physics-parity`. Scope:

- particle buffer (bindless), neighbor-grid build (model on VBD `collision_grid_build`),
- the PBF passes from "The PBF step" above with explicit `compute_to_compute` barriers,
- debug-sphere viz via `PhysicsDebugRenderer` infrastructure,
- validate: water settles to a flat surface at rest density, stays incompressible
  (no explode / clump), deterministic run-to-run, headless submit works.

No bodies, no heightfield, no rain. This de-risks the hardest technical piece before any
gameplay or rendering investment.

## Landmines (VBD scar tissue confirms these)

- **The barrier dance** — ~15 RAW-dependent compute passes per step. This is now automatic in
  both directions (cross-pass and intra-pass) and there is no manual escape hatch, but it is
  only as good as the `Entry` binding declarations: a member the shader writes must be
  `read_write`, and a bindless resource must be reachable via `device::resource_for_slot`
  (i.e. created through `create_buffer`/`create_image` `.bindless` or bound via
  `write_storage_buffer`/`write_sampled_image`). A resource that fails either condition gets
  **no barrier and no diagnostic** — that is now the highest correctness risk, in place of the
  old forgot-to-write-it risk.
- **Push-constant budget** — 256 B. PBF config goes in an SSBO, not push data (same lesson
  as `solver_config`).
- **Determinism** — atomic-scatter neighbor grids are non-deterministic. For a VBD-style
  parity guarantee the particle layer needs sorted grids, or accept non-determinism for the
  particle layer while keeping the heightfield deterministic.
- **OBB-only collision** — terrain-scale fluid realistically wants a baked SDF; budget for it.
- **Units** — `body_state` is unit-typed even on GPU; fluid math must respect meters/kg/s.

## Open questions (resolve before / during build)

- Particle budget number (governs grid size, domain extent, perf headroom alongside RT + any RL).
- Fluid-vs-world: per-particle `query_obb` vs. a baked SDF volume — and when to switch.
- Determinism stance for the particle layer (sorted grid vs. accept non-determinism).
- Heightfield resolution + draining model (evaporation / absorption / sinks).
- Rendering approach detail: screen-space fluid vs. surface mesh extraction.

## Recommendation

Start at **Phase 0**. If the tank prove-out falls out clean — incompressible, stable,
deterministic, submitting headless — the rest is incremental. Everything hard (plumbing,
barriers, neighbor grid, solver stability) is proven there before a day is spent on
gameplay or rendering.
