groupshared uint gs_wave_c[adjacency_workgroup_size + 1];
groupshared uint gs_wave_j[adjacency_workgroup_size + 1];

void collision_scan_adjacency(uint3 dispatch_id, uint3 group_thread_id) {
    uint tid = group_thread_id.x;
    uint body_count = pc.body_count;
    uint lane_count = WaveGetLaneCount();
    uint wave_index = tid / lane_count;
    uint wave_count = (adjacency_workgroup_size + lane_count - 1u) / lane_count;
    uint carry_c = 0;
    uint carry_j = 0;

    for (uint chunk_base = 0; chunk_base < body_count; chunk_base += adjacency_workgroup_size) {
        uint bi = chunk_base + tid;
        bool active = bi < body_count;
        uint c = active ? contact_counts[bi] : 0u;
        uint j = active ? joint_counts[bi] : 0u;
        uint rank_c = WavePrefixSum(c);
        uint rank_j = WavePrefixSum(j);
        uint total_c = WaveActiveSum(c);
        uint total_j = WaveActiveSum(j);
        if (WaveIsFirstLane()) {
            gs_wave_c[wave_index] = total_c;
            gs_wave_j[wave_index] = total_j;
        }
        GroupMemoryBarrierWithGroupSync();

        if (wave_index == 0) {
            uint running_c = 0;
            uint running_j = 0;
            for (uint w0 = 0; w0 < wave_count; w0 += lane_count) {
                uint w = w0 + tid;
                bool live = w < wave_count;
                uint wc = live ? gs_wave_c[w] : 0u;
                uint wj = live ? gs_wave_j[w] : 0u;
                uint base_c = running_c + WavePrefixSum(wc);
                uint base_j = running_j + WavePrefixSum(wj);
                if (live) {
                    gs_wave_c[w] = base_c;
                    gs_wave_j[w] = base_j;
                }
                running_c += WaveActiveSum(wc);
                running_j += WaveActiveSum(wj);
            }
            if (tid == 0) {
                gs_wave_c[wave_count] = running_c;
                gs_wave_j[wave_count] = running_j;
            }
        }
        GroupMemoryBarrierWithGroupSync();

        if (active) {
            contact_offsets[bi] = min(carry_c + gs_wave_c[wave_index] + rank_c, max_contact_adjacency);
            joint_offsets[bi] = min(carry_j + gs_wave_j[wave_index] + rank_j, max_joint_adjacency);
            contact_counts[bi] = 0;
            joint_counts[bi] = 0;
        }
        carry_c += gs_wave_c[wave_count];
        carry_j += gs_wave_j[wave_count];
        GroupMemoryBarrierWithGroupSync();
    }
}
