# Animation Blend Space — Scope

Replace single-clip playback with weighted multi-clip playback driven from a shared
normalised phase, so diagonal movement is a genuine mix of forward and strafe rather
than a choice between them.

Follows on from `plans/physics-driven-skinning.md`, which delivered the single-clip
player this generalises.

---

## 1. What exists today

`clip_player_component` holds one clip:

```cpp
resource::handle<clip_asset> clip;
time elapsed;
float speed = 1.f;
bool playing = true;
bool apply_root_motion = false;

std::array<std::uint16_t, max_bones> track_by_slot{};   // bind cache
velocity clip_ground_speed;
id bound_clip;
id bound_model;
```

`animation::clip_player::run` resolves each bone's track once, samples one pose per bone,
runs FK, writes kinematic targets. `sandbox::character_controller::select_clip` picks a
single clip by dominant input axis and sets `speed` from
`ground_speed / clip_ground_speed`.

The dominant-axis choice is the thing being replaced: it hard-snaps at 45 degrees and
plays a pure forward or pure strafe cycle while the character actually travels diagonally.

---

## 2. The split

**Engine owns weighted playback.** Sampling N clips, blending poses, keeping them in
stride, and deriving the rate correction are all generic and none of it is game-specific.

**The game owns the weights.** Which clip sits at which coordinate is content. Trilinear
weighting is a small pure function, so it ships as a free function plus a POD layout type
rather than a second system — reusable without inventing a parallel abstraction before
there is a second consumer.

The flow: the character controller says "I am moving at (strafe, forward) at this speed",
a helper turns that into weights, and the player consumes them. Producer/consumer, the
same shape as the rest of the engine.

---

## 3. Phase, not elapsed

The load-bearing idea. Blending a walk at heel-strike against a strafe at mid-stride
averages into a floaty mess with no clear foot contacts — this is the part that is usually
underestimated.

Every layer is sampled at `phase * clip.length()` with `phase` in [0, 1) shared across
layers. Because each Mixamo locomotion clip is exactly one stride, normalising by its own
length *is* the stride alignment. Clips of unequal length align for free.

Advance:

```
period    = sum(w_i * length_i)          blended stride period
authored  = sum(w_i * ground_speed_i)    blended authored ground speed
rate      = authored > 0 ? desired_speed / authored : 1
phase    += dt * rate / period
```

`rate` is the existing foot-sliding correction generalised: one extra weighted sum and it
keeps working. `authored == 0` is the idle case, which has no root motion to measure, so
it falls back to authored playback rather than dividing by zero.

---

## 4. Pose blending

Per bone, over the layers that actually have a track for it:

- translation: `sum(w_i * t_i)`
- rotation: weighted quaternion average — take the highest-weight layer as reference,
  negate any `q_i` with `dot(ref, q_i) < 0`, accumulate `w_i * q_i`, normalise

**Renormalise per bone.** A clip may lack a track for a given bone. If a layer without
that track still contributed its weight, the result would be dragged toward bind pose in
proportion to that weight — a subtle, direction-dependent droop that is hard to attribute
later. Accumulate only layers that have the track and divide by their summed weight; if
none do, fall back to the bind pose as today.

Root-motion stripping applies to the blended root pose, unchanged.

---

## 5. Move the bind cache out of the component

Today `track_by_slot` and `clip_ground_speed` live on `clip_player_component`, keyed by
`bound_clip` / `bound_model`. With N layers that becomes
`std::array<std::array<std::uint16_t, 32>, N>` — 256 bytes of cache per character at N=4,
plus per-layer bound ids, duplicated across every character using the same model and clip.

The binding is a property of the **(model, clip) pair**, not of the entity. Move it into
`animation::data` as a `flat_map` keyed on that pair, holding the resolved track indices
and authored ground speed. The component keeps only what is genuinely per-entity: layers,
phase, and flags.

This shrinks the component, removes duplication across characters, and scales to N layers
without growing anything per-entity.

---

## 6. Shape

The space is **3D**: two movement axes plus a speed axis (walk to run), resolved as
trilinear weights over the surrounding samples. Sprint stops being a clip swap and becomes
a coordinate, so walk to run blends rather than cutting.

**Engine**

```cpp
struct clip_layer {
    resource::handle<clip_asset> clip;
    float weight = 0.f;
};

struct clip_player_component {
    static constexpr std::size_t max_layers = 4;

    std::array<clip_layer, max_layers> layers{};
    std::uint32_t layer_count = 0;
    float phase = 0.f;
    velocity desired_speed;
    bool playing = true;
    bool apply_root_motion = false;
};

struct blend_sample {
    resource::handle<clip_asset> clip;
    vec3f position;
};

auto blend_weights(
    std::span<const blend_sample> samples,
    vec3f input,
    std::span<clip_layer> out
) -> std::uint32_t;
```

