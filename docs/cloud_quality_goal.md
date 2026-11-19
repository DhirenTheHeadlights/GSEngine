# Goal: Crisp, stable volumetric clouds in every direction, at lower GPU cost

## Aim
The cloud layer should look crisp and clear everywhere: overhead, toward and away from the sun, and down to the horizon. Today it looks acceptable high up and degrades quickly toward the horizon and toward one direction. The cloud passes should also get cheaper, not more expensive, while this is fixed.

Scoping found no single obvious bug, so this is a goal rather than one change. It found several independent weaknesses, any of which could be the artifact the owner sees.

## Starting point (from scoping, 2026-11-10)
Observed on one `sky_horizon_probe` run (attached, DX12, about 2810x1543):
- Stills at 45° and about 10° pitch show a low-contrast, smeared cloud layer with no defined edges and a fine dither grain over it.
- Per-tag p50: `cloud_raymarch_pass` 712 µs (p95 1976 µs), `cloud_resolve_pass` 265 µs, `cloud_composite_pass` 204 µs. That is about 1.18 ms p50 for the three passes. This is one run, so it is not a baseline.
- The screenshots include the `live::Clouds` settings panel over half the frame.

Leads from reading the code, none confirmed as the cause:
- **A. Jitter does not cover the step at distance.** `cloud_raymarch.slang` starts the ray at `t_enter + coarse_step * jitter`, but every step is scaled by `1 + t / step_growth_distance` (8 km). Beyond about 24 km the jitter spans less than one fine step, so sample shells sit at fixed distances and temporal accumulation cannot average them out. From the ground, that is everything below roughly 14° elevation.
- **B. Shape noise is undersampled at distance.** At 50 km the fine step is 0.7–1.5 km. The shape noise base feature is about 0.83 km and its Worley octaves go down to about 0.1 km. The shape texture has no mip chain and is always sampled at level 0. Only the detail noise is faded by step length (`cloud_detail_resolvable`).
- **C. No distance treatment.** Clouds get no aerial perspective. The march is cut hard at `t_enter + max_distance` and at `view_dir.y < 0.01`. Sun transmittance is evaluated at the camera, not at the sample.
- **D. Sun-direction sensitivity.** The forward phase lobe (g = 0.8) is about 260 times brighter toward the sun than sideways, so noise in the light march is amplified there. The light march uses 6 fixed-kernel samples, drops to 2 and is reused every second step beyond 20 km. Unclamped HDR values go into the history clamp. Whether the owner's "one direction" is the sun direction is not known.
- **E. The resolution chain blurs.** The trace is quarter resolution. Only 1 of every 4 trace pixels is fresh per frame; the other 3 are bilinearly reprojected from the previous low-res target, every frame. The resolve is half resolution, blends non-traced pixels at 0.05, clamps history to only 4 fresh taps, and the composite is a plain bilinear upsample with no depth awareness. A full refresh takes 16 frames.
- **F. Cost.** Each lit density step runs up to 6 + 1 base-density samples (two 3D fetches each) for the light march, on top of 2–3 fetches for the view sample. Nothing uses mips or cheaper far-field sampling.
- **G. Not an artifact, but a limit.** The march origin is fixed at x = z = 0, so the cloud layer follows the camera horizontally.

## Pass criteria
1. **Probe scenario exists.** A new scenario `cloud_quality_probe` on the Sky scene pins every cloud, sun and weather input, sets wind to zero, pushes a temporal reset, and captures stills with no UI in the frame.
   - 12 held views: pitch 80°, 35° and 5°, each at 4 azimuths relative to the sun (toward, +90°, away, −90°). Each view is held for at least 120 frames before capture, and captured twice, 30 frames apart.
   - 2 motion captures taken mid-pan at a fixed yaw rate, one at 35° and one at 5° pitch.
   - The run logs which capture file belongs to which view.
2. **Stable when held.** For every held view, the two captures compared with `Tools/frame_parity.py diff` give mean ≤ 0.5 and ≤ 1% of pixels over 8. Holds in 3 consecutive runs.
3. **Close to a reference, in every direction.** A reference run of the same scenario uses overrides `Clouds.target_divisor=1`, `Clouds.resolve_divisor=1` and the highest value of a march-quality setting added for this purpose.
   - Default-settings still vs reference still: PSNR ≥ 32 dB for every held view.
   - The worst held view is within 3 dB of the best one, so the horizon and sun-facing views are not allowed to trail the zenith.
   - Holds in 3 consecutive runs.
