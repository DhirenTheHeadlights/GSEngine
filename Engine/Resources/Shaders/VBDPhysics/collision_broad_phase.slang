import common;
import vbd_shared;
import vbd_physics;

[[vk::push_constant]]
ConstantBuffer<vbd_push_constants> pc;

[Layout(LayoutKind::VBDPhysics)]
[shader("compute")]
[numthreads(64, 1, 1)]
void cs_main(uint3 dispatch_id : SV_DispatchThreadID) {
    uint n = pc.body_count;
    uint total_pairs = n * (n - 1) / 2;
    uint tid = dispatch_id.x;
    if (tid >= total_pairs) return;

    float fn = float(n);
    float ftid = float(tid);
    float term = 2.0f * fn - 1.0f;
    uint i = (uint)floor((term - sqrt(term * term - 8.0f * ftid)) * 0.5f);
    uint j = tid - i * (2 * n - i - 1) / 2 + i + 1;

    if (i >= n || j >= n || i >= j) return;

    bool i_locked = (body_data[i].flags & FLAG_LOCKED) != 0;
    bool j_locked = (body_data[j].flags & FLAG_LOCKED) != 0;
    if (i_locked && j_locked) return;

    float3 min_a = body_data[i].aabb_min.xyz;
    float3 max_a = body_data[i].aabb_max.xyz;
    float3 min_b = body_data[j].aabb_min.xyz;
    float3 max_b = body_data[j].aabb_max.xyz;

    float margin = pc.speculative_margin;
    bool overlap =
        min_a.x - margin <= max_b.x && max_a.x + margin >= min_b.x &&
        min_a.y - margin <= max_b.y && max_a.y + margin >= min_b.y &&
        min_a.z - margin <= max_b.z && max_a.z + margin >= min_b.z;

    if (!overlap) return;

    uint slot;
    InterlockedAdd(collision_pairs[0], 1, slot);

    if (slot >= MAX_COLLISION_PAIRS) {
        InterlockedAdd(collision_pairs[0], -1, slot);
        return;
    }

    collision_pairs[1 + slot * 2] = i;
    collision_pairs[1 + slot * 2 + 1] = j;
}
