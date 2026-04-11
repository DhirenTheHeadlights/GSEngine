import common;
import vbd_shared;
import vbd_physics;

[[vk::push_constant]]
ConstantBuffer<vbd_push_constants> pc;

int3 world_to_cell(float3 pos, float cell_size) {
    return int3(floor(pos / cell_size));
}

uint hash_cell(int3 cell) {
    uint h = uint(cell.x * 73856093 ^ cell.y * 19349663 ^ cell.z * 83492791);
    return (h % GRID_TABLE_SIZE) + 1;
}

[Layout(LayoutKind::VBDPhysics)]
[shader("compute")]
[numthreads(64, 1, 1)]
void cs_main(uint3 dispatch_id : SV_DispatchThreadID) {
    uint i = dispatch_id.x;
    if (i >= pc.body_count) {
        return;
    }

    GpuBody body_i = body_data[i];

    bool i_locked = (body_i.flags & FLAG_LOCKED) != 0;

    float margin = pc.speculative_margin;
    float3 min_a = body_i.aabb_min.xyz;
    float3 max_a = body_i.aabb_max.xyz;

    float cell_size = pc.grid_cell_size;
    int3 min_cell = world_to_cell(min_a - margin, cell_size);
    int3 max_cell = world_to_cell(max_a + margin, cell_size);

    for (int cx = min_cell.x; cx <= max_cell.x; cx++) {
        for (int cy = min_cell.y; cy <= max_cell.y; cy++) {
            for (int cz = min_cell.z; cz <= max_cell.z; cz++) {
                uint bucket = hash_cell(int3(cx, cy, cz));
                uint entry_ptr = grid_data[bucket];

                while (entry_ptr != 0 && entry_ptr >= GRID_TABLE_SIZE + 1) {
                    uint j = grid_data[entry_ptr];
                    uint next = grid_data[entry_ptr + 1];

                    if (j > i && j < pc.body_count) {
                        bool j_locked = (body_data[j].flags & FLAG_LOCKED) != 0;
                        if (!(i_locked && j_locked)) {
                            float3 min_b = body_data[j].aabb_min.xyz;
                            float3 max_b = body_data[j].aabb_max.xyz;

                            bool overlap =
                                min_a.x - margin <= max_b.x && max_a.x + margin >= min_b.x &&
                                min_a.y - margin <= max_b.y && max_a.y + margin >= min_b.y &&
                                min_a.z - margin <= max_b.z && max_a.z + margin >= min_b.z;

                            if (overlap) {
                                uint slot;
                                InterlockedAdd(collision_pairs[0], 1, slot);

                                if (slot >= MAX_COLLISION_PAIRS) {
                                    InterlockedAdd(collision_pairs[0], -1, slot);
                                    return;
                                }

                                collision_pairs[1 + slot * 2] = i;
                                collision_pairs[1 + slot * 2 + 1] = j;
                            }
                        }
                    }

                    entry_ptr = next;
                }
            }
        }
    }
}
