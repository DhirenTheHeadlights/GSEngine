# Deterministic rollback for multiplayer

Written 2026-09-03. Direction agreed in chat: server-authoritative multiplayer where every
machine runs the real simulation, the network carries inputs plus acks plus periodic hashes,
and a client corrects by restoring a local snapshot and re-stepping with corrected inputs.
The GPU solver is the target; the CPU solver is the reference and the first proving ground.

## What research settled

- The VBD solver steps the whole world only. There is no per-body step and nothing in the
  engine re-steps a subset. Exact replay therefore means whole-world restore plus re-step.
- The state carried across steps is bounded and enumerated (inventory in the chat log of
  2026-09-03): per body transform and motion, `kinematic_step_start`, `sleep_counters`, joint
  lambdas, penalties and rest-orientation latches, the contact cache including its stamp and
  `last_seen` history, the solver's previous velocity and accel weight, `body_sleeping`.
  `body_airborne`, `id_to_body_index`, the constraint graph, colouring, islands and frozen
  Jacobians are rebuilt every substep.
- Body index is the dense motion-storage slot and removal is swap-with-last, so every
  index-keyed carried array must be re-keyed by id in a snapshot.
- Same-machine runs are already deterministic (scenario hashes). Cross-machine bit agreement
  is not a requirement for a server-authoritative shooter and is not a goal. The gates are
  restore-and-replay hash equality on one machine, and a measured server-versus-client drift
  budget.
- The GPU path keeps the authoritative world on the device and reads it back at least
  `max_frames_in_flight` (2) frames stale, never waiting. Rollback on the GPU is device-side
  copies plus a replay batch, with no new wait. Age costs two extra replay steps per
  correction and delays hit validation by up to two frames. Both acceptable.
- No fast-math or arch flags. Server and clients are one executable.

## Phases

### Phase 1: CPU snapshot ring, restore, replay, parity gate

Files: `Engine/Engine/Source/Physics/System.cppm`, `System.cpp`,
`Engine/Engine/Source/Physics/VBD/Solver.cppm`, `Sandbox/Sandbox/Source/Sandbox/Scenarios.cppm`,
`Scenarios.cpp`.

- `physics::step_snapshot`: step number, `body_snapshot { owner, transform, motion }` per
  motion component, `carried_body_state { owner, previous_velocity, accel_weight }` per body,
  copies of `sleep_counters`, `kinematic_step_start`, `joints`, `contact_cache`.
- `physics::data` gains `rollback_history_steps` (setting, default 0 = off), `step_index`,
  and the ring `std::vector<step_snapshot>` indexed by `step % history`.
- `vbd::solver` exposes its carried arrays (`previous_velocities()`, `accel_weights()`) and a
  `seed_carried(velocities, weights)` setter next to `seed_previous_velocities`.
- `update_vbd` records a snapshot at the end of every fixed step, including the all-asleep
  early return.
- `rollback_request { steps }` on the physics channel. `integrate` restores the ring entry
  `step_index - steps`, re-steps `steps`, then runs the frame's own steps. Restore writes
  components back by id and skips entities that no longer exist. On the GPU path the
  request logs once and is ignored until phase 3.
- Scenarios `rollback_reference` and `rollback_replay`: Pyramid scene, headless, CPU solver
  pinned, 120 measured frames, identical settings. The replay variant pushes a 60-step
  rollback at frame 70. The two world-state hashes must be equal. This is the gate every
  later phase re-runs.

Status 2026-09-03: built and passing. Pyramid pair hashed `0x4e2f3018e9d2d205` on both
runs; the stress pair (`rollback_reference_stress` / `rollback_replay_stress`, tumblers,
joints and impulses, 240 frames, 60-step rollback at frame 150) hashed
`0xce771b2aff6fac02` on both. The replay profile shows `restore_step` called once.
Phase 2's physics side landed in the same pass: per-step inputs (motors, kinematic start
transforms plus velocities, impulses) recorded in each ring entry and replayed from it,
the all-asleep skip evaluated per step, and `kinematic_step_start` dropped from the
snapshot because `prepare` owns it per frame and the recorded start transform reproduces
its rewind exactly.

### Phase 2: the step is the unit

- Inputs that affect physics are recorded per step in the ring entry: motor targets,
  kinematic targets, impulses, structural adds and removes.
