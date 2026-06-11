# Locomotion

Physically simulated humanoid walking. No animation playback, no kinematic shortcuts: every motion is the result of joint servo torques, ground contact, and a balance controller deciding where to step. This document describes what works today, how it is verified, and what comes next.

## What works today

The humanoid stands up, walks, sprints, turns toward the camera, stops, and stands still — all under physics, verified by a deterministic headless test. Current measured behavior (per smoke trial): average ground speed 0.18–0.21 m/s walking with a ~2.5× sprint window, mean step length 0.26–0.29 m, mean trunk pitch within ~5° of upright, and the pelvis never collapsing across walk/sprint/turn/stop transitions.

### The body

`spawn_humanoid` (Game/Game/Source/Shared/HumanoidSkeleton.cppm) builds a 17-bone rig: box pelvis/torso/head/feet, capsule thighs and shins, sphere head, box arms, and sprung toe segments on metatarsal hinges. Ball joints at the hips and shoulders, hinges at knees, ankles, toes, and elbows. There are no muscles — the rig is driven entirely by joint servos.

Two structural decisions matter most:

- **Jointed bones do not collide with each other.** The physics contact pipeline skips entity pairs connected by a joint (`add_scene_contacts_to_solver`). Before this, box corners ground against each other at every articulation and — worse — carried load: the trunk and knees were propped by corner contact, and every gain in the controller was unknowingly calibrated against that scaffolding. All current stiffness values assume the freed rig.
- **Arms spawn pre-hung and swing counter-phase.** Ball-joint shoulders are held by drives (hang pose is the spawn rest), and the leg controller swings each arm opposite its leg. Free-flopping arms measurably pumped the trunk.

### Actuation: joint drives

`gse::physics::joint_drive_component` is a per-axis angular servo solved implicitly inside the VBD joint solve — ball joints drive about each body-A local axis, hinges about their axis. Stiffness, target, torque cap, and damping per joint. Damping is implicit (predicted-angle delta, contributing to the Hessian); an explicit velocity term is unstable for light bodies and must not be reintroduced. Servo torque is an internal equal-and-opposite pair — the physical equivalent of net muscle torque, with no external force injection.

Load-bearing stiffness on the freed rig: hips 600, knees 650, ankles 400 N·m/rad, with stance torque caps 380/420/160 N·m and deliberately weaker swing-leg caps so swing reaction cannot pitch the trunk.

### The controller stack

Five systems run in sequence each tick (Game/Game/Source/Locomotion/):

1. **StateEstimator** — pelvis pose/velocity, per-foot grounded flags and support polygon (foot AABB unioned with the toe AABB, so heel-lift keeps support), mass-weighted center of mass, capture point (CoM + velocity × pendulum time), measured joint angles, trunk pitch and pitch rate.
2. **GaitScheduler** — the phase machine: idle → weight_shift → swing → plant, with capture-triggered recovery steps, turn-in-place triggers on heading error, and debounced fall detection (a single-substep contact spike is not a fall; pelvis-height collapse is).
3. **FootstepPlanner** — event-based stepping: the capture point is sampled once at swing entry and the target is committed, with at most one mid-swing refinement if capture deviates beyond a threshold. Continuous replanning chased the swinging legs' own mass oscillation into instability; commitment is what makes true-CoM capture usable. Step placement = nominal stride (sprint-blended) + capture correction + trunk-lean bias, rotated toward the desired heading (turn-by-stepping, clamped per step).
4. **BalanceController** — a horizontal velocity-drive motor on the pelvis: forward push (sprint-blended, capture-faded, launch-ramped so a standing body is never shoved at full force) plus capture braking. This is the largest remaining non-physical assist (400 N cap).
5. **LegController** — IK targets for stance (pose-hold) and swing (world-frame trajectory toward the committed target), written as joint drives. Trunk righting lives here: ankle center-of-pressure shift proportional to pitch and pitch rate, plus an adaptive posture trim (slow integral, gated by walk intensity — it is gait state, not posture state) split between ankle and a stance-hip bias. Late-stance ankle push and weight-shift toe-off provide ground propulsion; both fade as forward capture grows (speed regulation). Stance-hip yaw drives steer the pelvis toward the heading over the planted foot.