4. **Cheaper.** The summed p50 of `cloud_raymarch_pass`, `cloud_resolve_pass` and `cloud_composite_pass` is ≤ 75% of the baseline, on both `cloud_quality_probe` and `cloud_showcase`, at default settings, in 3 consecutive runs on an idle machine. The baseline is 3 runs of each scenario recorded in the ledger by the first task, before any renderer change.
5. **Not bought by lowering quality.** Defaults for `target_divisor` and `resolve_divisor` are unchanged or finer. Cloud layer extents, coverage, `max_distance` and shadow settings keep their defaults.
6. **Nothing else regresses.** The release build is clean. `cloud_showcase`, `sunset_showcase`, `atmosphere_showcase` and `sky_horizon_probe` pass. One `cloud_quality_probe` run with `Graphics.validation_layers_enabled=true` logs no validation errors. Cloud shadows on the ground still render in `cloud_showcase`.
7. **Owner sign-off.** The owner approves the 12 stills, the 2 motion captures and a `cloud_showcase` clip as crisp and artifact-free. This is an owner criterion.

## Charter (no need to ask)
- **Cloud renderer and shaders:** `Engine/Engine/Source/Graphics/Renderers/CloudRenderer.cppm` and `.cpp`; `Engine/Resources/Shaders/Clouds/*`; `Engine/Resources/Shaders/Bodies/Compute/cloud_*.slang`; `Engine/Resources/Shaders/Bodies/Graphics/CloudResolve.slang` and `CloudComposite.slang`.
  - Changing the march, jitter, noise bake and sampling (including mip chains), light march, temporal reprojection, resolve and upsample is in scope.
  - Adding cloud settings, and the march-quality setting criterion 3 needs, is in scope.
- **Reading atmosphere outputs:** binding the existing transmittance, sky-view and aerial-perspective resources into the cloud passes. Small additive helpers in `Engine/Resources/Shaders/Atmosphere/atmosphere_common.slang` are allowed.
- **Scenario and tooling:** add `cloud_quality_probe` in `Sandbox/Sandbox/Source/Scenarios/*` following `docs/scenario_authoring.md`. Extend `Tools/frame_parity.py` if a needed comparison is missing.
- **GPU layer, additive only:** if 3D mip generation or a sampler feature is missing, add it to `gse.gpu` for Vulkan and DX12 without changing existing behaviour.
- **Process:**
  - Build and run scenarios freely. Turn diagnostics on with `settings` overrides only.
  - Judge performance on an idle machine with at least 3 runs.
  - Revert experiments before a task closes and record every one in the ledger, including the ones that did not help.
  - Follow `docs/CODE_REVIEW_GUIDE.md`, `docs/STYLEGUIDE.md` and `docs/GRAPHICS_REVIEW.md`. No comments in code. Keep units typed.

**Off limits without asking:**
- editing `settings.ini`;
- lowering any default quality setting, resolution or distance to hit a number;
- changing the atmosphere model, its LUT generation, TAA, tonemap or the forward renderer beyond a binding;
- changing how cloud shadows look on scene geometry;
- the WebGPU backend, beyond keeping it compiling;
- re-baselining hashes of other scenarios;
- commits.

## Steering order
1. Add `cloud_quality_probe` with UI-free captures. Record the performance baseline and the stability and reference numbers for today's renderer in the ledger. Note which views are worst, which settles which leads matter.
2. Fix the march sampling at distance: jitter that spans the real step (lead A), then filtered shape sampling by step length (lead B). Re-measure.
3. Fix the distance treatment (lead C) and the sun-facing noise (lead D). Re-measure.
4. Rework the temporal chain for sharpness (lead E). Re-measure stability and reference PSNR.
5. Cut cost (lead F and whatever the profile shows) until criterion 4 holds without losing criteria 2–3.
6. Run the regression set and get the owner's sign-off.

## Assumptions
- The thresholds in criteria 2 and 3 were chosen without a measured baseline. If task 1 shows the reference itself is unstable, or that a threshold is unreachable for a reason unrelated to quality, steering escalates instead of changing the number.
- The machine `gse_run` uses is the owner's, attached runs use DX12 on the Intel adapter, and performance is judged there.
- "One direction" was not identified. If the probe shows no azimuth-dependent difference, steering asks the owner which direction they mean.
- Lead G (clouds following the camera) is left as it is unless the owner asks for it.

## Ledger