- Replay applies the recorded inputs for step j before stepping j. The frame path uses the
  same function, so the two paths cannot drift.
- The character controller becomes a per-step function of an input record (`player_input`
  on the character entity: actions state, camera yaw, sequence). The one-frame pipeline
  between controller and physics goes away.
- Kinematic drivers (tumblers, pistons, animation bones) advance by consumed step, never by
  frame; the standing rule from the GPU async work already says so.
- Gate: phase 1 scenarios plus a moving-character scenario with scripted input.

### Phase 3: GPU ring

After the shader-binding-order removal lands in `GpuSolver.cpp` and `SharedShaders.cppm`.

- Ring slots inside `gpu_solver`, sized by `rollback_history_steps` and a contact budget:
  copies of `body_buffer`, the contact and warm-start buffers with their counts, offsets and
  adjacency, and `joint_buffer`, taken at the end of every tick from the just-dispatched slot.
  Copy the live ranges, not the limits.
- Restore copies a ring slot into both live slots so `p.other` warm starts are right, and
  restores the host mirrors (`sleep_counters`, `kinematic_step_start`, joints, adaptive
  colour-cap and sweep-fold state).
- Replay is one batch of `n` ticks with per-tick motor and impulse arrays.
- Gate: `rollback_replay_gpu` on Vulkan, hash-equal to `rollback_reference_gpu`. DX12 reports
  only, per the parity harness contract.

Status 2026-09-03 evening: phase 3a is built and its gate PASSES on Vulkan.
`rollback_reference_gpu` and `rollback_replay_gpu` both hash `0x0857479aa2c46f4a`; the replay
run's log shows `gpu rollback: restoring tick 52 and replaying 20 tick(s)` from physics and
`vbd gpu rollback: restoring tick 52 into the next batch of 21 tick(s)` from the solver. Trust
those two log lines, not the profile's GPU per-pass table: that table undercounts burst and
one-shot passes (the 21-tick batch showed as ~4 extra ring copies and no restore row).
Scaffolding details: `gpu_solver` gained a
device ring (`ring_slot` per tick: bodies, joints, contacts, contact counts/offsets/adjacency;
`ring_body_capacity` 4096, `ring_contact_capacity` 16384, at most `ring_max_history` 64
slots) allocated once by `ensure_ring` from `physics::frame`. The ring stages live in a
separate implementation partition, `GpuSolverRing.cpp`, so the binding-order refactor and
this work do not edit the same file; `dispatch_compute` only gained the two call sites: a
restore stage before `stage_apply_body_inputs` when the upload carries `restore_tick`, and a
ring copy after `stage_update_sticking` at every tick boundary (`first_tick + tick`). On the
physics side `update_vbd_gpu` handles `rollback_request` by folding the replay into the same
batch (`ticks = replay + this frame`, `first_tick = target`) and `step_index` now advances on
the GPU path. Gate scenarios `rollback_reference_gpu` / `rollback_replay_gpu` (Pyramid,
history 30, 20-step rollback at frame 70) are declared.

Status 2026-09-03 night: phase 3b (per-tick inputs for replay batches) is built and PASSES.
The upload carries `motors_per_tick` and `impulse_counts`; motors are a flat
`ticks × motors_per_tick` array indexed by `(substep / substeps_per_tick) * motor_count` in
`vbd_predict` and `vbd_solve_color`, impulses are a flat array with per-tick offsets that
`vbd_apply_impulses` reads through `pc.impulse_offset`, and the impulse stage dispatches on
the first substep of every tick that has impulses. `substeps_per_tick` and `impulse_offset`
are the two new push constants. On the physics side `update_vbd_gpu` records each stepped
tick's `step_inputs` into `d.rollback_ring` (inputs only; the body snapshots stay empty on
the GPU path), refuses a rollback whose replay window has a missing entry, builds the per-tick
arrays from the ring for replay ticks and from this frame's inputs for the frame ticks, and
falls back to this frame's motors for every tick (with a warning) if the motor body set
changes inside the window, because the device motor map is per batch. `limits.max_impulses`
went from 64 to 4096 so a long replay window of wake impulses fits. Gates:
`rollback_impulse_reference` / `rollback_impulse_replay` (Pyramid CPU, two impulses on
`PyramidBlock_0_0` and `PyramidBlock_1_0` inside a 20-step window) both hash
`0x49ce974f117c4d0f`; `rollback_impulse_reference_gpu` / `rollback_impulse_replay_gpu`
(Vulkan, history 30) both hash `0x25508981f3f44177` with the restore log lines present. The
four earlier pairs still hash `0x4e2f3018e9d2d205`, `0xce771b2aff6fac02` and
`0x0857479aa2c46f4a`. The intermittent headless stall ("stuck inside frame() (all deps
ready)") is FIXED: `async::when_all` posts helper starts to the pool, so with a 21-tick batch
some stage coroutines pushed their `render_pass_request` after the frame's record round had
already drained empty, and the request could never record. The scheduler now has a stall
probe (`set_stall_probe`, wired by `engine`) that drains and names such late requests on every
watchdog dump; it caught `vbd_apply_restitution_stage#33` and `vbd_derive_velocities_stage#3`
on the first try. `dispatch_compute` now fans the stage batch out with
`async::when_all_inline`, which starts every helper on the calling thread before the parent
suspends. Plain `when_all` must stay posted: the scheduler fans every system task through it,
and an inline start serialized the whole update phase (tried, measured, reverted).

