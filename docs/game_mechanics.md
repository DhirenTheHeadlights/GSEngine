# Adding a Game Mechanic

A mechanic is a component, a system that declares what it touches, and (when the server must know about it) a message. This document is the recipe, with the sidearm as the worked example. The netcode is deterministic rollback over the solver's snapshot ring; see `rollback_request` in `Physics/System.cppm` and the `player_sync` log lines for the replay contract.

## The pieces

**A system** is a function whose parameters declare everything it reads and writes. The scheduler derives the order from those declarations, so nothing else needs wiring.

```cpp
[[= gse::system_run<>{}]]
auto run(
	gse::context& ctx,
	data& d,
	gse::shared_view<gse::actions::data> as,
	gse::shared_view<gse::world_system::data> world_d,
	gse::read<character_controller::component> characters,
	gse::write<component> sidearms,
	gse::structural<gse::physics::transform_component> transforms,
	gse::channel_read<gse::network::received<fire_request>> fire_in,
	gse::channel_write<gse::network::send_request<fire_request>> fire_out
) -> gse::async::task<>;
```

- `read<T>` / `write<T>`: component access. `write` is a conflict edge, so only take it when you mutate.
- `structural<T>`: the right to add or remove `T`. Required for `scene->build(...).with<T>(...)` on that type.
- `shared_view<data>`: a read of the fields another system marked `[[= shared]]`.
- `channel_read<T>` / `channel_write<T>`: messages, delivered the next frame.
- `[[= gse::runs_after<^^other::data>{}]]` pins an order when the data alone leaves a cycle.

**Input** is a `bindings` struct on the system's data, tagged `[[= gse::actions::set{}]]`. Held, pressed and released state for every bound action travels to the server inside the input frame automatically, so a bound action is already a server-side input.

```cpp
struct bindings {
	[[= gse::actions::mouse_bind<"Fire", gse::mouse_button::button_1>{}]]
	gse::actions::handle fire;
};
```

Read it with `gse::actions::pressed(d.binds.fire, state, as)` or `held(...)`. On the client `state` is `gse::actions::current_state(as)`; on the server it is the character's `player_input.state`.

**Physics** is driven by requests, never by touching the solver: `impulse_request`, `rollback_request`, motor targets on `motor_component`, and `gse::physics::query_transform(phys_s, id)` for a body's current transform.

**Entities** come from `scene->build(name).with<T>({...}).identify()`. The id is derived from the name, which is what makes client prediction work (below).

## Where each role runs

`Sandbox/Sandbox/Source/GameSystems.cpp` holds three lists: `shared_systems` run on both client and server, `client_systems` on clients only, `server_systems` on the server only. Adding a mechanic is one line in the right list. Inside a shared system, branch with `is_server(world_d)`, `is_client(world_d)` or `is_offline(world_d)` from `sandbox:role` instead of re-deriving it.

The server has no window, camera focus, or local actions state. A shared system must therefore get its inputs from `player_input` when it is acting for a remote character, and from the local actions state only for a possessed one. `character_controller::run` shows the pattern.

Registration order inside a list does not decide run order, and must not be made to. `character_controller` and `sidearm` read `orbit_camera::component` while writing `physics::transform_component`, which the orbit camera reads back, so the two orderings form a cycle that the scheduler has to break. Both carry `[[= gse::runs_after_optional<^^orbit_camera::data>{}]]`, which pins the camera in front of the systems that steer and aim with this frame's yaw. It is `_optional` because both are shared systems and `orbit_camera` is not registered on the server; `promote_optional_deps` drops a dep whose state is absent. Without the annotation the scheduler falls back to registration order, and moving a line between the lists silently costs a frame of aim latency with no compile error.

## Networking

Component types that replicate are listed in `NetSetup.cppm`. The engine's pack covers transforms, motion, collision, motors, kinematic targets, render and primitive specs, skeletons, clip players and controllers; game types go in `sandbox_networked_components`, and only fields marked `[[= networked]]` cross the wire.

