// entry:
uint instance_id = group_id.x;
if (instance_id >= pc.instance_count) {
    return;
}

uint local_pose_base = instance_id * pc.local_pose_stride;
uint skin_base = instance_id * pc.skin_stride;

if (group_thread_id.x == 0) {
    for (uint i = 0; i < pc.joint_count; ++i) {
        float4x4 local_pose = local_poses[local_pose_base + i];
        uint parent_idx = skeleton_data[i].parent_index;

        float4x4 global_pose;
        if (parent_idx == 0xFFFFFFFF || parent_idx == 0xFFFF || parent_idx >= pc.joint_count) {
            global_pose = local_pose;
        } else {
            global_pose = mul(skin_matrices[skin_base + parent_idx], local_pose);
        }

        skin_matrices[skin_base + i] = global_pose;
    }
}

GroupMemoryBarrierWithGroupSync();

for (uint j = group_thread_id.x; j < pc.joint_count; j += 64) {
    float4x4 global_pose = skin_matrices[skin_base + j];
    float4x4 inv_bind = skeleton_data[j].inverse_bind;
    skin_matrices[skin_base + j] = mul(global_pose, inv_bind);
}