`position` is (strafe, forward, speed). With a 4-way layout per speed tier only the four
in-plane samples are populated, so at most four layers are ever active: two movement
corners times two speed tiers. `max_layers = 4` holds until the 8-way layout lands, at
which point diagonal input spans four corners times two tiers and it becomes 8.

Single-clip playback is the degenerate `layer_count == 1, weight == 1` case, so nothing
needs a separate path.

**Game** — `character_controller` keeps a `blend_sample` layout instead of
`locomotion_clips`, and each frame writes input into `blend_weights` and the result into
the player's layers.

---

## 7. Work breakdown

### Phase 1 — Weighted playback, single layer
Introduce layers and phase; drive everything through the blend path with one layer at
weight 1. Move the bind cache into `animation::data`.

*Exit:* behaviour is identical to today. This is deliberate — it isolates the
representation change from any visible change, so a regression here is unambiguous.

### Phase 2 — Multi-layer blending
Pose blending with per-bone renormalisation, blended period and authored speed, rate
correction.

*Exit:* two clips at 50/50 produce a clean intermediate gait that stays in stride.

### Phase 3 — Blend space in the controller
`blend_weights` plus the 4-way by 2-tier layout, replacing `select_clip`.

*Exit:* diagonal movement blends smoothly with no snap at 45 degrees, and walk to sprint
blends rather than cutting.

---

## 8. Sharp edges

- **Quaternion averaging needs the sign fix.** Without hemisphere alignment against the
  reference, two nearly-equal rotations can average toward zero and produce a collapsed
  or flipped joint.
- **Weights must sum to 1** before they reach the player, or the pose scales toward the
  origin. Renormalise in `blend_weights` rather than trusting callers.
- **Idle has no root motion**, so it must not participate in the authored-speed average as
  a zero — it would drag `rate` upward and speed the whole blend up. Exclude zero-speed
  layers from that sum.
- **Phase wrapping stays bidirectional** anywhere a layer can play in reverse. With a real
  backward clip in hand this should no longer be needed for locomotion, but the guard is
  cheap and the failure (phase running away negative) is silent.

---

## 9. Settled

1. **4-way now, 8-way later.** The layout is data, so widening to diagonals is adding
   samples plus raising `max_layers` — no structural change.
2. **Speed is a third axis**, not a separate space. Sprint becomes a coordinate rather
   than a clip swap.
3. **Turns are in scope, but not as blend samples.** See section 10.

Clip inventory against the 4-way by 2-tier layout:

| Coordinate | Walk tier | Run tier |
| --- | --- | --- |
| forward | `walking` 1.03 s, 1.67 m/s | `running` 0.70 s |
| backward | `walking_backwards` 1.20 s, 1.13 m/s | *missing* |
| strafe left | `left_strafe_walking` 1.03 s | `left_strafe` 0.67 s |
| strafe right | `right_strafe_walking` 1.03 s | `right_strafe` 0.63 s |
| idle | `idle` 8.30 s | — |

`walking_backwards` is 1.20 s / 1.13 m/s, outside the matched 1.03 s walk family, so it is
the first clip that genuinely needs per-layer rate correction rather than getting stride
alignment for free. Good early test of that path.

One gap: **no backward run**. Until it exists the backward edge of the run tier should
reuse `walking_backwards` with rate correction rather than reversing `running` — a
reversed run reads worse than an over-driven walk.

---

## 10. Turns are a transition layer, not a blend space

`left_turn`, `right_turn`, `left_turn_90`, `right_turn_90` are not locomotion — they have
no movement direction to occupy a coordinate. They are **transitions**: discrete, played
once, and they own the character's facing for their duration.

Mixing them into the blend space would be a category error. A blend space has no concept
of "play once and stop", so a turn sample would sit permanently partially active.

Shape:

- The controller detects a yaw delta beyond a threshold while stationary or slow.
- A transition takes over: the chosen turn clip plays once at `weight = 1`, and **facing
  follows the clip** rather than the camera for its duration.
- On completion, control returns to the blend space.

`apply_root_motion` already carries the necessary distinction, but not finely enough. A
turn's authored *rotation* is the entire point, while its translation should still be
stripped — and the flag currently gates translation only. Turns will want it split into
separate rotation and translation controls.

This is a small state machine (locomotion and transition), the natural feature after
blending, and should be scoped separately rather than folded in here.
