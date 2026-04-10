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
    uint bi = dispatch_id.x;
    if (bi >= pc.body_count) {
        return;
    }

    GpuBody body = body_data[bi];

    float cell_size = pc.grid_cell_size;
    int3 min_cell = world_to_cell(body.aabb_min.xyz - pc.speculative_margin, cell_size);
    int3 max_cell = world_to_cell(body.aabb_max.xyz + pc.speculative_margin, cell_size);

    for (int cx = min_cell.x; cx <= max_cell.x; cx++) {
        for (int cy = min_cell.y; cy <= max_cell.y; cy++) {
            for (int cz = min_cell.z; cz <= max_cell.z; cz++) {
                uint bucket = hash_cell(int3(cx, cy, cz));

                uint slot;
                InterlockedAdd(grid_data[0], 1u, slot);
                if (slot >= GRID_MAX_ENTRIES) {
                    InterlockedAdd(grid_data[0], -1);
                    return;
                }

                uint entry_base = GRID_TABLE_SIZE + 1 + slot * 2;
                uint old_head;
                InterlockedExchange(grid_data[bucket], entry_base, old_head);
                grid_data[entry_base] = bi;
                grid_data[entry_base + 1] = old_head;
            }
        }
    }
}
