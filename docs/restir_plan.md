# ReSTIR — phased rollout plan

## What this is

A staged plan for adopting **ReSTIR** (reservoir spatiotemporal importance resampling) in GSEngine. ReSTIR is a statistical technique that lets one ray per pixel approximate the quality of dozens by reusing samples across screen-space neighbors and previous frames through small per-pixel reservoirs.

The thesis: don't trace more, **resample better**. With ReSTIR DI, 1 light ray/pixel gives the look of ~64 raw samples. With ReSTIR GI, full-resolution indirect diffuse replaces probe grids. With ReSTIR PT, real-time path-traced reference quality becomes achievable.

This doc scopes three phases (DI → GI → PT), prerequisites, considerations, and decision points. Read alongside `docs/rendering_plan.md`.

---

## Why now

The codebase is unusually well-positioned for ReSTIR:

| Prerequisite | Status |
|---|---|
| TLAS rebuilt per frame, custom_index = material palette index | ✅ Shipped |
| Inline RayQuery in compute and fragment shaders | ✅ Shipped |
| Motion-vector buffer (velocity from MRT depth prepass) | ✅ Shipped |
| Cross-frame history image infrastructure (TAA's ping-pong pattern) | ✅ Shipped |
| TAA temporal stabilization downstream of any new RT signal | ✅ Shipped |
| Material palette as a globally-bound SSBO with stable instance indices | ✅ Shipped (GeometryCollector owns it) |
| Per-frame buffer + descriptor pattern (`per_frame_resource<gpu::buffer>`) | ✅ Shipped |
| Render graph data-flow tracking + explicit `.after<T>` ordering | ✅ Shipped |

The only thing we lack is **per-pixel persistent storage**. That's just a new framebuffer image / SSBO at swap-chain extent.

Existing techniques that ReSTIR variants would partially or fully replace:

| Existing | ReSTIR replacement | Phase |
|---|---|---|
| `LightCullingRenderer` (16×16 forward+ tiles) | ReSTIR DI selects one light per pixel from any count | Phase 1 |
| Per-pixel inline RT shadow loop in `meshlet_geometry.slang` | ReSTIR DI handles light pick → one shadow ray, denoised | Phase 1 |
| DDGI probe-based indirect diffuse | ReSTIR GI for full-resolution indirect | Phase 2 |
| RT reflections (per-pixel inline) | Subsumable into ReSTIR PT eventually | Phase 3 |

ReSTIR DI and DDGI/probes can **coexist** as a quality preset choice. ReSTIR GI competes directly with DDGI.

---

## Phase 0 — Prerequisites and infrastructure

Two pieces of infrastructure need to land before any ReSTIR variant ships.

### 0.1 A denoiser

ReSTIR outputs are still noisy at 1 spp; the temporal+spatial reuse cuts variance dramatically but doesn't eliminate it. Production ReSTIR pairs with a denoiser (Intel OIDN, NVIDIA NRD, or a custom A-trous edge-aware blur).

**Recommendation**: integrate NVIDIA Real-Time Denoisers (NRD) or Intel OIDN early — they're libraries, not research. NRD is purpose-built for ReSTIR-class signals and has presets (ReBLUR, ReLAX, SIGMA). Ships as a Vulkan-compatible static lib.

Estimate: 1–2 weeks integration work for NRD.

Without a denoiser, ReSTIR DI looks like grainy hard shadows; with one, it looks like clean soft shadows.

### 0.2 Per-pixel reservoir storage

A reservoir is roughly 32–48 bytes. At 1920×1080 that's ~75–100 MB at the high end if we keep multiple variants. Need:

- `per_frame_resource<gpu::image>` of `r32g32b32a32_uint` (or a custom packed format) at swap-chain extent, for ping-pong.
- Compute write + sampled read both required → `image_flag::storage | image_flag::sampled`.
- Optionally: an SSBO laid out the same way, since reservoir math is integer-heavy and not natural for texture sampling. Choice depends on cost of integer texture loads vs SSBO addressing on the target hardware.

This pattern mirrors what TAA does for history color.

### 0.3 Reading material

These three papers, in order, are the canonical reference:

1. **Bitterli et al. 2020**, *Spatiotemporal reservoir resampling for real-time ray tracing with dynamic direct lighting* — ReSTIR DI, foundational. Unusually readable.
2. **Ouyang et al. 2021**, *ReSTIR GI: Path Resampling for Real-Time Path Tracing* — extends to indirect.
3. **Lin et al. 2022**, *Generalized Resampled Importance Sampling: Foundations of ReSTIR* (GRIS) — unified math framework. Read after the others as the "why" behind the formulas.

NVIDIA Falcor has reference implementations of all three. RTXDI is the standalone library for ReSTIR DI.

---

## Phase 1 — ReSTIR DI

**Goal**: replace the forward+ tile light culling and the inline per-light shadow loop with reservoir-based light selection.

### Algorithm sketch

Per-pixel, per-frame:

1. **Generate** M candidate lights (e.g., M=32) using a cheap PDF (uniform over all lights, or importance-sampled by power). Each candidate has a target weight = unshadowed BRDF × radiance × geometry term.
2. **Resample** down to N=1 via weighted reservoir sampling — pick one of the M weighted by target.
3. **Visibility test**: one shadow ray to the chosen sample. If occluded, weight → 0.
4. **Temporal reuse**: reproject last frame's pixel via motion vector, fetch its reservoir, merge with current via reservoir union.
5. **Spatial reuse**: K=3–5 neighbors at random radii (Poisson-disk), merge their reservoirs into current.
6. **Final shade**: BRDF × selected light's contribution.

Bias correction terms appear at every merge step — MIS weights between the source domains. The 2020 paper has explicit formulas; Falcor has them in code.

### What it touches

| File | Change |
|---|---|
| New: `RestirDiRenderer.cppm/.cpp` | Compute pipeline + reservoir SSBOs/images + per-frame descriptors |
| New: `Compute/restir_di_initial.slang` | Generate-and-resample initial M candidates → reservoir |
| New: `Compute/restir_di_temporal.slang` | Merge with reprojected previous reservoir |
| New: `Compute/restir_di_spatial.slang` | Merge with K spatial neighbors |
| New: `Compute/restir_di_shade.slang` | Read final reservoir, shade pixel, write to HDR |
| `ForwardRenderer.cpp` + `meshlet_geometry.slang` | Strip the `for (uint t = 0; t < tile_count; ++t)` light loop and per-light shadow trace; instead sample a `restir_color` framebuffer image |
| `LightCullingRenderer` | Optional: keep as a fallback for the "off" quality preset; otherwise remove |
| `RenderTargets.cppm` | New `targets::restir_di_color` framebuffer image |
| `Engine.cpp` + `Graphics.cppm` | Register and export |

**Estimated LOC**: ~800 across all files, ~250 of that being shader code. One PR-sized chunk if disciplined.

### Considerations

- **Per-light reservoir vs single-sample reservoir**: the paper uses single-sample (N=1). RIS quality with M=32 is ample for direct lighting in most scenes. Multi-sample reservoirs (N>1) are a tuning lever for hard cases.
- **Light data structure**: ReSTIR DI handles millions of lights. We currently cap at 1024. If we want to actually exploit ReSTIR's main strength, the light list should become a flat SSBO with no upper bound (or much higher, e.g., 16K). That's a small change to `light_buffers` sizing.
- **Importance sampling for initial candidates**: uniform sampling works but is wasteful. CDF-by-power or grid-based light hierarchies (BVH over lights) is the production move. Phase 1 ships uniform; CDF-by-power is a Phase 1.5 follow-up.
- **Bias mode**: the paper offers "biased" (visibility evaluated only once, neighbors trusted) and "unbiased" (visibility checked at merge time, expensive). Bias mode is ~3× faster, almost imperceptible artifacts in practice. Ship biased.
- **GI fallback**: ReSTIR DI handles direct only. Indirect still comes from DDGI (or eventually Phase 2). The forward shader needs to compose both.

### Pairing with denoiser

The ReSTIR DI output is the per-pixel direct lighting result. Denoise it with NRD's **ReBLUR** preset (designed for diffuse/specular separate signals) before composition. TAA further smooths what NRD can't catch.

### Risks

- **Reprojection failures**: when motion vectors are wrong (occluded → revealed pixels), temporal reuse poisons quality. Heuristics: depth + normal similarity tests at merge time, falling back to spatial reuse only.
- **Disocclusion**: same issue at object edges. Reservoir age/count tracking is the standard mitigation — reset when reuse confidence is low.
- **Multi-light correlation**: small spheres of light can be missed if the initial M candidates don't include them. Mitigated by good initial PDF.

### Deliverable

A settings toggle: `Lighting mode = Forward+ | ReSTIR DI`. With ReSTIR DI selected, scenes with hundreds of lights render at the same cost as 64-light scenes did before, with cleaner soft shadows.

---

## Phase 2 — ReSTIR GI

**Goal**: replace DDGI with full-resolution indirect diffuse via ReSTIR.

### Algorithm sketch

Per-pixel, per-frame:

1. **Generate** one indirect bounce: trace a cosine-weighted hemisphere ray from the surface. Hit point's direct lighting (sun + emissive) is the radiance.
2. The "sample" stored is the **hit point's position + outgoing radiance** — not just a direction.
3. Reservoir maintains the best-found bounce sample.
4. **Temporal reuse**: reproject, merge — but with a **Jacobian correction** because the same incoming direction at a different pixel has different probability density.
5. **Spatial reuse**: K neighbors; the Jacobian is the geometry term ratio (relative hit-point solid angles).
6. **Final shade**: BRDF × stored reservoir radiance.

The Jacobian terms are where ReSTIR GI gets fiddly. The Ouyang et al. paper has explicit formulas; the Falcor reference implementation has them in code form.

### What it touches

Same shape as Phase 1, but bigger payload (radiance vector per reservoir) and trickier bias correction:

| File | Change |
|---|---|
| New: `RestirGiRenderer.cppm/.cpp` | Compute pipeline + GI-specific reservoirs |
| New: `Compute/restir_gi_initial.slang` | One bounce ray + cosine-weighted reservoir init |
| New: `Compute/restir_gi_temporal.slang` | Reproject + Jacobian-corrected merge |
| New: `Compute/restir_gi_spatial.slang` | Spatial merge with Jacobian |
| New: `Compute/restir_gi_shade.slang` | Read reservoir, output indirect contribution |
| Forward shader | Replace `sample_gi_irradiance` call with sample of `restir_gi_color` framebuffer image (when ReSTIR GI is active) |
| Settings | `Indirect diffuse mode = DDGI | ReSTIR GI` |
| Keep DDGI alongside as the "low" preset; ReSTIR GI is "high" |

**Estimated LOC**: ~1200, ~400 of shader. 2× Phase 1.

### Considerations

- **Per-pixel state grows**: now ~64 bytes/pixel (position + radiance + reservoir bookkeeping). At 1920×1080 that's ~130 MB. Acceptable.
- **Multi-bounce**: pure ReSTIR GI samples one bounce. Multi-bounce comes from DDGI or a probe layer running underneath (ReSTIR GI rays terminate by sampling DDGI for the next bounce). Hybrid is what the paper actually recommends.
- **Specular vs diffuse**: ReSTIR GI as published is diffuse only. Specular path resampling lives in ReSTIR PT.
- **Denoiser**: NRD has a separate diffuse-indirect preset. Use it.
- **Reconnection shift**: this is the explicit shift mapping mentioned in the GRIS literature. The Jacobian is `(cos_θ_new / dist²_new) / (cos_θ_old / dist²_old)` for each reused neighbor.

### Risks

- **Light-leaking through thin walls**: less than DDGI (no probe grid trilinear blend), but disocclusion handling still needs care. Visibility tests at merge time help, at perf cost.
- **High-frequency indirect features (caustics-adjacent)**: ReSTIR GI doesn't help — these need PT-class shifts.
- **Highly specular materials**: indirect specular needs ReSTIR PT or a separate specular-only pass (e.g., screen-space + RT reflections, which we have).

### Decision point

After Phase 2, decide: **does DDGI stay as a low-end fallback, or get removed entirely?** Keeping it adds maintenance; removing it forces ReSTIR GI to be the only indirect-diffuse option. The doc's recommendation is to keep DDGI as the "low" preset (cheaper, more deterministic) and ship ReSTIR GI as "medium/high."

---

## Phase 3 — ReSTIR PT (probably defer)

**Goal**: full multi-bounce path resampling. The "real-time path-traced reference" promise.

### What changes vs Phase 2

ReSTIR PT operates on **whole paths**, not single bounces. The shift mappings get complex — you have to match path vertices across pixels (reconnection) or rerun the random number stream from a known point (random replay). The hybrid shift used in production trades off between the two based on local roughness.

The math is in Lin et al. 2022 (GRIS) and the ReSTIR PT paper proper.

### Why defer

- **6+ months of focused work realistically.**
- **Cyberpunk Overdrive uses this** — it's the bleeding edge of what shipping engines do, not the median.
- **Subsumes much of the rest of the renderer**: if you have ReSTIR PT, you don't need DDGI, you don't need RT reflections as a separate pass, you barely need light culling. Big architectural shift.
- **Denoiser becomes harder**: full-path noise has different characteristics than DI or one-bounce GI; NRD has presets but tuning is involved.

### When to revisit

- After Phases 1 + 2 are shipped and battle-tested.
- When there's a content reason to need it (heavy emissive scenes, caustics, etc. — things the simpler ReSTIR variants can't do).
- When the team has time to do research-grade work, not just integration work.