Bad-network results (2026-09-03 night). Tooling: `--engine-net-simulated-latency-ms` and
`--engine-net-simulated-loss-permille` delay and drop received packets in the endpoint, and
the real-time windowed scenarios `net_walk_cpu` / `net_walk_gpu` drive a connected client with
synthetic W/A/S/D. Three protocol bugs fell out first: the 32-packet ack window was outrun by
reliable replication bursts (fixed by flushing an ack packet every 16 received packets and
after 30 ms idle), a repeated `connection_accepted` deactivated the client's scene and removed
the character it had built into it (fixed by ignoring repeats and making `activate_scene`
idempotent), and replication was unreliable (now reliable). Results on the CPU solver both
ends: 50 ms one-way latency, no loss: one 1.2 cm correction replayed in 10 steps during a 20 s
walk, positions match the server to 2e-4 m. With 2 percent loss: 4 to 6 corrections per two
seconds while moving, max error 13 cm, replays 9 to 10 steps, converged to 2e-6 m at rest, ack
lag 7 to 11 steps. The GPU pairings are blocked: a headless dedicated server on the GPU
solver runs about 12 physics steps per wall second with 70 to 130 ms frames, and a GPU client
against a CPU server corrects every frame because the solvers disagree, each correction
replays a 24-tick batch that costs more than a frame, and the error runs away. `player_sync`
now snaps instead of replaying more than 24 steps, which bounds a CPU spiral but cannot save
the mixed-solver case.

Netcode ownership (2026-09-03 night): the engine host is game-agnostic. It tags
`player_input.acked_sequence` and `acked_step` and fills `received<T>::controller`; the
Sandbox owns `player_state` (broadcast from the physics ring at the acked step, one per step
per client) and `sidearm::fire_request` (reliable shot index plus aim; deterministic round
names so the client's predicted round is the entity the server replicates).

Status 2026-09-03 night, later: phase 3c (readback-aligned ring bodies) is built and the six
gate runs above still hash the same. `gpu_solver` records `first_tick + ticks` per dispatch
generation and exposes it as `readback_tick()`; the readback block in `update_vbd_gpu` fills
that tick's ring entry with `body_snapshot`s taken from the components right after the
readback is written into them, and publishes the tick as `physics::data::observed_step` (the
CPU path sets `observed_step = step_index` after every update). A GPU-path rollback applies
the request's body overrides whose `step` equals the restore tick by writing the override
transform and motion into the components and raising `reset_pending`, which the existing
`vbd_apply_body_inputs` stage turns into a full body write after the ring restore; overrides
at later steps inside the window are not applied on the GPU path (the next server state
re-checks them against the replayed ring, so nothing is lost beyond one frame). `player_sync`
now defers any server state whose local step is newer than `observed_step` to the next frame
instead of snapping, because on the GPU path the ring has no bodies for steps the readback
has not reached yet. Verified at rest 2026-09-03 night: a local dedicated server plus a client
on `Physics.use_gpu_solver=true` reports `0 snaps, 0 local rollbacks, ack lag 5-7 steps` with
the proxy at the GPU solver's resting height; before 3c every state would have been a snap.
Movement on the GPU path is still to be exercised in a real session.

The "models sink into the floor" report that followed was not a rollback bug. The client's
proxy arrived over replication without its `collision_component`: `physics::ensure_results`
drained the collision `added` events on the server before `replicate_deltas` saw them, and
component events are one queue per storage. The collider-less proxy free-fell and every
correction pulled it back, which is a steady state ~1.9 m below the floor. `ensure_results`
now scans owners instead of draining events; a local server plus client shows the proxy at the
server's height with zero corrections once the ring covers the acked steps. A resting
character replays bit-exactly on the CPU apart from a 1e-12 orientation-w difference from the
controller's per-frame slerp (`rollback_reference_character` / `rollback_replay_character`,
compared with a spawn-aligned state-dump diff because the model loads on a wall-clock frame).
`player_sync:` now prints the local proxy's shape, body kind, mass, contact state, the server
step and the ack lag every two seconds while connected; read those before suspecting physics.

