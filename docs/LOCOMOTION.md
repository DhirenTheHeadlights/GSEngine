# Locomotion: physically-simulated, learned control

## Where this is going

The end state is **animation-quality, FPS-ready humanoid locomotion that is fully physically simulated** — every motion is the result of joint-servo torque and ground contact, with no animation playback and no kinematic target the body merely tracks. We reach it with a **learned controller**: a neural policy, trained by reinforcement learning to imitate reference motion (DeepMimic / AMP class), that outputs joint targets each tick. The body stays a physical object the whole time — the policy replaces the hand-tuned heuristics, not the physics.

The decision that shapes everything below: **the policy is trained inside GSEngine's own physics**, on a GPU-batched version of the VBD solver we already have — not in Isaac Lab / MuJoCo with a transfer step afterward. This document scopes that system.

The hand-tuned controller stack that exists today (`StateEstimator → GaitScheduler → FootstepPlanner → BalanceController → LegController`) is **not throwaway**. It is the scaffolding the learned system is built on: it already defines the observation, the action seam, the test harness, and a working baseline that serves as a reward reference and a runtime fallback. The pivot is from *hand-tuning fixed-gain reactive controllers on a marginal system* (which has a hard ceiling on naturalness and responsiveness — see [The pivot](#the-pivot-why-not-keep-hand-tuning)) to *learning the controller*, while keeping the same physical substrate and the same actuation seam.

---

## Why train in-engine (decision made)

The standard RL-locomotion workflow is: rebuild the character in a GPU-batched simulator (Isaac Lab, MuJoCo MJX, Brax), train there in minutes-to-hours, then transfer the policy back. We are deliberately **not** doing that. Three reasons, in priority order:

1. **Zero sim-to-sim gap.** A policy trained in Isaac learns *Isaac's* contact model and integrator. Deployed on VBD it sees different dynamics — the policy is fighting a physics it never trained against. The usual fix (domain randomization) only *masks* the gap; it never closes it. For a project whose entire identity is "physically based in *my* engine," training in a foreign physics and hoping it transfers is the one thing that undercuts the premise. Train in VBD and the policy is correct by construction.
2. **The sim is the product.** A GPU-batched, headless, resettable VBD is independently valuable — it is what makes large-scale physics tests, procedural-content validation, and any future learned behavior (not just locomotion) possible. Building it is an engine capability, not a throwaway training rig.
3. **Throughput is the only thing that ever made RL fast, and we can own it.** PPO needs ~10⁸–10⁹ physics steps to learn humanoid locomotion. Serial, that is ~weeks. The entire reason the field went from "weeks on a cluster" to "minutes on one GPU" around 2021 was *massively parallel simulation* — thousands of environments stepping at once on the GPU. We already have a GPU VBD solver; extending it to batch thousands of independent worlds is the lever, and it is a tractable extension, not a rewrite (see [Component A](#a-batched-gpu-physics-the-throughput-lift)).

**What we are accepting in exchange:** we build the batched-sim training harness and the RL plumbing ourselves instead of borrowing Isaac's. That is real engineering — the phased plan below front-loads the cheap correctness work and de-risks the expensive GPU work before committing to it.

---

## The architecture

Two halves. The **simulation half lives in the engine** (C++, GPU). The **learning half drives it** — it reads observations, runs the policy, writes actions, computes advantage, and updates weights. This is exactly Isaac Gym's own split; the difference is we own both sides and the sim is VBD.

```
                 ┌─────────────────────────── in engine (GPU) ───────────────────────────┐
   reset ──────► │  N independent VBD worlds (one humanoid each)                          │
                 │     env management: termination, domain randomization, per-env reset   │
                 │            │ body_state (pos/vel/orient), contacts, joints              │
                 │            ▼                                                            │
                 │  StateEstimator  ──►  observation buffer  [N × obs_dim]  (GPU-resident) │
                 └────────────────────────────────┬───────────────────────────────────────┘
                                                   │
        ┌──────────────────── learning half (PPO; libtorch or Python) ───────────────────┐
        │   obs ─► policy πθ ─► action [N × act_dim]                                       │
        │                          │                  reward r(state, reference, command) │
        │   advantage / GAE ◄──────┘◄──────────────────────────── done flags, returns     │
        │   θ ← θ + α ∇ clipped-surrogate                                                  │
        └──────────────────────────────────┬──────────────────────────────────────────────┘
                                            │ action buffer [N × act_dim] (GPU-resident)
                 ┌──────────────────────────▼─────────────────────────────────────────────┐
   step  ──────► │  apply action → joint_drive targets → VBD solve(dt) → next body_state   │
                 └──────────────────────────────────────────────────────────────────────────┘
```

At **ship time**, the learning half is gone. The trained policy is a small MLP; a forward pass per character per tick (microseconds, no GPU needed) writes the same joint targets. One sim, one humanoid, a few hundred KB of weights.

### The MDP, mapped onto what already exists

The reason this is an evolution and not a rewrite: the Markov decision process is **already spelled out in the locomotion types**. We are not inventing an observation or an action space — we are reusing the ones the hand-tuned stack already computes and consumes.

| MDP element | Today's type | Notes |
|---|---|---|
| **Observation** `o_t` | `gs::locomotion::state` (Game/Game/Source/Locomotion/Types.cppm) | `StateEstimator` already produces pelvis pose/velocity/orientation, per-foot position + grounded flags, support polygon, CoM, capture point, measured hip/knee angles, pelvis pitch + pitch rate. This *is* a proprioceptive observation vector. Extend with: full per-joint angle+velocity (currently only hip/knee/pelvis are surfaced), foot contact forces, and — for imitation — the reference clip's phase variable φ. |
| **Command / goal** `g` | `gs::locomotion::intent` | `forward`, `strafe`, `sprint_blend`, `desired_yaw`, `has_heading`, `jump`. Already the player/task interface. Becomes the goal-conditioning input to the policy (velocity + heading + gait command). |
| **Action** `a_t` | `gse::physics::joint_drive_component` per-axis targets | The LegController already actuates by writing per-axis angular servo targets (stiffness/target/torque-cap/damping), solved implicitly in the VBD joint solve (`accumulate_joint_drive`). The policy writes the **same** targets — most naturally as a *residual* PD target on top of the reference pose (DeepMimic-style), which keeps actions small and learning stable. The `gse::physics::motor_component` pelvis balance crutch is *not* in the action space; pure-physics RL aims to retire it (it stays only as a fallback assist for the shipped player — see [the crutch tension](#f-inference-runtime-the-ship-side-payoff)). |
| **Reward** `r_t` | new — `f(state, reference, command)` | Imitation term (match reference joint angles, velocities, end-effector positions, CoM) + task term (track commanded velocity/heading) + regularization (energy, action smoothness, no self-penetration). AMP replaces the hand-specified imitation term with a learned discriminator. See [Component D](#d-reference-motion--reward). |
| **Episode / reset** | `gs::locomotion::smoke_test` (SmokeTest.cppm) | The smoke is already a headless, fixed-step, deterministic, multi-trial episode loop with reset (scene deactivate→reactivate), per-trial metrics, and a bit-exact `state_hash`. The trainer is its multi-env generalization: N parallel episodes, each reset independently on termination. |

**This is the concrete content of "the controller work is not throwaway":** `StateEstimator` becomes the observation function unchanged; the joint-drive seam becomes the action interface unchanged; the smoke harness becomes the episode loop; and the five hand-tuned systems become (a) the baseline that defines what "working" looks like, (b) a source of reference trajectories to imitate before any mocap exists, and (c) the runtime fallback if the policy ever destabilizes.

---

## Components to build

Each component below lists *what it is*, *what it builds on*, *the work*, and *the risk*.

### A. Batched GPU physics (the throughput lift)

**What.** Run N independent VBD worlds — one humanoid per world, no cross-world interaction — in one batched GPU dispatch, so a training step advances all N at once.

**Builds on.** `gse.physics:vbd_gpu_solver` (`gse::vbd::gpu_solver`) is **already a full compute pipeline**, not a stub: predict, solve-color, lambda update, derive-velocities, finalize, collision reset/grid-build/broad-phase/narrow-phase/build-adjacency/build-coloring, restitution, impulses, freeze-jacobians, plus upload/readback of body snapshots and grounded flags. The solver structs (`body_state`, `contact_constraint`, `velocity_motor_constraint`, `joint_constraint`, `solver_config`) carry `[[= shaders::shader_struct]]`, so their GPU buffer layouts are reflection-generated from the C++ definitions — adding a field propagates to the shader side automatically.

**The work — and why it is smaller than it looks.** Independent environments share **no constraints**, so the union of N worlds is a constraint graph of N disconnected components. The existing graph coloring and the per-body Gauss-Seidel/Jacobi solve are *already correct on a disconnected graph* — they color and solve each component independently with no change. The batching lift therefore is **not** "rewrite the solver"; it is three targeted pieces:

1. **Collision env-isolation.** The broad phase must never generate a contact between bodies in different worlds. Two options:
   - *Spatial offset (fast prototype):* lay each env out on a wide world-space grid, far enough apart that the existing uniform grid never bridges them. Zero solver change; works immediately. Costs: float precision far from origin, and grid memory scales with the bounding volume.
   - *Env-tagged broad phase (principled):* add `env_id` to `body_state` (reflection carries it to the shader), and reject candidate pairs with mismatched `env_id` in broad phase. No spatial blow-up; the clean long-term path. **Recommended**, with spatial-offset as the throwaway prototype to unblock Phase 3 early.
2. **The jointed-pair contact filter on the GPU.** Known debt (was item §5): the CPU path skips contacts between jointed bodies (`add_scene_contacts_to_solver`); the GPU narrow phase does not, which is why the articulated humanoid runs on CPU today. The batched humanoid *must* run on GPU, so this filter has to move into the GPU narrow phase (a per-pair jointed-neighbor bitset, uploaded once per env since topology is fixed).
3. **Cheap per-env reset.** Training resets constantly and per-env (env 7 terminates while the rest run on). The smoke's scene deactivate→reactivate (6+ ticks, full teardown) is far too heavy and **crashes when a ragdoll is mid-fall at reset** (was item §5 — now blocking, not a nuisance). Replace with a GPU-side reset that overwrites one env's `body_state` block with a stored initial pose, zeroes its velocities, and clears that env's contact-cache / warm-start entries — a buffer write + a per-env clear, no pipeline rebuild.

**Risk.** Highest-uncertainty component. VBD convergence at batch scale (do 4 iterations still hold for thousands of envs with randomized masses?), GPU memory for N×(bodies+contacts+jacobians), and determinism across the batched dispatch. Mitigated by proving the whole RL loop on CPU first (Phases 1–2) so this phase debugs *only* the GPU sim, never the sim and the RL math together. Also note the standing build gotcha: clean/cascade rebuilds corrupt some BMIs on gcc-trunk-mcf — develop the solver changes incrementally on warm BMIs.

### B. Episode / environment management

**What.** The per-env lifecycle: initial-state distribution, domain randomization, termination, and reset — all vectorized over N envs and deterministic per seed.

**Builds on.** The smoke's stage machine (`warmup → running → resetting`), its metrics accumulation, and its `state_hash`.

**The work.**
- **Initial-state distribution.** Reference-state initialization (RSI): reset each env to a *random frame of the reference clip* (pose + velocities), not always a neutral stand. RSI is the single biggest stabilizer for imitation learning — it lets the policy see late-cycle states without first surviving early-cycle ones.
- **Domain randomization.** Per-env, per-episode: body masses (the rig already carries per-segment masses in `skeleton_refs`), friction (today **global** in `physics::system` — needs a per-body/per-env friction field, which also unblocks the §3 crutch goal), joint-drive gains, initial velocity, and external shove impulses. Seeded from `(base_seed, env_id, episode_count)` for reproducibility.
- **Termination.** Fall (pelvis height collapse — the gait scheduler's debounced detector generalizes), imitation divergence (tracking error past a threshold), self-penetration, and time-limit truncation (distinguished from failure for correct bootstrapping).

**Risk.** Low–medium. Mostly mechanical once reset (A.3) is cheap. The friction field is a small core-physics change but touches contact generation.

### C. The MDP bridge (obs / action buffers)

**What.** The data plane between sim and policy: present `state` (+ extensions + reference phase) to the policy as a contiguous `[N × obs_dim]` observation view, and route a `[N × act_dim]` action tensor back to joint-drive targets.

**Builds on.** `StateEstimator` (observation source), the joint-drive seam (action sink).

**The work.** A **single source of truth for the layout** (reflection over the observation struct generates the field manifest, consistent with how `shader_struct` generates buffer layouts — avoids obs/action index drift, the classic RL footgun). Unit types are layout-compatible with their underlying scalar, so the unit-typed obs/action structs reach libtorch the same way they already reach GPU push constants — a pass-through *view* over the same bytes at the foreign-API edge, **not** a unit-stripping copy, and never a bare scalar carried in engine logic. Keep obs/action **GPU-resident**: if the sim runs on the GPU and the policy runs in libtorch (also GPU), the obs/action views and inference should stay on-device to avoid PCIe round-trips every step — that round-trip, not the math, is what bottlenecks naive in-engine RL. First cut may round-trip through host for simplicity; the on-GPU path is the throughput optimization.

**Risk.** Low. The one hazard is layout drift between trainer and shipped inference — solved by generating both from one definition.

### D. Reference motion & reward

**What.** The data and the objective that make the motion look *human*, not merely stable. This is the lever for animation quality (old §2).

**Builds on.** Nothing yet — new. But the existing hand-tuned gait can **bootstrap** it: record `state` trajectories from the working controller as the first reference set, before any mocap pipeline exists. The policy can learn to imitate the heuristic gait, proving the imitation loop end-to-end on data we already produce.

**The work.**
- **Reference store + retarget.** Load mocap clips (or recorded heuristic trajectories), retarget to the 17-bone rig, index by a phase variable φ ∈ [0,1). Storage is per-frame joint rotations + root motion.
- **Imitation reward (DeepMimic).** Weighted sum of pose, velocity, end-effector, and CoM tracking errors vs the reference at the current φ. Hand-specified, interpretable, the proven baseline.
- **AMP (later, for quality + generality).** Replace the hand-weighted imitation term with a learned **discriminator** rewarding motion that is *indistinguishable from the reference distribution*, while a separate task reward drives velocity/heading. AMP removes per-clip reward engineering and blends a motion *dataset* into natural transitions — this is what turns "tracks one clip" into "moves like a person across all commands."

**Risk.** Medium. Reward shaping is the part that eats iteration time (not GPU time). RSI (B) and starting from bootstrapped heuristic references de-risk it substantially.

### E. Training loop + policy network

**What.** PPO (the locomotion-RL default): collect rollouts from the N envs, compute GAE advantages, update the actor/critic with the clipped surrogate objective. Policy and value are small MLPs (~2–3 hidden layers).

**The work — where the learning half lives.** The sim is in-engine regardless; the question is where autodiff + the optimizer live. Three routes, in recommended order:

1. **libtorch in a `gse.ml` module (recommended target).** Link the C++ Torch API, wrapped in an engine module per the "never `#include` third-party headers" rule (same pattern as `gse.vulkan` / the planned `gse.directx`). Autograd, Adam, and checkpoint serialization for free; **in-process with the sim**, so obs/action can stay on-GPU (C) with no IPC. This keeps training in-engine, matching the stated goal.
2. **Python bridge (fast-prototype escape hatch).** Expose the batched env over a C ABI / shared memory to a mature Python PPO (rl_games, CleanRL). Fastest path to a *first* learning result and to iterating reward/obs design, because the RL code is battle-tested. Out-of-process and Python — acceptable for prototyping the objective, not the in-engine end state. Worth using *only* if it accelerates settling C/D before the libtorch investment.
3. **Hand-rolled in C++ (purist, inference-only-grade).** PPO is a few hundred lines; an MLP forward+backward and Adam are straightforward. Viable and fully dependency-free, but you validate the RL math yourself — reserve for if libtorch proves too heavy a dependency. Note: the *inference* forward pass (F) should be hand-rolled regardless, so the shipped engine carries no ML dependency.

**Risk.** Medium. PPO is well-trodden; the risk is hyperparameter/reward coupling, mitigated by validating the loop on a trivial task (Phase 1) before locomotion.

### F. Inference runtime (the ship-side payoff)

**What.** Run the trained policy in the normal single-instance game: load weights, forward pass per character per tick, write joint-drive targets.

**The work.** A hand-rolled MLP forward pass in `gse.nn` (no training deps, loads a weights blob), reading the same `state` observation and writing the same `joint_drive_component` targets. Compute is trivial (microseconds/char). Goal-conditioned on `intent`, so the existing WASD/heading/sprint controls drive it directly.

**The crutch tension, resolved.** Old §3/§4: the pelvis `motor_component` and foot-anchor crutches keep the player from ever falling, which an FPS needs, but conflict with pure-physics purity. With a learned controller this largely dissolves — a well-trained policy *is* the robust balance, no magic motor required. **For the shipped player, keep a small balance assist as a safety net and make it bulletproof; reserve fully crutch-free pure-physics for NPCs / ragdoll / sandbox**, where an occasional fall is fine or the point. The learned policy is also what finally makes the §3 goal (propulsion + balance entirely through joints and contact) reachable, since it can discover the forward-braking foot placement the hand-tuned legs never could.

### G. Determinism, the gate, tooling

**What.** Keep the bit-exact, headless, deterministic discipline that made the hand-tuned work tractable, generalized to training and policies.

**The work.** Per-env seeded RNG; a `state_hash` generalized across envs for regression (a sim change that shouldn't alter dynamics must not move the hash); evaluation runs of a frozen policy through the existing smoke phases (settle → walk → 90° turn → sprint → stop → stand) as an acceptance gate; training-curve + reward-term telemetry. The shipped policy must still pass the smoke gate (trials 2–5; trial 1's across-process flake must finally be killed — it becomes intolerable when every training reset exercises that path).

---

## Phased roadmap

Ordered to **front-load cheap correctness and de-risk the expensive GPU work last**. The governing principle (learned the hard way in the hand-tuned era): never debug two unproven things at once. Prove the RL loop on slow-but-correct CPU sim before building the fast GPU sim.

**Phase 0 — Lock the MDP seam (no new sim).**
Define `obs_dim`/`act_dim` and the unit-typed layout against today's `state` and joint-drive seam (Component C, layout only). Validate by having the *existing heuristic controller* emit (obs, action) tuples — confirms the observation is sufficient and the action space actuates the rig — and record heuristic-gait trajectories as the first reference set (D bootstrap). Decide actuation (residual-PD-on-reference, recommended) and the learning-half home (E). *No physics or RL work yet — pure interface + data.* **Full worker spec: [Phase 0 — detailed implementation plan](#phase-0--detailed-implementation-plan).**

**Phase 1 — Single-env CPU training loop, end to end.**
Wrap one CPU VBD world as a gym-style env (reset/step/obs/reward/done) using the existing solver. Run PPO on a **trivial task first** (stand / balance, or track a constant forward velocity) to prove the entire loop — obs/action wiring, reward, advantage, update, checkpoint — in isolation. Slow is fine; this is correctness, not speed. **Full worker spec: [Phase 1 — detailed implementation plan](#phase-1--detailed-implementation-plan) (and the Phase 0 close-out it depends on).**

**Phase 2 — CPU-parallel envs → first real learning run.**
Run 32–64 envs across cores. Train a velocity-and-heading-conditioned **walk** that imitates the bootstrapped heuristic reference. This validates obs/action/reward/RSI/termination on real locomotion. Days-per-run is acceptable — it is the design-validation phase, and a positive result here is what justifies the GPU investment. **Full worker spec: [Phase 2 — detailed implementation plan](#phase-2--detailed-implementation-plan).**

**Phase 3 — Batched GPU physics (the throughput lift).**
Build Component A: env-isolation (spatial-offset prototype first, then `env_id` broad phase), the GPU jointed-pair filter, and cheap per-env reset. Bring obs/action on-GPU (C). Target thousands of envs and training runs in **minutes-to-hours**. The RL math is already proven (Phases 1–2), so this phase debugs only the GPU sim. **Full worker spec: [Phase 3 — detailed implementation plan](#phase-3--detailed-implementation-plan) (the Phase 2 GAE fix is done — gate cleared; Phase 2a validated, 2b locked in).**

**Phase 4 — Motion imitation / AMP → animation quality.**
Now also absorbs the **bootstrap imitation moved out of Phase 2** (2026-06-14): the heuristic-reference recording (`--locomotion-record`), the hand-weighted DeepMimic imitation reward, and RSI — see Phase 2's AD-13/AD-15 + the deferred subtasks for the design. Stand up the real reference pipeline (mocap retarget), move from hand-weighted imitation to AMP with a motion dataset. This is where the result crosses from "stable physical walk" to "moves like a character." Add the natural detail the procedural gait lacked (heel-toe roll — note the toe hinge is passive today, a real toe drive is its own item; ground-adaptive foot IK; arm/spine secondary motion emerge from imitation).

**Phase 5 — Inference integration + FPS work.**
Ship-side runtime (F): policy in `gse.nn`, driven by `intent`. Then the FPS layer — the **upper/lower-body split** (legs locomote on the locomotion policy while the torso + weapon track aim independently) is the single highest-value FPS change and composes naturally here (lower body = locomotion policy conditioned on velocity, upper body = aim pose / its own small policy). Then crouch/jump/lean/mantle and **hit reactions** (blend to ragdoll on damage and recover — trivial when the controller is already a policy over physics).

---

## Phase 0 — detailed implementation plan

This is the full worker spec for the first roadmap step. Phase 0 builds **no simulation and no RL**. It produces three things: (1) a frozen, reflection-described **unit-typed observation/action layout** (`obs_dim`, `act_dim`, field manifest), (2) a **recorder** that captures `(observation, action)` tuples and full kinematics from the existing heuristic controller as it runs the smoke, and (3) a **validation** that proves the records round-trip losslessly and that the recorded actuation fully determines the motion. The output data set doubles as the first imitation reference (Component D bootstrap). When this phase is done we know the MDP interface is correct and sufficient *before* a single line of physics-batching or PPO is written.

**Definition of done:** the smoke runs with `--locomotion-record` and writes a versioned trajectory file; the unit-typed records serialize/deserialize bit-for-bit (Tier-1); and a replay of the recorded actuation reproduces the smoke's `state_hash` on trials 2–5 (Tier-2, recommended). No regression to the existing 5/5 gate.

### 0.0 Orientation — read before touching anything

- **Read these files first** (they are the entire surface you are wrapping): [Types.cppm](Game/Game/Source/Locomotion/Types.cppm) (the `state`/`intent`/`skeleton_refs` definitions), [StateEstimator.cppm](Game/Game/Source/Locomotion/StateEstimator.cppm) (the observation source), [LegController.cppm](Game/Game/Source/Locomotion/LegController.cppm) (`write_drives`, the action sink), [SmokeTest.cppm](Game/Game/Source/Locomotion/SmokeTest.cppm) (the loop + `state_hash`), [HumanoidSkeleton.cppm](Game/Game/Source/Shared/HumanoidSkeleton.cppm) (the joints), and `JointDriveComponent.cppm` (the action component).
- **Read [docs/STYLEGUIDE.md](docs/STYLEGUIDE.md) and obey it** — no comments, snake_case, declarations inside the `export namespace` and definitions outside it, **unit types for every physical quantity — NEVER strip units** (the `observation`/`action` structs are unit-typed and stay that way through the recorder and `observe()`; there is no float-packing step in Phase 0 — see AD-1), concepts in the template-parameter slot, no anonymous/`detail` namespaces, no `inline` in modules. These are lint-enforced; violations will be rejected.
- **Build / commit discipline (hard rules):** do **not** run `cmake`/`ninja`/`g++` and do **not** `git add`/`commit`/`push` on your own. Stage all edits, then ask the owner to build and run the gate. Develop incrementally — adding the two new `.cppm` partitions below forces a CMake reconfigure, and a clean/cascade rebuild on this toolchain (gcc-trunk-mcf) is known to corrupt some BMIs. If a build fails with a "Bad file data" / cluster-load error after adding a partition, that is the known corruption, *not* your code — flag it, don't chase it.
- **Pattern to copy for any new system:** `struct x { struct [[= gse::settings::category<"…">{}]] data { … }; static auto run(data& d, gse::read<…>, gse::write<…>) -> gse::async::task<>; };`, registered with `e.add_system<x>()`. See `state_estimator` for the canonical example.

### 0.1 Architectural decisions (resolve first)

Each has a recommendation. **If the owner does not say otherwise, build the recommended option** — they are chosen to be the lowest-risk path that does not foreclose the end state. Decisions the worker must *not* make unilaterally are marked 🚩.

**AD-1 — Observation/action representation (DECIDED — unit-typed, no packing in Phase 0).** The `observation` and `action` structs have **unit-typed members** (`gse::angle`, `gse::velocity`, `gse::angular_velocity`, `gse::length`, `gse::displacement`, `gse::vec3<…>`, etc.); only genuinely dimensionless quantities (normalized commands, sin/cos of phase) are plain `float`. They are the single source of truth, consumed by the recorder now and by the trainer + shipped inference later. **Phase 0 does no float/tensor packing** — the recorder serializes the unit-typed records as-is (they are layout-compatible PODs). When Phase 1 feeds them to a policy, the network input is a foreign-API edge exactly like a GPU push constant: unit types are layout-compatible with their underlying scalar and pass through *directly*, never stripped in engine logic — so that is a Phase-1 concern, not a Phase-0 packer. Reflection over the members generates only the recording's **field manifest** (ordered member names + scalar count + a `layout_hash` drift guard), not a packer. *Rejected:* reflecting directly over `state` (it mixes raw, derived, and bool fields and has nowhere to put the reference phase φ or the missing joint signals).

**🚩 AD-2 — Action space scope & form.** *Recommended scope:* the **10 actively-driven axes** the heuristic modulates each tick — `hip_*` (pitch, yaw, roll) + `knee` + `ankle`, per leg (see the table in 0.2). Arms (shoulders/elbows) and toes are held at a constant drive set once at spawn; **exclude them now**, add them in Phase 4 when imitation needs natural arm motion. *Recommended form:* **residual-PD-on-reference with fixed PD gains** — the policy's action is a per-axis *target-angle residual*; stiffness, damping, and torque caps are fixed config, **not** part of the action. The heuristic's per-tick gain/torque-cap switching (deliberately weaker swing leg) is *recorded as diagnostics* but is not in the policy action vector. *Excluded from the action space entirely:* the pelvis balance `motor_component` and the foot-anchor motors (the crutch the project aims to retire). **Why it's flagged + a consequence to surface:** because the motors co-produce the heuristic's motion, the bootstrapped reference reflects *motor-assisted* motion. An imitation policy restricted to joints must reproduce that motion **without** the motor — so the reference is slightly "optimistic" relative to pure-joint capability. This is intended (it is the whole pivot) but the owner should acknowledge it before we treat the heuristic trajectories as ground truth.

**AD-3 — Recorder placement / tick ordering.** *Decision (low-risk, do it):* a **dedicated recorder system** scheduled **after** the controllers, not folded into `smoke_test`. Reason: `smoke_test::run` runs *before* the controllers within a tick (it produces `intent` that they consume), so reading `joint_drive_component` there would capture the **previous** tick's action. A separate system that reads `state` (written by `state_estimator`) **and** `joint_drive_component` (written by `leg_controller`) is auto-ordered after both by the ECS's read-after-write scheduling, giving same-tick `(state_t, action_t)`. Verify the ordering held by confirming a recorded action equals what `leg_controller` wrote that tick.

**🚩 AD-4 — Replay-validation depth.** *Tier-1 (required):* offline serialize→deserialize round-trip of a record is exactly lossless (unit values preserved). *Tier-2 (recommended):* re-run the smoke with the controllers disabled, feeding the **recorded full actuation** (10 joint targets + per-tick stiffness/torque caps + the motor writes) back into the components each tick, and assert the `state_hash` matches the recorded run on trials 2–5. Tier-2 is the real proof that "the action seam actuates the rig" and that the recording captures *every* actuated DOF; it costs a replay path. **Flagged** because the owner may accept Tier-1 plus a lighter check to save time.

**🚩 AD-5 — Learning-half home (decide, don't build).** *Recommended decision to record now:* target **libtorch wrapped in a `gse.ml` module**, in-process with the sim (keeps obs/action on-GPU later), with a **Python bridge** as the prototyping escape hatch if reward/obs iteration stalls, and a **hand-rolled MLP forward pass** for ship-side inference regardless. Phase 0 writes none of this — it only ratifies the direction so Phase 1 can lean on it. **Flagged:** it is a dependency commitment with real build-system risk on this toolchain.

**AD-6 — Serialization format.** *Decision (do it):* a versioned POD binary — a header `{ magic, version, obs_dim, act_dim, layout_hash, fixed_dt, controlled_joint_count }` followed by fixed-size per-tick records. `layout_hash` is a hash of the ordered `observation`/`action` field names; the trainer refuses to load a file whose hash disagrees with its compiled layout. This is the guard against the classic obs/action index-drift footgun.

### 0.2 Ground truth (verified — trust this over any earlier notes)

**`state` fields available today** (all in [Types.cppm](Game/Game/Source/Locomotion/Types.cppm), written by `state_estimator`): `pelvis_position/velocity/orientation`, `pelvis_forward/right`, `foot_position_l/r`, `foot_grounded_l/r`, `any_foot_grounded`, `double_support`, `support_min/max/center`, `com_world`, `lean_world/lean_body`, `velocity_world/velocity_body`, `capture_offset_world/body`, `capture_forward/right`, `horizontal_speed`, `pendulum_time`, `hip_angle_l/r`, `knee_angle_l/r`, `pelvis_pitch`, `pelvis_pitch_rate`.

**Three gaps you must close (obs-only, no solver change → in scope):**
1. **Ankle angles** — not surfaced. Add `ankle_angle_l/r` by reusing the existing `hinge_angle_about_x(orientation_a, orientation_b)` helper on the shin↔foot pair (exactly as hip/knee already do).
2. **Per-joint angular velocities** — none exist. Add hip/knee/ankle rates per leg by finite-differencing the measured joint angle: `(angle_t − angle_{t−1}) / dt`. Cache previous angles in `state_estimator::data`, and **zero the difference on the first valid tick after a reset** (otherwise the reset produces a one-tick velocity spike that breaks determinism once episodes reset constantly).
3. **Foot contact forces** — **out of scope for Phase 0.** They require the solver to expose per-contact impulses (physics work). Leave a named field/TODO; binary `foot_grounded_*` flags stand in for now.

**`intent` (the command/goal):** `forward`, `strafe`, `intensity`, `sprint_blend`, `desired_yaw`, `has_heading`, `sprint`, `jump`.

**`joint_drive_component`** (`target: vec3<angle>`, `stiffness: vec3<angular_stiffness>`, `damping: float`, `max_torque: torque`, `enabled: bool`) — a per-axis implicit angular servo; hinges use only axis 0 (`x`). The component is the action sink.

**The 10 driven axes** — `leg_controller::write_drives` ([LegController.cppm:785](Game/Game/Source/Locomotion/LegController.cppm:785)) writes, per tick, to the six joints whose ids live in `skeleton_refs`:

| action index | joint id (`skeleton_refs`) | `target` axis | source (`leg_pose`) |
|---|---|---|---|
| 0 | `hip_l_joint_id` | x | `pose.hip` (flexion) |
| 1 | `hip_l_joint_id` | y | `pose.hip_yaw` |
| 2 | `hip_l_joint_id` | z | `pose.hip_roll` |
| 3 | `knee_l_joint_id` | x | `pose.knee` |
| 4 | `ankle_l_joint_id` | x | `pose.ankle` |
| 5–9 | `hip_r`/`knee_r`/`ankle_r` | same pattern | `targets.right` |

`act_dim = 10`. (Knee/ankle write 0 with 0 lateral stiffness on axes y,z — those are inert, not part of the action.) Arms (joints 2,3,5,6) and toes (joints 14,15) receive a constant hold drive at spawn ([HumanoidSkeleton.cppm:528](Game/Game/Source/Shared/HumanoidSkeleton.cppm:528)) and are not modulated.

**Per-tick gain/torque switching (record as diagnostics, not action):** `write_drives` swaps hip/knee/ankle stiffness and torque caps depending on whether the leg is the swing leg / airborne. Capture these in the full-actuation record for Tier-2 replay and for later analysis of whether fixed gains suffice.

**Tick order within the smoke:** `state_estimator` (writes `state`) → `gait_scheduler` → `footstep_planner` → `balance_controller` (writes pelvis `motor_component`) → `leg_controller` (writes `joint_drive_component` + foot-anchor `motor_component`) → **[recorder goes here]** → `smoke_test` runs *earlier* in the same tick (it wrote the `intent` these consumed). Ordering is by data dependency, not registration order.

**`state_hash`** (the determinism anchor, [SmokeTest.cppm](Game/Game/Source/Locomotion/SmokeTest.cppm)): an FNV-1a fold of pelvis position, both foot positions, and pelvis velocity (12 floats) per tick. Identical run-to-run inside one binary. This is the equality oracle for Tier-2 replay.

### 0.3 The observation/action contract (the layout to freeze)

Create a new partition `gs:locomotion_mdp` ([Game/Game/Source/Locomotion/](Game/Game/Source/Locomotion/), header `export module gs:locomotion_mdp; import std; import gse; import :locomotion_types;`) and add `export import :locomotion_mdp;` to [Game.cppm](Game/Game/Import/Game.cppm) in alphabetical order.

**`observation` (v0 target, `obs_dim = 30`)** — **unit-typed** members, root-local where the quantity is spatial (translation invariance), command concatenated, phase last. `obs_dim` is the scalar count (a `vec3` contributes 3) recorded in the file header so Phase 1 can size the policy input — a count for documentation, not a packing instruction:

```
struct observation {
    gse::length pelvis_height;
    gse::angle pelvis_pitch;
    gse::angular_velocity pelvis_pitch_rate;
    gse::vec3<gse::velocity> velocity_body;
    gse::angle hip_angle_l;
    gse::angle knee_angle_l;
    gse::angle ankle_angle_l;
    gse::angle hip_angle_r;
    gse::angle knee_angle_r;
    gse::angle ankle_angle_r;
    gse::angular_velocity hip_rate_l;
    gse::angular_velocity knee_rate_l;
    gse::angular_velocity ankle_rate_l;
    gse::angular_velocity hip_rate_r;
    gse::angular_velocity knee_rate_r;
    gse::angular_velocity ankle_rate_r;
    bool foot_grounded_l;
    bool foot_grounded_r;
    gse::displacement capture_forward;
    gse::displacement capture_right;
    gse::displacement lean_body_x;
    gse::displacement lean_body_z;
    float cmd_forward;
    float cmd_strafe;
    float cmd_sprint;
    gse::angle heading_error;
    float phase_sin;
    float phase_cos;
};
```

`cmd_*` and `phase_*` are deliberately plain `float` — they are dimensionless (normalized command inputs and a unit-circle phase), not physical quantities, so unit-wrapping them would be wrong. Everything physical is unit-typed. The `ankle_angle_*` and the six joint-rate fields depend on the §0.2 estimator extensions; if the owner defers those, drop them and `obs_dim = 22`. Use **body-frame** quantities that already exist (`velocity_body`, `lean_body`, `capture_offset_body`→`capture_forward/right`) — never the `*_world` variants. `heading_error` is the existing `heading_error(state, intent)` helper. `phase_sin/cos` come from a continuous cycle phase φ ∈ [0,1) derived from `gait` (accumulate normalized progress over a full L→R→L cycle); φ is **heuristic-derived in Phase 0** and becomes clip-indexed in Phase 4 — leave a clear seam.

**`action` (`act_dim = 10`)** — unit-typed target angles. They are absolute targets in the recording (the residual-vs-absolute distinction is a *policy-output* convention; the recorded heuristic action is the absolute target it wrote, which becomes the reference the residual is measured against):

```
struct action {
    gse::angle hip_l_pitch;
    gse::angle hip_l_yaw;
    gse::angle hip_l_roll;
    gse::angle knee_l;
    gse::angle ankle_l;
    gse::angle hip_r_pitch;
    gse::angle hip_r_yaw;
    gse::angle hip_r_roll;
    gse::angle knee_r;
    gse::angle ankle_r;
};
```

**Reflected field manifest (no packing).** Phase 0 needs no float packer. Reflect over the members — `template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked())))`, the loop in `gse::shaders::emit_slang_struct` ([ShaderCodegen.cppm](Engine/Engine/Source/Gpu/Shader/ShaderCodegen.cppm)) is the template — only to generate the recording's **field manifest**: the ordered member names, the scalar count (`obs_dim`/`act_dim`, summing a `vec3` as 3), and a `layout_hash<T>()` (FNV over the ordered names) the loader checks so a struct change can't silently desync from an older recording. The structs are layout-compatible PODs, so the recorder writes them **as-is** (a fixed-size binary record = the struct bytes); there is no unit→float conversion anywhere in Phase 0. The eventual policy-input view (Phase 1) is layout-compatible pass-through, like GPU push constants — not a packer, and not a Phase-0 concern.

### 0.4 Ordered subtasks

1. **P0.1 — `observation`/`action` + field manifest.** Create `gs:locomotion_mdp` with the two unit-typed structs and the reflection-generated manifest (`field_names<T>()`, the scalar count, `layout_hash<T>()`). No packer. *Verify (Tier-1):* a tiny self-check (a test entry or a temporary assert behind the record flag) writes a record with known unit values to a byte buffer and reads it back, asserting the unit values round-trip equal — i.e. serialize/deserialize is lossless; same for `action`.
2. **P0.2 — estimator extensions** (skip if owner defers per §0.7). In [Types.cppm](Game/Game/Source/Locomotion/Types.cppm) add `ankle_angle_l/r` (`gse::angle`) and the six joint-rate fields (`gse::angular_velocity`) to `state`. In [StateEstimator.cppm](Game/Game/Source/Locomotion/StateEstimator.cppm) compute ankle angles via `hinge_angle_about_x(shin, foot)`, and joint rates via finite difference with a previous-angle cache in `data` and a reset-zeroing guard. *Verify:* `state_hash` is unchanged (these are new fields, not hashed) and the new fields read sane in a logged tick.
3. **P0.3 — `observe()` mapping.** In `gs:locomotion_mdp`, write `observe(const state&, const intent&, const gait&) -> observation` — copy/derive the **unit-typed** fields straight across (body-frame variants `velocity_body`/`lean_body`/`capture_*`, the `heading_error(state, intent)` helper, the derived φ). `observe` returns unit-typed values; nothing in Phase 0 strips units.
4. **P0.4 — action capture mapping.** Write `action_from_drives(const skeleton_refs&, gse::read<joint_drive_component>&) -> action` (pull `target.x/y/z` from the six joints) and the inverse `apply_action(const action&, const skeleton_refs&, gse::write<joint_drive_component>&)` for replay.
5. **P0.5 — recorder system.** Create partition `gs:locomotion_recorder` with a system `struct recorder { struct [[= gse::settings::category<"Recorder">{}]] data { bool enabled = false; std::string path; …prev-state for φ/rates if needed…; std::ofstream out; }; static auto run(data&, gse::read<skeleton_refs>, gse::read<state>, gse::read<intent>, gse::read<gait>, gse::read<gse::physics::joint_drive_component>, gse::read<gse::physics::motor_component>) -> gse::async::task<>; };`. On each tick where `enabled && state.valid`, build `observe(...)`, `action_from_drives(...)`, and a **full-actuation record** (the 10 targets + per-tick stiffness/torque caps from the drives + the motor writes) + **reference kinematics** (root position/orientation, root linear+angular velocity, the measured joint angles available, φ, contact flags), and append a fixed-size record (the unit-typed structs written as bytes — they are layout-compatible PODs, so no conversion) to `out`. Open the file lazily on first enabled tick (header per AD-6), write incrementally (RAII close). *Reference-kinematics caveat to encode:* measured 3-axis hip angle is **not** available (only flexion via `hinge_angle_about_x`); Phase 0 records the **commanded targets** as the reference joint trajectory and the measured flexion angles where available, and marks full-3-axis-measured-angles as a known gap for Phase 4.
6. **P0.6 — wire into the smoke.** Add `bool locomotion_record = false;` and `std::string locomotion_record_path;` to `gs::startup::config` ([Main.cpp:7](Game/Game/Source/Main.cpp:7)), register `e.add_system<gs::locomotion::recorder>(…)` in `run_locomotion_smoke` right after `leg_controller` ([Main.cpp:46](Game/Game/Source/Main.cpp:46)), and have `smoke_test` enable recording only during the `running` stage (so warmup/reset ticks are excluded) — communicate the active flag/trial/φ via the recorder's `data` or a channel request, mirroring how the smoke already drives state. Confirm the flag parses (`--locomotion-record`); if the reflection arg-parser mangles it, match the exact spelling it derives from the member name.
7. **P0.7 — reference-set artifact (D bootstrap).** Confirm the written file is the first reference set: a clean settle→walk→turn→sprint→stop sequence of full kinematics, loadable by `obs_dim`/`act_dim`/`layout_hash` from the header. Document the record schema in a short `docs/` note or a header comment-free struct so Phase 2 can read it.
8. **P0.8 — Tier-2 replay validation (recommended).** Add a `--locomotion-replay <file>` path that, instead of registering the five controllers, registers a replay system that each tick reads the next record and applies the **full actuation** (joint targets/gains/caps via `apply_action` + the recorded motor writes), runs `state_estimator` + `smoke_test` for metrics, and asserts the live `state_hash` equals the recorded one on trials 2–5. A mismatch means the recording missed an actuated DOF — fix what's missing.
9. **P0.9 — stage for review.** Summarize the frozen `obs_dim`/`act_dim`/`layout_hash`, the file schema, and the Tier-1/Tier-2 results. Do **not** build or commit — hand back to the owner for the gate run.

### 0.5 Acceptance criteria

- New partitions `gs:locomotion_mdp` and `gs:locomotion_recorder` exist, exported from [Game.cppm](Game/Game/Import/Game.cppm), styleguide-clean.
- `--locomotion-record <path>` writes a versioned file; header carries `obs_dim`, `act_dim`, `layout_hash`, `fixed_dt`.
- **Tier-1:** serialize→deserialize of a record is exactly lossless for both structs (unit values preserved).
- **Tier-2 (recommended):** replay reproduces `state_hash` on trials 2–5.
- **No regression:** an ordinary `--locomotion-smoke` run (recording off) is still 5/5 with an unchanged `state_hash` for trials 2–5 (the recorder must be a pure read-only observer when enabled, and absent from the hash path).

### 0.6 Out of scope (do not do these in Phase 0)

Any VBD/solver change; foot contact-force exposure; per-body friction; the `env_id` broad-phase work; any neural network, autodiff, libtorch, or Python; reward functions; multi-env anything; the trial-1 flake fix and the reset-mid-fall crash (those belong to Component A.3 / B). If a subtask seems to require one of these, stop and flag it — it means the layout is reaching past the seam.

### 0.7 Open questions for the owner

- AD-2 (action scope = 10 axes, fixed gains, motors excluded — and the "optimistic reference" consequence), AD-4 (Tier-2 required or optional?), AD-5 (ratify libtorch as the learning-half target).
- Should the reference set also be recorded from an **in-game** play session (real player intent), or is the **scripted smoke** sequence sufficient as the first reference? (Recommend: smoke first; it is deterministic.)
- `obs_dim = 30` (with estimator extensions) vs `22` (without) — approve the estimator extensions as in-scope Phase 0 work?

---

## Phase 1 — detailed implementation plan

The second roadmap step: stand up the **whole RL loop on a single CPU environment** and prove it on a trivial task. The goal is *correctness of the loop*, not speed — one humanoid, slow is fine. When this phase is done, a seeded run shows reward climbing on a trivial task, and a reloaded checkpoint reproduces the behavior through the smoke gate. No batching, no GPU, no AMP, no mocap.

**Definition of done:** `run_locomotion_train` runs headless and deterministic; on the trivial task (stand/balance) the reward curve rises for a fixed seed; a saved checkpoint, reloaded with the learning half *off*, holds the task to timeout; and the loop's per-seed trajectory is reproducible (a regression hash like the smoke's). The existing controllers-on smoke stays 5/5 and bit-identical (the train path is separate).

### 1.0 Preconditions — close the Phase 0 gaps first

P0 delivered the data types and capture logic but is **not runnable or validated**. Close these before (or as the first part of) Phase 1 — Phase 1 depends on a working recording/replay and a trusted seam:

- **P1.0a — Make recording runnable.** Add `locomotion_record` (bool) + `locomotion_record_path` (string) to `gs::startup::config` (mirror `locomotion_smoke`), thread them into `run_locomotion_smoke`, and set the recorder's `enabled`/`path` at registration. **Move `enabled`/`path`/`out` OUT of the `[[= settings::category]] data`** — an `std::ofstream` is immovable and a per-run path is not a setting; make them plain runtime fields (or hold the stream behind a `std::unique_ptr`). Gate writes to the smoke's `running` stage so warmup/reset ticks are excluded (communicate the stage via a shared field or a channel request, as the smoke already drives `intent`).
- **P1.0b — Tier-1 self-check.** Serialize→deserialize one `observation` and one `action` record and assert the unit values round-trip — the proof the manifest + POD write are lossless.
- **P1.0c — Tier-2 replay (recommended, the real proof).** A `--locomotion-replay <file>` path that drops the five controllers and each tick applies the **full recorded actuation** — targets via `apply_action` **plus** `enabled`/`stiffness`/`max_torque` from the diagnostics **plus** the pelvis `motor_component` — then runs `state_estimator` + `smoke_test` and asserts the `state_hash` matches trials 2–5. Note `apply_action` today writes only `.target`; replay must also restore the gains and the motor, or the body won't move.
- **P1.0d — Eyeball one reference file** (the Component D bootstrap): a clean settle→walk→turn→sprint→stop trajectory, header dims/hashes sane.
- **P1.0e — Fix/guard the reset-mid-fall crash.** It becomes *blocking* here: Phase-1 episodes end by *falling*, so every reset exercises the null-deref path. Either fix the null deref (preferred) or settle the body to rest before reactivating the scene.

### 1.1 Architectural decisions (resolve first; recommendations are the default)

**🚩 AD-7 — Learning-half home (this is AD-5 coming due — it must be *built* now).** *Recommended:* **libtorch wrapped in a `gse.ml` module** (per the never-`#include`-third-party rule, same pattern as `gse.vulkan`), in-process with the sim, with the **trainer realized as an engine system** (AD-8). Autograd + Adam + checkpointing come for free, and obs/action never leave the process. *Fallbacks:* a **Python bridge** (expose the env over a C-ABI/shared-memory and drive a battle-tested PPO — fastest to a *first* learning result, but out-of-process and breaks the in-engine elegance) and a **hand-rolled C++ PPO** (dependency-free, but you'd debug your own RL math *and* the new env at once — violates "never debug two unproven things"). **Flagged hard:** libtorch is a real dependency with build-system risk on gcc-trunk modules/BMIs — the owner should ratify before the worker starts, since all of Phase 1 hangs off it. (Inference at ship time is hand-rolled regardless — keep the forward pass separable from the training code.)

**AD-8 — The env *is* the headless engine tick loop (do it).** Don't build a separate "gym" abstraction for a single env. `step` = one engine tick; `obs` = `observe()` called inside a new `trainer` system; `action` = the policy forward pass; `reset` = scene deactivate→reactivate (reuse the smoke harness). The gym semantics fall out of a `trainer` system that mirrors `smoke_test`'s structure. PPO updates run inline every `rollout_length` ticks (blocking the tick is fine headless).

**AD-10 — Fixed PD gains; the env must enable the drives (do it, easy to miss).** Per AD-2 the action is a target *residual* with **fixed** gains. Critical detail: `spawn_humanoid` creates the six controlled-joint drives **`enabled = false` with zero stiffness/torque** — the heuristic `write_drives` is what turns them on each tick. With the controllers gone, the `trainer` must, on reset, set `enabled = true` + fixed stiffness/torque on the 10 axes (reuse the heuristic's load-bearing values: hips 600, knees 650, ankles 400 N·m/rad; caps ~380/420/160 N·m) and then write *only* targets via `apply_action`. Otherwise the policy's actions do nothing.

**🚩 AD-9 — Does the trivial task keep a balance assist?** A freshly-initialized policy falls instantly (fine for RL). The open question is whether *joints-only* balance is even learnable on this marginal rig. *Recommended:* start **motors off** (pure-joint) so we learn whether the pivot's premise holds; if learning stalls, add a small pelvis assist as a curriculum crutch and wean it. Flagged because it touches the core "can the policy be the balance" thesis.

### 1.2 The environment (`run_locomotion_train`)

Mirror `run_locomotion_smoke` ([Main.cpp:38](Game/Game/Source/Main.cpp:38)): `set_fixed_step_override(1)`; `gse::start(setup, { .create_window = false, .render = false, .persist_settings = false })`. In the setup lambda **keep** physics + `pose_driver::system` + `state_estimator`, **drop** `gait_scheduler`/`footstep_planner`/`balance_controller`/`leg_controller`, and add the `trainer` system. `world_loader_setup` → activate the sandbox scene exactly as the smoke does.

The `trainer` system each tick (it owns the policy, the rollout buffer, the optimizer, the RNG, and the episode state):
1. read `state` (written by `state_estimator` this tick); build `obs = observe(state, neutral_intent, default_gait, phi)` (for the stand task `intent`/`gait`/φ are inert — feed defaults);
2. sample `a ~ πθ(obs)` (seeded RNG), record `logπ` and the critic value;
3. `apply_action(a, refs, drives)` (drives already enabled with fixed gains from reset);
4. on the *next* tick, compute `reward` and `done` from the resulting `state`, and store the transition;
5. on `done` (fall or timeout) push a scene-reactivate request and re-enable/re-gain the drives;
6. every `rollout_length` steps, run the PPO update on the batch; checkpoint every `K` updates.

**Determinism:** seed the action-sampling RNG; a fixed seed must give a reproducible reward curve and a reproducible trajectory hash (the Phase-1 regression oracle, generalizing the smoke's `state_hash`).

### 1.3 The learning half

If **libtorch/`gse.ml`** (recommended): actor + critic MLPs (2–3 hidden layers, `tanh`), a Gaussian policy (diagonal log-σ), PPO with GAE(λ), clipped surrogate, value loss, entropy bonus, Adam. Checkpoint = a weights blob. Keep the **inference forward pass** in a form that can later be re-implemented dependency-free (Phase 5 / Component F) — don't entangle it with the training graph. If the module wrapper fights gcc-trunk, fall back to the Python bridge to unblock *validating the env*, and revisit the in-engine port once the loop is proven.

### 1.4 The trivial task + reward

- **Task A (do first): stand / balance** from the neutral spawn. `reward = w_up·upright(pelvis height near target, small tilt) + alive_bonus − w_energy·effort − w_smooth·‖a − a_prev‖`; `done` = pelvis below the gait fall threshold *or* timeout (~10 s). Success = upright to timeout. This alone exercises obs-sufficiency, the action seam, reward, advantage, update, and checkpointing.
- **Task B (after A passes): track a constant forward velocity** — add a tracking term on `velocity_body.z` vs a target.
- Keep terms few and interpretable; this is loop validation, not final reward design (that's Phase 2 / Component D). **Change the sim *or* the reward, never both between eval runs** (the smoke-era rule, generalized).

### 1.5 Ordered subtasks

1. **P1.0** — close the Phase 0 preconditions (1.0a–1.0e). Gate: a recording is produced, Tier-1 passes, reset survives a fall.
2. **P1.1 — `gse.ml` (or the chosen AD-7 home).** Stand up actor/critic MLPs + a forward pass + Adam + checkpoint save/load, validated on a throwaway unit (e.g. fit `y = 2x`) so the ML plumbing is trusted before it meets the env.
3. **P1.2 — `run_locomotion_train` + the `trainer` system** (AD-8): the headless harness, drives enabled with fixed gains on reset (AD-10), the per-tick obs→action→step→reward→store loop, scene-reactivate reset.
4. **P1.3 — reward + done for Task A**, plus the seeded RNG and the trajectory regression hash.
5. **P1.4 — PPO update + GAE**, rollout buffer, periodic checkpoint.
6. **P1.5 — train Task A to standing**; confirm reward rises for a fixed seed and a reloaded checkpoint stands to timeout with the learning half off.
7. **P1.6 — (optional) Task B**, then stage for review. Do **not** build or commit — hand back for the gate.

### 1.6 Acceptance criteria

- `run_locomotion_train` is headless, fixed-step, deterministic per seed (reproducible reward curve + trajectory hash).
- Reward climbs on Task A; a reloaded checkpoint holds the stand to timeout with no training code in the loop.
- The controllers-on `--locomotion-smoke` run is untouched: still 5/5, `state_hash` bit-identical for trials 2–5.
- The inference forward pass is separable from the training graph (no ship-time ML dependency implied).

### 1.7 Out of scope (Phase 2+ / Components A, B, D)

Batching, GPU, `env_id` isolation, the GPU jointed-pair filter, the cheap per-env GPU reset, multi-env, on-GPU obs/action, domain randomization, reference-state initialization (RSI), AMP/mocap, and final reward shaping. Single CPU env, one trivial task, trusted-ML-plumbing-first. If a subtask seems to need any of the above, stop and flag it.

### 1.8 Open questions for the owner

- **AD-7:** ratify **libtorch/`gse.ml`** as the learning-half home (it must be built in Phase 1), or prefer the Python bridge to validate the env first?
- **AD-9:** trivial task **joints-only** (test the pure-physics balance thesis) vs **with a small assist** (guaranteed-learnable, weaned later)?
- `rollout_length`, episode timeout, and checkpoint cadence — pick now or let the worker choose defaults and report?

---

## Phase 1 — close-out & validation gate (the actual next step)

Phase 1 is **done**: the single-env loop is proven (builds, crash-free, deterministic), and after a tuning pass the **policy demonstrably learns the stand task** (2026-06-14). The value function fits returns (critic loss 702→89) and the stand policy improves with training — episodes lengthen ~130→~200–316 steps and reward climbs ~385→~600–970 over 117 updates. The lever was **exploration noise, not a crutch**: the Gaussian `log_std` init was −0.5 (σ≈0.6 rad ≈ 35°/joint/step — enough to topple a balancing humanoid every step); dropping it to −1.5 (σ≈0.22 rad) let the policy survive long enough to get a gradient. No AD-9 assist was needed. The reset-mid-fall crash is fixed (in-place reset) and the smoke still passes (trials 2–5 bit-identical). Full *mastery* (standing to the 1000-step timeout) isn't reached on one CPU env — that's throughput-bound and is exactly what Phase 2/3 unlock.

**Status — PHASE 1 DONE & VERIFIED (2026-06-14): builds green, crash-free, deterministic, and the policy demonstrably learns.** Selftest `PASS`; `--locomotion-train` runs with **no crash**, **critic loss 702→89**, and (after dropping exploration `log_std` −0.5→−1.5) the stand reward **climbs** ~385→~600–970 with episodes lengthening ~130→~316 steps; `--locomotion-smoke` trials 2–5 pass bit-identical (`state_hash=4352de497c7a2a54`; trial 1 = the known flake). All close-out items resolved. **Next: Phase 2 (CPU-parallel envs)** for the throughput to reach full stand mastery and then a walk — the loop *and* the learning are both proven, so it is no longer "scaling an unproven loop."

Ordered by severity:

1. **✅ FIXED — reset-mid-fall crash (was P1.0e).** Verified reproducible (intermittent SEH `access_violation`, read@0x8, after ~94 resets), then fixed by **replacing the scene-teardown reset with an in-place reset**: the trainer snapshots every body's transform once and, on episode end, restores transforms + zeros velocities. This is provably clean because the CPU VBD solver **rebuilds body state from `transform_component`/`motion_component` every frame** (`old_position = tc->position`, `velocity = mc.current_velocity`; `id_to_body_index` rebuilt too) — so a teleport produces no velocity spike. The scene is never deactivated, so the crashing teardown path is gone. Verified: 1315 episodes, 0 crashes; also faster + previews the Component A.3 cheap reset. *(The underlying null-deref still lives in the physics scene-teardown path — left for Component A.3; the trainer just avoids it.)* This work also surfaced + fixed a **scheduler dependency cycle**: the recorder's `shared_view<smoke_test>` stage-gate made `recorder → smoke_test → recorder`, aborting the smoke at startup *even with recording off* (it's a schedule-build edge, not runtime) — removed the `shared_view`; the recorder now records whenever `enabled` (trim warmup ticks in post).
2. **✅ DONE — make the loop deterministic.** Replaced `rng(std::random_device{}())` with a seed from `ppo_config` (default fixed). Add a per-seed regression signal — reuse the smoke's `state_hash` approach over the trainer's trajectory, or log a reward-curve checksum — so a run is reproducible. This is a stated Phase-1 acceptance criterion.
3. **✅ DONE — update on rollout-full, not every episode.** Trigger changed from `if (done || buf.full())` to update **only when `buf.full()`**; on `done`, reset and keep accumulating into the same buffer (the `dones[]` flags + the `bootstrap_value` already make GAE correct across episode boundaries). Updating on every fall trains on tiny, high-variance batches and will mask whether the loop actually learns.
4. **✅ DONE — units in `compute_reward` / `episode_done`.** Bare float literals replaced with unit constants (`gse::meters(0.9/0.4)`, `gse::degrees(45)`); only the dimensionless reward scalar is `static_cast` to float. *(Nice-to-have left: have `done` reuse the gait scheduler's actual fall threshold instead of a local `gse::meters(0.4)`.)*
5. **✅ DONE — Tier-1 validation.** `--locomotion-selftest` (`gs::locomotion::locomotion_selftest`) asserts the obs/action scalar dims (30/10), the `pack`/`unpack` slot values, and non-zero `layout_hash`. *(Run it as step (a) above.)*
6. **✅ `std::linalg` removed (done in source).** Confirmed absent from libstdc++-trunk (build error) — the three ops are now plain loop helpers (`linear`, `linear_transpose`, `rank1_add`) over the existing dynamic buffers; the lint comments are gone. **CPU-perf lever for Phase 2:** if per-env step time matters at 32–64 envs, vectorize those three loops with the engine math lib's container-agnostic SIMD — the forward dot-product reduction is the one loop `-O3` won't auto-vectorize without `-ffast-math`. Not worth it for single-env Phase 1; the real throughput lever is the Phase 3 GPU batch (and the fixed-extent, col-major engine *matrices* are the wrong fit for a dynamic-width MLP — loops/SIMD, not those).
7. **🟡 Sanity-test the PPO independently (recommended).** Before trusting it on the env, fit the MLP to a trivial target (e.g. `y = 2x`) so the RL math is validated separately from the sim — the "never debug two unproven things" rule. And do the Tier-2 replay (was P1.0c) if not already: a `--locomotion-replay` path that re-applies the recorded full actuation and matches the smoke `state_hash`.
8. **✅ Loop proven AND policy demonstrably learning.** `--locomotion-train` runs crash-free + deterministically; PPO fires on full rollouts; **critic loss falls 702→89**; and after the exploration-noise fix the **policy improves with training** — episodes ~130→~200–316 steps, reward ~385→~600–970 over 117 updates. Root cause of the earlier flat reward: the Gaussian `log_std` init was −0.5 (σ≈0.6 rad ≈ 35°/joint/step), which toppled the balance before any gradient formed; dropped to −1.5 (σ≈0.22 rad). **No AD-9 assist needed.** Full mastery (1000-step timeout) is throughput-bound → Phase 2. *(`log_std` init is currently a literal in `actor_make`; promote to `ppo_config` when convenient. A per-seed trajectory regression hash is still a nice-to-have.)*

### Decisions to ratify

- **AD-7 (resolved — keep hand-rolled):** owner ratified 2026-06-14. The hand-rolled PPO/MLP stays — it's correct, dependency-free (Component F's "zero ship-time ML dep" is already satisfied), and proven to learn. Revisit only if Phase 4's AMP discriminator makes hand-written backprop too costly to maintain.
- **AD-9 (resolved):** joints-only **does** learn the stand once exploration noise is sane (`log_std` −1.5) — no balance assist was needed, which is a clean win for the pivot thesis (the policy *is* the balance). Keep joints-only.

---

## Phase 2 — detailed implementation plan

**Implementation status (2026-06-15): all three review bugs fixed; 2a validated; 2b walk locked in (partial tracking — gait quality deferred to Phase 4).** A worker implemented Phase 2 (grid spawn in `SandboxScene.cppm` via `world_training_setup` + `--locomotion-train` using `cfg.ppo.n_envs`; `trainer` extended to N envs with per-env `env_state` + per-env in-place reset). The three review bugs below were then fixed and the run validated.

**2a (stand@N) validated** (2026-06-15): 32-env, critic loss 499→~120, episode reward 200→580, episode length 73→197 steps, 1725 resets, no crash, deterministic seed.

**2b (velocity walk) — locked in, partial tracking.** Per-env velocity command (forward 0.25–0.55 m/s, strafe ±0.1 m/s, resampled each episode) is fed through `intent` into `observe()`; reward = `2.0·exp(−4·‖v_body−cmd‖²) + 0.5·upright + 0.3·heading + 0.1·alive`. A first attempt that kept the stand reward's survival floor (`alive`+`upright`+`height`) let the policy farm survival and ignore the command (`track_err` flat ~0.62); the tracking-dominant reward above fixed that — `track_err` falls 0.75→~0.57 (best ~0.40), critic loss 361→~25, reward climbs — but it walks only *roughly* toward the command (~0.55 m/s error on ~0.4 m/s commands; plateaus). Collision-safe on CPU via 8 m grid spacing + `max_steps` 400 (no `env_id` filter until Phase 3); a per-episode `track_err` (mean ‖v_body−cmd‖) is logged. **Decision (2026-06-15): tight velocity tracking via pure reward shaping is deferred — gait quality is Phase 4's job (imitation / AMP), which supersedes hand-shaped tracking.** Phase 3 (GPU batch) is now unblocked — the GAE gate is cleared.

Review findings (all resolved 2026-06-15):

- ✅ **GAE per-env (was: computed across env boundaries).** Added `env_of` (a per-transition env tag) to the rollout buffer + a per-env `bootstrap`; `compute_gae` now gathers each env's transition indices in buffer order and walks them independently with that env's bootstrap. Capacity 1024 = exact multiple of n_envs 32, so the buffer fills on a clean tick boundary and each bootstrap = V(s_last); the gather is robust to uneven counts regardless.
- ✅ **Full 17-bone reset (was: 10 of 17).** `skeleton_refs` now carries `all_bone_ids` (= `skeleton_handle.bone_ids`, captured at spawn); `take_env_snapshot` iterates all 17 so arms/head reset cleanly too.
- ✅ **Unique per-env entity names (the bug that made multi-env never actually run).** `spawn_humanoid` takes a `name` param (sets `rig.skel.name`; `spawn_skeleton` derives entity names as `"{skel.name}.{bone}"`), so N humanoids no longer alias onto one id — the startup `Cannot activate` assert is gone.
- ✅ **Hand-formatted to the styleguide** (definitions single-line, declarations wrapped one-per-line; no vertical alignment); `g_training_n_envs` inlined.

The rest of this section is the original plan (still valid for the per-env-stream fix and the remaining work).

---

Phase 2 turns the proven single-env loop into **N parallel CPU environments** and trains the **first real locomotion** — a velocity-and-heading-conditioned walk — validating obs/action/reward/termination/RSI on a real task. A positive result here is what justifies the Phase 3 GPU investment. Days-per-run is acceptable; this is design validation, not the fast path.

**Definition of done:** `--locomotion-train` runs **N ≈ 32–64 humanoids in one scene** (one engine tick advances all), deterministic per seed, crash-free; and a **velocity-and-heading-conditioned walk policy** demonstrably improves — tracks a commanded forward speed while staying upright, reward climbing. The smoke still passes (trials 2–5 bit-identical). **Scope locked 2026-06-14: Phase 2 ends at the velocity walk; imitation + RSI (former stage 2c) move to Phase 4**, where the mocap/AMP reference pipeline lives — making the walk *look* like the reference is that phase's job, and it avoids bundling reference/RSI machinery with the parallel-env validation.

**Governing rule (unchanged): never debug two unproven things at once.** So Phase 2 is staged — 2a proves *parallelization* on the already-proven stand task; 2b proves the *walk reward* on proven parallel infra.

### 2.0 What Phase 1 leaves you (build on this)

- Single-env loop proven: `trainer` (`gs:locomotion_trainer`), hand-rolled PPO/MLP (`gs:locomotion_nn`), the obs/action `memcpy` seam (`gs:locomotion_mdp`), in-place reset, deterministic, learns the stand (`log_std −1.5`).
- The recorder (`gs:locomotion_recorder`, `--locomotion-record <path>`) works — it's how you produce the reference set (2c).
- `spawn_humanoid(scene, pos, orient) -> skeleton_handle` returns `bone_ids` (all 17 bodies) and `joint_ids` (all joints); the **owner entity is `bone_ids[0]` (pelvis)**, and `skeleton_refs` + the locomotion components are attached there. **Copy the exact wiring from [SandboxScene.cppm:55-90](Game/Game/Source/Sandbox/SandboxScene.cppm:55).**
- `state_estimator` already iterates *all* entities with `state` — it produces N states with no change once N humanoids exist. The trainer is the part that must become N-aware.

### 2.1 Architectural decisions (flagged)

**AD-11 — parallel envs = N humanoids in one scene on a spatial grid (recommended).** Spawn N humanoids spaced far enough apart that the broad phase never pairs them (e.g. a √N×√N grid at ~4 m spacing). One engine tick advances all N — the CPU VBD solver already parallelizes across bodies (`task::parallel_invoke_range`) and independent humanoids are disconnected constraint-graph components, so **no solver change**. The RL collects N transitions/tick. Reuses the whole Phase-1 loop and previews Phase 3 (same disconnected-worlds idea, on GPU). Isolation = **spatial offset** (Component A.1 prototype); the principled `env_id` broad-phase is Phase 3. *Flag:* float precision degrades far from origin — keep the grid ≤ ~50 m extent for 64 envs (fine for CPU).

**AD-12 — policy batching: loop N forwards now.** Call the hand-rolled MLP once per env per tick. For N≤64 on a 30→128→128→10 net that's microseconds — correctness first. Batching (matrix-matrix) + your container-agnostic SIMD is the perf lever if per-tick time bites; the real throughput win is Phase 3 GPU. **Recommend loop-N.**

**AD-13 — the reference set (Component D bootstrap) — Phase 4 (deferred with 2c).** When built: `GoonSquad --locomotion-smoke --locomotion-record <path>` (the recorder captures obs/action/full-actuation/`reference_kinematics` per tick while `enabled`). Write a loader (check the header's `obs_dim`/`act_dim`/`layout_hash`, reject on mismatch) that indexes frames by the recorded phase φ. *Flag:* scripted-smoke reference (deterministic, easy) vs in-game player intent (varied commands) — recommend scripted-smoke first.

**AD-14 — reward, staged.** 2b task reward (Phase 2) = track commanded velocity (`velocity_body` vs a per-episode target) + heading + upright + alive + a small effort/smoothness penalty. **Phase 4** adds the DeepMimic imitation term — weighted pose / joint-velocity / end-effector / CoM error vs the reference at current φ. *Flag:* reward weighting is the iteration-heavy part (human-days) — change the reward **or** the sim, never both between eval runs.

**AD-15 — RSI (reference-state initialization) — Phase 4 (deferred with 2c).** On reset, set each env to a *random frame of the reference* (pose + velocities), not always the neutral stand — the single biggest stabilizer for imitation. Extends the in-place reset to write bodies to a reference frame's transforms. *Flag:* the recorder's `reference_kinematics` currently has root pose + measured leg-joint angles, not full per-bone transforms — either extend the recorder to capture all 17 bone transforms for exact RSI, or approximate (set root + joint-drive targets, let a few settle steps run).

### 2.2 The work — staged

- **Stage 2a — N parallel envs, re-prove the STAND.** Grid-spawn N humanoids; make the trainer loop all N owners each tick (per-env obs/action/transition/episode-state/in-place-reset using that env's `bone_ids`); one shared seeded RNG iterated in fixed env order. Keep Phase-1's stand task/reward. **Gate:** stand still learns at N=32–64, deterministic, crash-free, ~N× throughput. Isolates the parallelization.
- **Stage 2b — velocity-and-heading walk.** Randomize a target speed/heading per env-episode through the existing `cmd_*`/`heading_error` obs; reward tracks it + upright + alive (+ effort). No imitation yet. **Gate:** commanded-velocity tracking error falls, reward climbs — the "first real learning run."
- **Stage 2c — imitation for naturalness → DEFERRED to Phase 4** (decided 2026-06-14). The design — load a reference, add the DeepMimic imitation reward + RSI so the walk resembles the heuristic gait — is retained in AD-13/AD-15 and subtasks P2.4–P2.5 below as Phase 4's starting point; it is **not built in Phase 2**.

### 2.3 Ordered subtasks

1. **P2.1 — grid spawn.** `spawn_training_grid(scene, n)`: N humanoids at grid offsets (reuse the [SandboxScene.cppm:55-90](Game/Game/Source/Sandbox/SandboxScene.cppm:55) wiring), a ground plane spanning the grid, returns per-env `{owner_id = bone_ids[0], bone_ids}`. Call it from `run_locomotion_train` instead of the single-humanoid sandbox. *Verify:* `state_estimator` reports N valid states.
2. **P2.2 — trainer over N envs.** Convert single-owner logic to per-env arrays (obs/action/prev/episode-state indexed by env); collect N transitions/tick; per-env done + in-place reset (restore that env's `bone_ids` + zero motion). **Gate 2a.**
3. **P2.3 — command sampling + walk reward (2b).** Per-env-episode random target velocity/heading into `intent`; reward tracks it + upright/alive/effort. **Gate 2b.**
4. **P2.4 — determinism + gate + stage for review.** Per-seed reproducibility (reward-curve / trajectory hash); re-gate `--locomotion-smoke` (trials 2–5 unchanged). Do **not** build or commit without the owner.

**Deferred to Phase 4** (kept here as the design starting point): *reference produce + load* (record the heuristic gait, write the loader + `layout_hash` check, index by φ) and *imitation reward + RSI* (pose/vel/EE/CoM imitation vs reference@φ; RSI to random reference frames).

### 2.4 Acceptance

- N=32–64 humanoids in one scene, one tick advances all, deterministic per seed, crash-free.
- Stand re-proven at N (2a); commanded-velocity walk learns (2b). *(Imitation / RSI is Phase 4.)*
- `--locomotion-smoke` trials 2–5 still pass, bit-identical.

### 2.5 Out of scope (Phase 3+)

GPU batching, the `env_id` broad-phase, the GPU jointed-pair filter, on-GPU obs/action, **imitation reward, RSI, AMP / mocap (all Phase 4)**, per-body friction / domain randomization beyond command sampling. Spatial-offset CPU envs only. If a subtask seems to need one of these, stop and flag it.

### 2.6 Open questions for the owner

- **Resolved (2026-06-14):** Phase 2 stops at 2b (parallel envs + velocity walk); imitation + RSI move to Phase 4.
- Still open — **Phase 2:** N (32 vs 64) and grid spacing. **Phase 4:** AD-13 reference source (scripted-smoke vs in-game); AD-15 exact-RSI (extend the recorder to full per-bone transforms) vs approximate-RSI.

---

## Phase 3 — detailed implementation plan

Phase 3 is the **throughput lift**: move the batched humanoid sim onto the **GPU** so a training step advances **thousands** of envs at once, taking training runs from days to **minutes–hours**. The bet (earned in Phases 1–2): the RL math is already proven on CPU, so this phase debugs **only the GPU sim** — never the sim and the RL together. It is the **highest-uncertainty phase** in the roadmap (VBD convergence at batch scale, GPU memory, determinism across the batched dispatch).

**Definition of done:** the articulated humanoid runs on `gse::vbd::gpu_solver`; thousands of envs train in one batched dispatch; a velocity walk learns in minutes–hours; determinism preserved (a sim change that shouldn't alter dynamics doesn't move a batched `state_hash`); the CPU path still passes the smoke.

### 3.0 Preconditions (HARD GATE)

- **Phase 2 must be fixed and proven first.** The multi-env **GAE bug** (review finding above) must be fixed and a CPU-parallel **velocity walk shown to learn**. Do not build GPU batching on an unproven/incorrect RL loop — that reintroduces the exact "two unproven things at once" trap this whole roadmap avoids.
- **Engine debt on the critical path** (from the baseline notes): the GPU narrow phase lacks the **jointed-pair contact filter**, which is *why the articulated humanoid runs on CPU today* — this is the first thing Phase 3 must fix (3a). The reset-mid-fall crash is already side-stepped by the in-place reset, but its GPU analogue (3c) must be cheap and per-env.

### 3.1 Architectural decisions (flagged)

**AD-16 — env isolation: spatial-offset prototype → `env_id` broad-phase.** The spatial grid from Phase 2 already isolates envs (no cross-env contacts if spaced apart); it ports to GPU unchanged as the *prototype* to get batching working early. The principled path is **`env_id` on `body_state`** (a `[[= shaders::shader_struct]]` type, so adding the field propagates to the GPU buffer layout via reflection automatically) + rejecting candidate pairs with mismatched `env_id` in the broad phase (`vbd_broad_phase_stage` / `collision_broad_phase_pipeline`). Recommend: spatial-offset first to unblock, then `env_id` to remove the float-precision / grid-memory blow-up at thousands of envs.

**🚩 AD-17 — the policy↔GPU seam (consequence of AD-7 hand-rolled CPU PPO).** With the sim on GPU and the policy a hand-rolled **CPU** MLP, naïvely each step copies obs GPU→CPU and actions CPU→GPU — a PCIe round-trip per step, which is the real throughput ceiling for in-engine RL (not FLOPs). Two options: **(i)** accept the round-trip first (obs is small — 30 floats × N; simplest, get batching working), then **(ii)** port the **inference forward pass** to a GPU compute shader (the engine already has compute pipelines; the MLP is tiny matmuls + tanh) so obs/action/inference stay on-device, with PPO *training* (backprop) still on CPU via periodic weight upload. **Flag:** (ii) is the throughput payoff but real work; decide when the round-trip actually bottlenecks. (This is also where libtorch-on-GPU would have helped — but AD-7 is settled hand-rolled, so on-GPU = a compute-shader forward pass.)

**AD-18 — determinism across the batched dispatch.** Per-env seeded RNG `(base_seed, env_id, episode)`; a `state_hash` generalized across envs for regression. GPU floating-point reductions/atomics can be non-deterministic — budget for making (or accepting bounded non-determinism in) the batched solve, and keep the bit-exact discipline as the debugging substrate.

### 3.2 The work — staged (each stage gated)

- **Stage 3a — run the articulated humanoid on the GPU solver at all.** Implement the **jointed-pair contact filter in the GPU narrow phase** (`vbd_narrow_phase_stage` / `collision_narrow_phase_pipeline`): a per-pair jointed-neighbor bitset, uploaded once per env (topology is fixed), so the narrow phase skips contacts between joint-connected bodies — mirroring the CPU `add_scene_contacts_to_solver` filter. **Gate:** one humanoid on the GPU solver reproduces the CPU stand/walk behavior (compare `state_hash` trends / gait metrics).
- **Stage 3b — batch N envs on GPU (spatial-offset).** Upload N humanoids (the Phase-2 grid) to the GPU solver; one batched dispatch advances all. Independent humanoids are disconnected components — the existing coloring/solve handle them. **Gate:** N≈hundreds–thousands step on GPU, env-isolated, deterministic enough; throughput ≫ CPU.
- **Stage 3c — cheap per-env GPU reset.** GPU-side: overwrite one env's `body_state` block with a stored initial pose, zero its velocities, clear that env's contact-cache / warm-start entries — a buffer write + per-env clear, no pipeline rebuild (the GPU analogue of the CPU in-place reset). **Gate:** per-env resets mid-batch with no cross-env disturbance, no stall.
- **Stage 3d — `env_id` broad-phase + on-GPU obs/action.** Replace spatial-offset with `env_id` isolation (AD-16); bring obs packing + policy inference on-device per AD-17. **Gate:** thousands of envs, training run in minutes–hours, walk learns.

### 3.3 Ordered subtasks

1. **P3.0 — confirm the gate:** Phase 2 GAE fixed + a CPU walk demonstrably learning. If not, stop — fix Phase 2 first.
2. **P3.1 — GPU jointed-pair filter** (3a): per-env jointed-neighbor bitset uploaded in `upload(...)`; narrow-phase shader skips jointed pairs. Validate one humanoid on GPU vs CPU.
3. **P3.2 — GPU batch, spatial-offset** (3b): upload N humanoids; batched dispatch; verify isolation + determinism + throughput.
4. **P3.3 — cheap per-env GPU reset** (3c).
5. **P3.4 — `env_id` broad-phase** (AD-16) + **on-GPU obs/action / compute-shader inference** (AD-17, 3d).
6. **P3.5 — scale + train:** thousands of envs, minutes–hours run, walk learns; generalize the `state_hash` regression across envs; CPU smoke still green. Do **not** build/commit without the owner.

### 3.4 Acceptance

- Articulated humanoid runs on the GPU solver (jointed-pair filter done); thousands of envs in one batched dispatch, env-isolated.
- A velocity walk trains in minutes–hours; batched run is deterministic enough for a `state_hash`-style regression.
- CPU `--locomotion-smoke` still passes (trials 2–5 bit-identical); CPU train path still works as the reference.

### 3.5 Out of scope (Phase 4/5)

AMP / mocap / animation-quality imitation (Phase 4); inference runtime in the shipped game + the FPS upper/lower-body split (Phase 5); per-body friction / domain randomization beyond Phase 2's command sampling (Component B, fold in as needed).

### 3.6 Open questions

- AD-17: accept the PCIe round-trip first, or go straight to a compute-shader forward pass? AD-18: target bit-exact GPU determinism or accept bounded non-determinism? N target (thousands — how many fit in GPU memory for N×(bodies+contacts+jacobians))? Whether to keep spatial-offset or commit to `env_id` from the start.

---

## Risks & open questions

- **VBD at batch scale** (Component A) — convergence, memory, determinism across thousands of randomized envs. The dominant unknown; Phases 1–2 exist to isolate it.
- **On-GPU obs/action round-trips** — the practical throughput ceiling for in-engine RL is PCIe traffic, not FLOPs. Keeping the loop on-device (C) is the optimization that matters; budget for it.
- **Reward shaping iteration** (Component D) — the real time-sink (human-days, not GPU-days). RSI + bootstrapped references + AMP all attack it.
- **libtorch as an engine dependency** (Component E) — wrapping it cleanly under the module rules, and the build interaction with gcc-trunk modules / BMI corruption. The Python-bridge and hand-rolled routes are the fallbacks.
- **Action space choice** — residual-PD-on-reference (stable, recommended) vs direct torque (more general, harder). Decide in Phase 0, revisit if learning stalls.
- **The trial-1 reset flake and reset-mid-fall crash** — tolerable nuisances today, blocking once every episode resets. Fix as part of Component A.3 / B.

---

## Baseline: the controller that exists today (what we build on)

Everything below is the working hand-tuned system. It is the substrate, the harness, and the reference for the learned controller — read it as *the inputs to the plan above*, not as a roadmap.

### The body

`spawn_humanoid` (Game/Game/Source/Shared/HumanoidSkeleton.cppm) builds a 17-bone rig: box pelvis/torso/head/feet, capsule thighs/shins, sphere head, box arms, sprung toe segments on metatarsal hinges. Ball joints at hips/shoulders, hinges at knees/ankles/toes/elbows. No muscles — driven entirely by joint servos. Two structural decisions: **jointed bones do not collide with each other** (the contact pipeline skips joint-connected pairs; before this, box corners carried load and every gain was calibrated against that scaffolding — all stiffness values assume the freed rig), and **arms spawn pre-hung and swing counter-phase** (free arms pumped the trunk). Per-segment masses live in `skeleton_refs` (Types.cppm) — the per-env mass randomization in Component B reads them.

### Actuation: joint drives (the action seam)

`gse::physics::joint_drive_component` is a per-axis angular servo solved implicitly inside the VBD joint solve (`accumulate_joint_drive` in vbd_solver) — ball joints drive about each body-A local axis, hinges about their axis; per-joint stiffness, target, torque cap, damping. Damping is implicit (predicted-angle delta into the Hessian); an explicit velocity term is unstable for light bodies and must not be reintroduced. Servo torque is an internal equal-and-opposite pair (net muscle torque, no external force injection). **This is the action interface for the learned policy.** Load-bearing stiffness on the freed rig: hips 600, knees 650, ankles 400 N·m/rad; stance torque caps 380/420/160 N·m with deliberately weaker swing-leg caps so swing reaction cannot pitch the trunk.

### The controller stack (the observation source + baseline + fallback)

Five systems run in sequence each tick (Game/Game/Source/Locomotion/):

1. **StateEstimator** — pelvis pose/velocity, per-foot grounded flags and support polygon (foot AABB ∪ toe AABB, so heel-lift keeps support), mass-weighted CoM, capture point (CoM + velocity × pendulum time), measured joint angles, trunk pitch + pitch rate. **This becomes the observation function.**
2. **GaitScheduler** — phase machine (idle → weight_shift → swing → plant) with capture-triggered recovery, turn-in-place, debounced fall detection; tolerates both-feet-airborne flight; swing-release posture gates are sprint-blended; sprint cadence faster than walk.
3. **FootstepPlanner** — event-based: capture point sampled once at swing entry, target committed with at most one mid-swing refinement (continuous replanning chased the legs' own oscillation into instability). Placement = sprint-blended nominal stride + capture correction + trunk-lean bias, rotated toward heading.
4. **BalanceController** — horizontal velocity-drive motor on the pelvis (the largest non-physical assist, 400 N): sprint-blended/capture-faded forward push + capture braking. Being a velocity drive it *governs* — leg-produced surplus above target rides through only inside a narrow `ride_headroom` band. **The crutch the learned controller aims to retire** (kept as a shipped-player safety net).
5. **LegController** — IK targets for stance (pose-hold) and swing (world-frame trajectory), written as joint drives. Trunk righting via ankle CoP shift + slow integral posture trim; late-stance ankle push + weight-shift toe-off for propulsion; heel-lift roll-over onto the sprung toe; sprint-gated run push-off into flight; deadzoned emergency reflexes (lateral-capture hip roll, sink-arrest knee extension) inert in nominal gait.

Movement is **camera-relative, body-relative** (W toward camera-forward, S backpedal, A/D strafe, sprint forward-only); heading always active (turn-in-place above ~26°). Sprint is a smoothed ~1 s blend, not a switch.

**What works today:** the humanoid stands, walks (~0.22 m/s), runs with genuine flight (sprint run-window ~0.40 m/s committed, 0.43 capability with bigger feet), strafes, backpedals, turns to face the camera, stops, and stands — all under physics, 5/5 on the gate. It is **robust but not nimble**: turning, acceleration, and footwork read procedural/heavy, not animated. That ceiling is the reason for the pivot.

### Verification: the locomotion smoke (the episode-loop seed)

`GoonSquad.exe --locomotion-smoke` runs headless, fixed-step, settings-persistence disabled. Five deterministic trials: settle 1.5 s → walk → 90° turn at 2 s → sprint 4–8 s → walk → release input at 13 s (heading held) → must stand to 20 s. Reports pass/fall, plants, `avg_speed` (turn-diluted — don't use for run speed), `sprint_speed` (4–8 s straight-line — the real run metric), `mean_step`, `mean_pitch`, capture maxima, `heel_lift_y`, and a bit-exact `state_hash`. **Gate on trials 2–5** (deterministic); trial 1 is a known across-process flake (must be fixed before training — see Risks). The `state_hash` is load-bearing: identical run-to-run within a binary, so a changed hash proves a knob did something and an unchanged hash exposes a no-op edit. **This is the template for the training episode loop and the policy acceptance gate.**

### Hard-won rules that still matter under learning

- `Engine/Resources/Misc/settings.ini` auto-saves on exit and silently overrides code defaults in normal runs; delete the locomotion sections after changing code defaults (smoke disables persistence).
- Tune/validate one thing at a time against the gate; rebuilds can flip marginal trajectories — re-gate after every rebuild. (Generalizes to RL: change the sim *or* the reward, never both, between eval runs.)
- Determinism is the debugging substrate — preserve the bit-exact discipline into the batched sim.

### Engine debts (now blocking, not background)

- **GPU solver lacks the jointed-pair contact filter** — the articulated humanoid runs on CPU today. Component A.2 moves the filter onto the GPU narrow phase; this is on the critical path for batched training.
- **Scene reset while a ragdoll is mid-fall crashes the physics system** (null deref; repro preserved). Tolerable as a trial-1 nuisance today; **blocking** once every episode resets — folded into Component A.3 / B (cheap per-env reset).
- **Friction is global** (`friction_coefficient` 0.6 for every contact in `physics::system`; no per-body friction). A per-body/per-env friction field is needed for domain randomization (B) and also unblocks shifting the foot-anchor hold onto real contact friction (the §3 crutch goal).

---

## The pivot (why not keep hand-tuning)

The hand-tuned stack is robust, emergent, and self-correcting, but it has a hard ceiling on **naturalness** (it reads procedural/robotic) and **responsiveness** (heavier and laggier than a player expects). The ceiling is architectural, not a tuning deficit: hand-authored fixed-gain reactive controllers on a near-fall (marginal) system flip trajectory unpredictably, so every gain is a knife-edge compromise between stability and liveliness, and the two trade off against each other. Months of careful work broke two speed walls and added strafe/backpedal/turn — real progress — but each gain came harder than the last, which is the signature of an architecture at its ceiling. A learned controller replaces *hand-specifying the gains* with *optimizing the whole control law against a reward and a reference*, on the same physics and the same actuation seam — which is why none of the substrate above is wasted. It is the difference between balancing the system by hand and letting the system learn to balance itself.