Skip for now. Schedule a re-evaluation after Phase 2 ships.

---

## Cross-cutting considerations

### Bias mode policy

Every ReSTIR variant has a "biased" and "unbiased" mode. Biased trusts reused samples without re-evaluating visibility; unbiased re-evaluates and uses MIS to weight. Biased is roughly 3× faster and visually almost identical in most scenes.

**Recommendation**: ship biased mode as default. Expose an "unbiased mode" debug toggle for sanity-checking. The artifacts of biased are well-understood and usually acceptable.

### Memory budget

Rough estimate at 1920×1080:

| Resource | Bytes/pixel | Total |
|---|---|---|
| ReSTIR DI reservoir (current + previous) | 32 × 2 | ~130 MB |
| ReSTIR GI reservoir (current + previous) | 64 × 2 | ~260 MB |
| Denoiser history (NRD internal) | varies, ~40 | ~80 MB |
| **Phase 1 total** | — | ~210 MB |
| **Phase 1 + 2 total** | — | ~470 MB |

At 4K the totals 4× — ~1.9 GB combined. Still fits in modern GPU budgets but is real memory. Worth knowing before committing.

### Debugging strategy

ReSTIR bugs are notoriously hard. Standard playbook:

1. **Output target weight as color** to visualize what the reservoir thinks it has. Loud red areas = high-weight samples → either real bright lights or bugs.
2. **Disable spatial reuse** — does the signal still look right? If not, the temporal reuse is poisoning. If yes, spatial reuse is the bug.
3. **Disable temporal reuse** — same logic flipped.
4. **Visualize reservoir M count** — should grow monotonically up to a cap. If it's stuck low, reuse isn't happening.
5. **Compare against unbiased reference**: run a 256-sample path-traced reference offline for one frame, A/B against ReSTIR output. ReSTIR converges to the reference as M→∞.