The input lag (ack lag pinned at ~36 steps, every correction replaying 35-40 steps) had two
causes, neither in the rollback code. `system_clock::snap_delta` discarded its accumulated snap
error whenever it exceeded 8 ms, and a headless dedicated server at ~9000 fps tripped that
constantly, losing about a third of wall time: its 2 s health windows took ~3.3 s and it
simulated ~36 fixed steps per wall second against the client's 60. Snapping is now
time-conserving (the trip repays the error) and is disabled outright when the engine has no
window. Independently, the server's per-client input queue held 32 inputs and applied one per
step with no catch-up, so any backlog was permanent; it now holds four and drops the oldest.
The dedicated server logs a `server:` health line every two seconds (frames, physics steps,
longest frame, clients, deepest input queue).

Design notes from reading `GpuSolver.cpp` on 2026-09-03 (written before the scaffolding):

- One dispatch is one batch: `p.substeps = ticks × physics_substeps` iterations of the
  substep stage list, then `stage_state_copy` (body + joint buffers into `p.other`) and
  `stage_publish` (readback channels). Tick boundaries are every `physics_substeps`
  iterations; the ring copy is a new stage inserted at each boundary, into ring slot
  `(tick_index % history)`.
- The GPU carried set is entirely device-side, which is simpler than the CPU: `body_buffer`
  (positions, velocities, `sleep_counter`, `accel_weight`), `joint_buffer` (lambdas,
  penalties), `contact_buffer` as the warm-start source, and `contact_counts` /
  `contact_offsets` / `contact_adjacency` which `warm_start_lookup` walks. Copy live ranges:
  body and joint counts are host-known; the contact count is device-side, so the contact
  copies are bounded by a `rollback_contact_budget` setting rather than `limits.max_contacts`.
- Restore copies a ring slot into BOTH live slots so `p.other` warm starts match, and resets
  the host-side adaptive colour-cap and sweep-fold state to the values recorded with the
  slot. `m_dispatch_slot` parity is unchanged by a restore because both slots are written.
- Replay is one batch of `n` ticks. Motor targets are per batch today (`m_upload_motors`
  plus `m_upload_motor_map`), so the replay needs per-tick motor arrays: upload `n × motors`
  and index by tick in the push constants where the predict stage samples the motor. That
  is the only shader change. Impulses already carry a body index and apply on substep 0;
  for replay they apply on the first substep of their tick.
- Readback stays as is. A replay's result is visible to the ECS `max_frames_in_flight`
  later, which is the accepted age cost; `player_sync` reads the CPU ring for its records
  today, so on the GPU path the ring entry's body snapshot must come from the readback
  (age-shifted by `readback_age_steps`) rather than from the ECS write-back.
- Memory at defaults: 4096 bodies × ~400 B plus a 32k-contact budget × ~100 B ≈ 4.8 MB per
  slot; 120 slots ≈ 580 MB, so the networked floor on the GPU path should be ~30 slots
  (half a second), not 120, unless the budget is trimmed by live counts.

### Phase 4: netcode

- Client sends one `input_frame` per fixed step, sequence = step. Server queues per client
  and applies one per step to `player_input`. Server broadcasts `player_state` per player per
  frame with the sequence whose input drove the step, plus a periodic world hash.
