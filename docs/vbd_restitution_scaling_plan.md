# VBD GPU solver: restitution stage past 65k bodies and 2k contacts

## Problem

`vbd_apply_restitution` runs as a single workgroup of `adjacency_workgroup_size` lanes. It gathers every gated contact (restitution above zero, negative normal lambda, approach faster than the threshold) into LDS, sorts by a 64-bit key, and then one lane per island applies its contacts in sorted order so the result does not depend on the order the atomics handed out slots.

Two parts of that stop working as the scene grows.

- The key packs `body_a` and `body_b` into 16 bits each. Above 65535 bodies the pair field aliases, unrelated contacts compare equal, and the apply order within an island follows the atomic slot order. At 8192 envs (about 98k bodies) the trainer hash changes run to run; every smaller count is stable.
- `restitution_capacity` is 2048 gathered contacts. Past that the stage abandons the sort and a single lane walks all `num_contacts` in index order. At 8192 envs the narrow phase holds about 175k contacts, so this costs 14.6 ms per tick (29 ms per frame, 47 % of the frame) against 0.68 ms at 4096.

## Change

Key the sort on the full body indices and split the work by island across workgroups.

- The key becomes two parts: `hi = (body_a << 32) | body_b` as `uint64` and `lo = (replayed << 24) | feature_rank24` as `uint32`, compared lexicographically. For bodies below 65536 the old single key ordered contacts by exactly `(body_a, body_b, replayed, feature_rank)`, so the sorted order is unchanged there. LDS grows from 12 B to 16 B per entry (32 KB at capacity).
- The stage dispatches `ceil_div(max(island_count, 1), adjacency_workgroup_size)` workgroups. Workgroup `g` owns islands `[g * wg, (g + 1) * wg)`. Every workgroup scans all contacts; each gathers only the gated contacts whose island falls in its range, sorts that subset, and lane `t` applies island `g * wg + t` in sorted order. Islands are disjoint body sets and a contact that reaches the per-island path only writes bodies of its own island (the other body is locked), so the per-island apply sequence is the same as the single-workgroup one.
- Every workgroup still evaluates the stray/split test on every gated contact, so all of them observe the same `restitution_serial` flag. When it is set, workgroups other than 0 exit and workgroup 0 regathers every gated contact and runs the existing single-workgroup path unchanged: sort and one-lane apply, or the index-order walk when the count exceeds capacity.
- When a workgroup's own subset exceeds capacity, one lane of that workgroup walks all contacts in index order applying only those whose island is in its range. This is the existing fallback restricted to the workgroup's islands.
- The peak-count diagnostic in `collision_state[state_tick_hash_base_index]` becomes the largest per-workgroup subset instead of the global count. Nothing reads it on the CPU.

## Why this is bit-identical

Below 65536 bodies the two-part key sorts identically to the old key. With one workgroup (island count at most `adjacency_workgroup_size`, the default trainer counts up to 1024 envs) the shader takes the same path with the same data. With several workgroups each island's contacts are applied by exactly one lane in the same relative order as before, and the bodies touched by different islands do not overlap. The only observable difference is when the global gathered count exceeds capacity while every per-workgroup subset fits: the old path applied in contact index order, the new one in sorted order. Both are valid; the trainer hashes at 1024 envs must not move, and the 4096 hash is compared before and after so any such change is recorded rather than assumed.

## Gate

- Hash gate on both backends at the default env count (Vulkan `AC3D5339…`, DX12 `F838905F…` at 409600 steps).
- 4096-env hash before and after (80 updates, `A3C7A502…` on gpuchain21).
- Two 8192-env runs must produce the same hash (today they differ).
- Interleaved A/B against gpuchain21 at 8192 and 4096 envs with the shader-swap harness.
