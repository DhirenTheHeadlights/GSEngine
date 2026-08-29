# Skinning Phase 4 — Polish and Payoff

Phase 3 exit is met: the character renders skinned, driven end-to-end through physics, with
per-instance culling bounds derived from the bone bodies.

Five workstreams, independent of each other. Recommended order is 4.1 → 4.2 → 4.3, with 4.4 and
4.5 taken only if something demands them.

---

## 4.1 Ragdoll flip with the mesh attached — verification

The demonstration the whole design exists for. Already proven before the mesh existed: the
`ragdoll_request` channel flips the proxy off and the bones to `dynamic_body`.

Nothing in the render path should need to change. `skin::collect` reads bone
`transform_component`s without caring who writes them, so a dynamic bone feeds the palette
exactly like a kinematic one.

Verify:

- bones flip to dynamic, mesh follows the collapse with no render-path change
- the per-instance AABB tracks the ragdoll as limbs spread rather than clamping to the standing pose
- the mesh does not snap or pop on the frame of the flip

Risk is in the proxy/bone handoff, not rendering. Cheap, and it is the point — do it first.

---

## 4.2 Motion vectors

**The bug.** Motion vectors are written by the depth prepass fragment shader
(`meshlet_depth_only.slang`), which computes the previous position as
`inst.prev_model_matrix * v.position`. For a skinned batch `prev_model_matrix` is identity and
`v.position` is already the *current* world-space deformed position, so `prev == curr` and every
skinned pixel emits exactly zero motion. TAA therefore treats a running character as static —
ghosting and smearing under movement.

A matrix cannot express this: skinning moves each vertex independently, so the previous position
has to come from the previous frame's deformed buffer.

**Shape of the fix.**

- `deformed_target` holds two buffers instead of one, alternating by frame parity. The deform pass
  writes the current one; the previous survives untouched.
- `deformed_slot_for` also yields the previous slot; `normal_instance_batch` gains
  `prev_deformed_vertices`.
- Depth prepass gains a `prev_vertices_buffer` binding and a push-constant flag. When the flag is
  set the mesh shader reads `prev_vertices_buffer[global_vertex_idx].position` directly instead of
  multiplying by `prev_model_matrix`; otherwise it keeps today's behaviour verbatim.

Meshlet topology is fixed under deformation, so `global_vertex_idx` addresses both buffers
consistently — no remapping needed.

**Cost.** Doubles deformed-vertex VRAM: ~28k verts x 32 B x 2 = ~1.8 MB per character. Irrelevant.

**Gotcha.** On the first frame after spawn the previous buffer is uninitialised, which would emit
garbage motion vectors and a one-frame TAA smear. Seed it — clear to the current deformed result
when the target is first allocated.

---

## 4.3 RT shadows for skinned meshes

Skinned meshes currently cast no ray-traced shadow: the BLAS loop skips them, and the TLAS loop
excludes them for free because their `entry.model` is invalid.

To enable:

- build a BLAS per skinned mesh from `deformed_target.vertices` plus the mesh's existing index
  buffer, rebuilt each frame. `gpu::build_blas_in_place` already exists and is already recorded
  into a pass, so no new RHI surface is required.
- register a TLAS instance with an identity transform — the deformed vertices are world-space.
- order the build `.after<^^skin::deform_pass>`. The RT shadow pass currently sits
  `.after<^^geometry_collector::frame, ^^physics_transform::frame>`.

**Rebuild vs refit.** `build_acceleration_structure_mode::update` and the `allow_update` build flag
exist in the RHI, but only the *TLAS* path plumbs them (`directx::build_tlas`). A BLAS refit would
need `allow_update` threaded through `create_blas` / `build_blas_in_place` plus a mode parameter —
a real RHI change. A full rebuild per frame needs none of that and is what many engines do for a
handful of characters.

Rebuild first. Measure. Only add the refit path if it actually shows up.

---

## 4.4 Meshlet culling for skinned batches — currently disabled

The depth prepass amplification shader frustum- and cone-culls each meshlet using bind-pose
`meshlet_bounds`, which go stale the moment the mesh deforms. Phase 3 disabled it for skinned
batches via the `skip_meshlet_cull` push constant.

Options:

- **Leave it off.** ~28k verts at ~32 verts/meshlet is ~900 meshlets per character. The
  amplification shader still runs, it just never rejects. Fine for a small cast.
- **Conservative bounds.** Expand each bind-pose bound by the maximum displacement of the bones
  influencing it. Needs a meshlet→bone mapping built at bake time.
- **Recompute bounds** in a compute pass over meshlets after deform. Exact, but another dispatch.

Leave it off unless profiling says otherwise. Batch-level culling already works off the bone AABBs,
so a character fully offscreen costs nothing regardless.

---

## 4.5 Transparent skinned meshes

`collect_skinned` hardcodes `.color = vec4f(1.f)`, so skinned entries always land in the opaque
queue and never reach OIT. The OIT binding site already honours `deformed_vertices`, so the only
change is deriving tint and opacity from the skinned material the way `collect_static` does.

Only worth doing if a skinned model actually needs transparency.

---

## Known limits carried forward

- `skeleton_instance_component::max_bones` is 32. `skin::collect` treats `slot >= bone_count` as
  unbound, so a denser rig silently loses its tail rather than failing loudly.
- `character.v3.gsmdl` is still in the pre-v2 bare-material-string format and cannot be baked by the
  current `bake()`. Not loaded — the sandbox uses `x_bot.v3` — so it is inert until regenerated with
  `rig_derive.py --name character`.