Messages are plain structs tagged `[[= gse::network::network_message{}]]`, added to `network_messages` in the same file. A system sends with `send_request<T>{ .message, .to, .reliable }`; no address means broadcast on the server and "to the server" on a client. It receives `received<T>` with `.from` and `.controller`, the sender's `player_controller` id, which maps to their character through `controllers.find(controller)->controlled_entity_id`.

Anything the server may deliver twice (reliable resends do happen) must be idempotent on the receiver.

## Your character versus everyone else's

The two are not simulated the same way, and conflating them is the bug that keeps coming back.

**Your own character is predicted.** It runs the real simulation locally, keeps a per-step snapshot ring, and on a mismatched ack restores that step and re-steps with corrected inputs. `player_sync` does this only for the proxy named by `world_system::local_controlled_entity`.

**Every other character is interpolated, not predicted.** `player_sync` buffers each remote's `player_state` samples by `server_step` and replays them on a delay of `interpolation_delay_steps`, lerping position and slerping orientation between the two samples straddling the playback clock. The playback clock advances in real time and eases toward `newest_server_step - delay`, so packet jitter does not reach the screen; it hard-resyncs only when it drifts more than `playback_resync_steps`.

To make that stick, the first time a remote proxy is seen `player_sync` gives it a `kinematic_target_component` and flips its `motion_component::body` to `kinematic_body` (the previous value is kept in the track so possession changes can restore it). `physics::apply_kinematic_targets` then derives `current_velocity` from the target delta, which is what lets `render_transform` smooth the proxy between physics steps.

Do not dead-reckon a remote with its motor drive and then correct it against the local ring. That was the original design and it corrected on every single state: the drive arrives `ack lag` steps late, so the local guess sits ~0.75 m behind at run speed, which never fits inside the 1 cm deadband. The symptom in the log is `remote rollbacks` equal to the number of states received.

The tradeoff is deliberate: a remote proxy is kinematic, so it collides and blocks but cannot be pushed locally. The server still simulates it dynamically and arbitrates.

## Predicting a server-spawned entity

Name the entity from facts both sides know, using `predicted_entity_name(kind, owner, sequence)` from `sandbox:runtime_spawns`. The client spawns its predicted copy immediately, the server spawns the authoritative one with the same name when the message arrives, and replication lands on the client's existing entity instead of creating a second one. The sidearm's rounds do exactly this with the shot index as the sequence.

Never build a string from an id with `tag()` in shared or netcode code; replicated ids have no local name and the lookup asserts. Use `number()`.

## Diagnostics

Every system that matters prints one summary line every two seconds. Follow that convention: it is how every netcode bug so far was found. The lines to read first when something is off: `player_sync:` (local rollbacks, remote tracks and starved frames, snaps, ack lag, proxy identity), `server:` (steps per window, input queue depth), `client net:` (messages and upserts by type), `world:` (possession gains and losses).

## Testing

- Hash gates: annotate a scenario with `gse::scenario::info` and compare `world-state hash` between a reference and a variant run. Headless, hermetic, fast.
- Network conditions: `--engine-net-simulated-latency-ms N --engine-net-simulated-loss-permille M` on either process.
- Scripted play: a scenario with `.real_time = true` and no scene keeps the wall clock, connects with `--engine-net-connect`, and can drive input through `gse::input::synthetic_input_request`. `net_walk_cpu` is the template.

## Checklist for a new mechanic

1. Component and system in `Sandbox/Sandbox/Source/Shared/`, with its bindings if it takes input.
2. Register it in `GameSystems.cpp` under shared, client or server.
3. Add the partition to `Sandbox/Sandbox/Import/Sandbox.cppm`.
4. If it spawns entities the server owns, spawn them with `predicted_entity_name` on both sides.
5. If the server must learn something that is not an action state, add a message to `network_messages`.
6. If a game component must replicate, add it to `sandbox_networked_components` and mark its fields `[[= networked]]`.
7. Give it a two-second summary line.