AI agents are mediocre at this. Plan on real human debugging time per phase.

### Interaction with TAA

TAA further smooths ReSTIR's per-pixel noise. The two are complementary:
- ReSTIR amortizes Monte Carlo cost
- TAA amortizes pixel-resolution cost

Both rely on motion vectors, but neither knows about the other. Run ReSTIR before TAA in the chain. The velocity buffer drives both.

One subtle issue: TAA's neighborhood clamp can over-clamp ReSTIR-driven highlights (a newly-revealed bright light source). Tuning the clamp aggressiveness becomes a per-content choice.

### Interaction with DDGI

If both DDGI and ReSTIR GI are present, they should not double-count indirect light. Three policies:

1. **ReSTIR replaces DDGI when active** — forward shader checks the quality preset, samples one or the other.
2. **DDGI is the multi-bounce tail** — ReSTIR GI does 1 bounce, then samples DDGI for the bounce-of-bounce. Best quality, more complex.
3. **DDGI is fallback only** — used when ReSTIR reservoir is invalid (disocclusion).

Pick (1) for Phase 2's first ship, evaluate (2) later.

### Coexistence with existing inline RT

Our forward shader has `trace_shadow_ray`, `trace_ao`, `trace_reflection` inline. ReSTIR DI subsumes shadow rays. ReSTIR GI subsumes AO (cosine-weighted hemisphere = AO + indirect). RT reflections stay as a separate pass until ReSTIR PT.