Heading is always active: camera facing when idle (turn-in-place above ~26° error), camera-relative input direction when moving. Sprint is a smoothed blend (~1 s), not a switch — the binary transition slammed the brakes at sprint release and toppled the body.

### Verification: the locomotion smoke

`GoonSquad.exe --locomotion-smoke` runs headless, fixed-step, with settings persistence disabled (code defaults only — see Gotchas). Five deterministic trials, each: settle 1.5 s → walk → 90° turn at 2 s (mid-launch) → sprint 4–8 s → walk → release input at 13 s with heading still held → must stand to 20 s. Each trial line reports pass/fall, plants, `avg_speed`, `mean_step`, `mean_pitch`, capture maxima, and a state hash.

The state hash is load-bearing for development: trials are bit-identical run-to-run within a binary, so a hash change proves a knob did something and an unchanged hash exposes a no-op edit. Failures found in manual play get encoded as gate phases (the launch-turn, the stop-with-heading, and the sprint window all came from real falls).

### Hard-won rules

- Tune one knob family at a time against the gate; batched packages failed repeatedly from coupling.
- Rebuilds can flip marginal trajectories even with identical source — re-gate after every rebuild.
- `Engine/Resources/Misc/settings.ini` is auto-saved on exit and silently overrides code defaults in normal runs. When code-default tuning changes, delete the locomotion sections from the ini (they regenerate); otherwise manual play runs stale values.
- Instantaneous-proportional pitch feedback through the stance hip is a fold-amplifier; slow integral bias is safe. Righting belongs at the ankle (CoP), posture correction in the trim.

## Known limits and next steps

In rough value order:

1. **Turning sharply at speed** is beyond the current controller — a 90° (even 45°) heading snap during sprint cannot be stabilized by capture-stepping in any parameterization tried; lateral and forward overflow just trade places. It needs real mechanisms: body banking into the turn and crossover stepping. The first ingredient is in: swing-hip roll (`swing_roll_gain` 0.3) is enabled and was the change that stabilized the launch-turn margin — it was destabilizing on the corner-propped rig and only became viable on the freed one. Banking builds on it.
2. **Walking speed** (~0.13–0.2 m/s vs human ~1.2) is bounded by the capture budget and the step-conversion chain — and the swing-roll stability above traded some speed for margin. The toes exist as a longer push lever but the controller does not yet exploit roll-over (heel-lift onto the toe segment in late stance); deeper ankle push is a measured no-op until it does. After roll-over: revisit stride and cadence with the `mean_step` metric attributing each change.
3. **Crutch removal.** The pelvis balance motor (400 N) and the foot anchor motors (700/150 N) are still load-bearing; each architectural improvement so far has bought a tranche of reduction. The end state is propulsion and balance entirely through joints and contact.
4. **A true run gait** — flight phases, landing absorption — is a different machine from fast walking and should be built as its own gait mode once turning-at-speed is solved.
5. **Engine debts:** the GPU solver path lacks the jointed-pair contact filter (CPU-only today), and scene reset while a ragdoll is mid-fall crashes the physics system (null deref; repro preserved, task filed).

## Field guide

- The smoke logs to stdout and `Engine/Resources/Misc/log.txt`. Manual-play falls leave a full trace; `ENTER FALLEN` lines plus the surrounding estimator/gait context have root-caused every fall so far.
- Controls: WASD camera-relative movement, mouse heading, shift sprint.
- All tuning lives in the settings-annotated `data` structs of the five locomotion systems and is editable live in the settings UI (subject to the ini gotcha above).