- Client keeps the ring, compares acked states against its records, and on mismatch restores
  the acked step and replays with the corrected remote inputs. Below a deadband nothing
  happens; beyond a cap the client resyncs from a server snapshot, which is the phase 1
  snapshot serialized through the binary archive.
- Late join uses the same snapshot payload over the reliable path.
- Remote players interpolate between acked states; their animation blend derives from acked
  velocity in the facing frame.

Status 2026-09-03: plumbing built, not yet exercised with characters (headless runs cannot
load the skinned model). Decisions taken while building it:

- The controller runs one frame ahead of the physics that consumes its motor target, on both
  sides. The scheduler pins it after physics through the orbit camera and animation reads of
  `transform_component`, and the server pins it after the host through `player_input`. So
  the client controller keeps a stash: at frame N it sends last frame's stash once per fixed
  step physics just ran, then rebuilds the stash from this frame's actions and orbit yaw. The
  server host tags each outgoing `player_state` with the sequence that was in `player_input`
  before this frame's application, which is the input that drove this frame's steps. The two
  conventions line up without either side removing the pipeline.
- `player_input` lives on the controller entity, added by the host at accept. The sandbox
  controller finds a character's input by scanning `player_controller` for the one whose
  controlled entity is that character.
- `player_state` carries `server_step`, the owner's `input_sequence`, position, velocity,
  orientation and the applied motor drive. A client learns the server-step to local-step
  offset from its own acks and uses it to place remote players' states on its own timeline.
- Remote proxies stay dynamic on the client. Their motor is driven by the acked drive, so
  between acks they dead-reckon with the solver, and their orientation and blend weights come
  from the acked state.
- Corrections go through `rollback_request::body_overrides`, each stamped with the step it
  applies at; the physics replay loop is one `update_vbd` call per step so an override can
  land between steps. Below a 1 cm deadband nothing happens. Acks older than the ring or at
  the current step are applied directly instead of replayed.
- Only the LOCAL proxy's acks trigger a replay (2026-09-03, after the first character run
  showed models sinking into the floor). Remote proxies are corrected by direct writes of
  the acked position and velocity plus a zero impulse to wake them, and dead-reckon through
  their acked motor drive between acks. Whole-world rollbacks on every remote ack were the
  largest untested surface and are not needed for the local player's feel; re-introduce
  them only behind a measured interaction case.
- An override now resets the body's sleep counter. A body asleep at the restored step would
  otherwise stay inactive at the overridden position, which is how a server position a
  little below the client's rest depth left a proxy frozen inside the floor.
- Multi-step frames: kinematic start transforms are applied on the first step of a frame
  only. Re-applying them on every step rewound kinematic bodies to the frame start each
  step, so moving platforms advanced one step per frame regardless of the step count.
- `player_sync` logs a `player_sync:` line every two seconds with local/remote correction
  counts, max errors, snaps and the longest replay. Read it in the client log to see the
  correction rate; a local rate near one per frame while moving means the input sequence
  and the ring step are misaligned.
- The engine's own input sender and the `camera_yaw_request` roundtrip were deleted. The game
  is the only input-frame sender.
- Found and fixed on the way: `mpsc_ring_buffer` published a slot (advanced head) before
  writing it, so the endpoint's socket thread sent torn or default packets and lost roughly
  150 per second to WSAEADDRNOTAVAIL once a client was connected. It predates this work
  (logs from 09-02 show it). Per-slot sequence numbers now gate readiness. The headless
  server-plus-client run is clean of sendto errors after the fix.
- The ring is enabled for networked sessions by `world_system::run` pushing
  `physics::rollback_history_request` every frame (120 steps when networked, 0 otherwise);
  physics keeps `max(setting, floor)`. A one-shot push from `init` was lost because physics
  is a deferred system. Verified 2026-09-03: a networked client's bench profile shows
  `physics::snapshot_step`. Without the ring, `player_sync` snaps the local proxy to every
  ack and movement waits on the server while the locally driven animation starts at once,
  which was the reported input lag.

## Out of scope

- Endpoint batching of many messages per datagram. Follow-up once traffic is measured.
- Detached dedicated-server pacing. Pacing today only comes from the editor link.
- Orientation reconciliation. Orientation is replicated, not corrected.

## Coordination

The shader-binding-order removal runs in parallel. Phases 1 and 2 touch no file in that
refactor's set. Phase 3 waits for it.