After Phase 1, strip the inline shadow loop from `meshlet_geometry.slang`. After Phase 2, strip AO.

---

## Settings / quality preset surface

After both phases, the user-facing settings tree:

```
Lighting
  Direct lighting mode:        Forward+ | ReSTIR DI
  Indirect diffuse mode:       None | DDGI | ReSTIR GI
  RT shadow quality:           Off | Hard | Low | Medium | High   (only if Forward+ active)
  ReSTIR DI candidates (M):    16 | 32 | 64
  ReSTIR GI bounces:           1 | 2 (via DDGI fallback)
  Denoiser:                    Off | NRD
```

Existing settings (TAA, bloom, RT reflections) stay unchanged.

---

## Suggested PR slicing

1. **NRD integration** — denoiser standalone. Plumb HDR + motion vectors + depth into NRD; output denoised HDR. Doesn't need ReSTIR to test (point it at existing RT shadow output for verification).
2. **Reservoir storage + first ReSTIR DI pass (initial+temporal only, no spatial reuse)** — barely works but proves the pipeline.
3. **Spatial reuse pass + bias correction** — DI complete.
4. **Light list expansion** — raise the cap from 1024 to 16K+; consider light BVH for importance sampling.
5. **Forward shader strips its shadow loop** — DI fully replaces the inline path.
6. **ReSTIR GI initial pass + temporal reuse** — barely works again.
7. **GI spatial reuse + Jacobians** — GI complete.
8. **Forward shader composes DDGI fallback** — graceful disocclusion handling.

