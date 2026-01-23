struct JointData {
    float4x4 inverse_bind;
    uint parent_index;
    uint _pad0;
    uint _pad1;
    uint _pad2;
};

[[vk::binding(0, 0)]]
StructuredBuffer<JointData> skeletonData;

[[vk::binding(1, 0)]]
StructuredBuffer<float4x4> localPoses;

[[vk::binding(2, 0)]]
RWStructuredBuffer<float4x4> skinMatrices;

struct push_constants {
    uint joint_count;
    uint instance_count;
    uint local_pose_stride;
    uint skin_stride;
};

[[vk::push_constant]]
ConstantBuffer<push_constants> pc;

[shader("compute")]
[numthreads(64, 1, 1)]
void cs_main(uint3 group_id : SV_GroupID, uint3 local_thread_id : SV_GroupThreadID) {
    uint instance_id = group_id.x;
    if (instance_id >= pc.instance_count) {
        return;
    }

    uint local_pose_base = instance_id * pc.local_pose_stride;
    uint skin_base = instance_id * pc.skin_stride;

    // Phase 1: Compute global poses from local poses (hierarchical multiplication)
    // This MUST be serial due to parent-child dependencies, so only thread 0 does this
    if (local_thread_id.x == 0) {
        for (uint i = 0; i < pc.joint_count; ++i) {
            float4x4 local_pose = localPoses[local_pose_base + i];
            uint parent_idx = skeletonData[i].parent_index;

            float4x4 global_pose;
            // Check for invalid parent (root joint): 0xFFFFFFFF, 0xFFFF (65535), or out of range
            if (parent_idx == 0xFFFFFFFF || parent_idx == 0xFFFF || parent_idx >= pc.joint_count) {
                // Root joint: global pose = local pose
                global_pose = local_pose;
            } else {
                // Child joint: global pose = parent's global pose * local pose
                // Parent was already computed in previous iteration
                global_pose = mul(skinMatrices[skin_base + parent_idx], local_pose);
            }

            // Store global pose temporarily in output buffer
            skinMatrices[skin_base + i] = global_pose;
        }
    }

    // Synchronize to ensure Phase 1 is complete before Phase 2 begins
    GroupMemoryBarrierWithGroupSync();

    // Phase 2: Apply inverse bind matrices in parallel across all threads
    // Each thread processes multiple joints if joint_count > 64
    for (uint j = local_thread_id.x; j < pc.joint_count; j += 64) {
        float4x4 global_pose = skinMatrices[skin_base + j];
        float4x4 inv_bind = skeletonData[j].inverse_bind;
        skinMatrices[skin_base + j] = mul(global_pose, inv_bind);
    }
}