Slicing is roughly 4–6 weeks for Phase 1 (PRs 1–5), 8–10 weeks for Phase 2 (PRs 6–8) at solo-dev + AI-agent pace.

---

## Out of scope (for now)

- **ReSTIR PT** — scheduled re-eval after Phase 2.
- **Custom shift mappings** for our SDF-traced geometry or volumetric clouds. GRIS-level math, research project.
- **Cooperative-vector / WMMA acceleration** for any neural denoiser. Worth revisiting if a Vulkan cooperative-vector path becomes stable.
- **Neural denoisers** beyond OIDN/NRD. Tracked separately under the NRC scope.
- **Light hierarchies** (BVH over lights for importance sampling) — useful with ReSTIR DI but a separate workstream.

---

## Decision points to revisit

- **After NRD integration**: did the denoiser noticeably help RT shadows alone? If no, something's wrong with the existing RT signal and ReSTIR will inherit that.
- **After ReSTIR DI ships**: does scene perf scale meaningfully better with light count than Forward+? If not, the light list expansion or importance sampling isn't paying off.
- **After ReSTIR GI ships**: keep DDGI or remove? Decision based on perf delta + visual quality on representative scenes.
- **After both phases**: is the engine ready for the ReSTIR PT investment? Or is the win-per-effort better spent on something else (Lumen-style surface cache, neural radiance cache, etc.)?
