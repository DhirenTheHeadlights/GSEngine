import common;
import meshlet_common;

[[vk::binding(0, 0)]]
cbuffer CameraUBO {
    float4x4 view;
    float4x4 proj;
};

[[vk::binding(0, 1)]] StructuredBuffer<Vertex> vertices;
[[vk::binding(1, 1)]] StructuredBuffer<MeshletDescriptor> meshlets;
[[vk::binding(2, 1)]] StructuredBuffer<uint> meshletVertexIndices;
[[vk::binding(3, 1)]] ByteAddressBuffer meshletTriangles;
[[vk::binding(4, 1)]] StructuredBuffer<MeshletBounds> meshletBounds;
[[vk::binding(5, 1)]] StructuredBuffer<InstanceData> instanceData;

[[vk::push_constant]]
cbuffer PushConstants {
    uint meshlet_offset;
    uint meshlet_count;
    uint first_instance;
};

groupshared MeshletPayload s_payload;

[shader("amplification")]
[numthreads(32, 1, 1)]
void as_main(uint gtid : SV_GroupThreadID, uint3 gid : SV_GroupID) {
    uint meshlet_idx = gid.x * 32 + gtid;
    uint instance_index = first_instance + gid.y;
    bool visible = false;

    if (meshlet_idx < meshlet_count) {
        uint global_idx = meshlet_offset + meshlet_idx;
        MeshletBounds bounds = meshletBounds[global_idx];
        InstanceData inst = instanceData[instance_index];

        float3 world_center = mul(inst.model_matrix, float4(bounds.center, 1.0)).xyz;

        float3 scale_sq = float3(
            dot(inst.model_matrix[0].xyz, inst.model_matrix[0].xyz),
            dot(inst.model_matrix[1].xyz, inst.model_matrix[1].xyz),
            dot(inst.model_matrix[2].xyz, inst.model_matrix[2].xyz)
        );
        float max_scale = sqrt(max(scale_sq.x, max(scale_sq.y, scale_sq.z)));
        float world_radius = bounds.radius * max_scale;

        visible = !frustum_cull_sphere(world_center, world_radius, view, proj);

        if (visible && bounds.cone_cutoff >= 0.0) {
            float3 world_cone_axis = normalize(mul((float3x3)inst.model_matrix, bounds.cone_axis));
            float3 view_center = mul(view, float4(world_center, 1.0)).xyz;
            float3 view_cone_axis = normalize(mul((float3x3)view, world_cone_axis));
            float3 view_dir = normalize(-view_center);
            float cone_sin = sqrt(1.0 - bounds.cone_cutoff * bounds.cone_cutoff);
            if (dot(view_cone_axis, view_dir) < -cone_sin) {
                visible = false;
            }
        }
    }

    if (gtid == 0) {
        s_payload.count = 0;
        s_payload.instance_idx = instance_index;
    }
    GroupMemoryBarrierWithGroupSync();

    if (visible) {
        uint slot;
        InterlockedAdd(s_payload.count, 1, slot);
        s_payload.meshlet_indices[slot] = meshlet_offset + meshlet_idx;
    }
    GroupMemoryBarrierWithGroupSync();

    DispatchMesh(s_payload.count, 1, 1, s_payload);
}

struct DepthOutput {
    precise float4 clip_pos : SV_Position;
};

[shader("mesh")]
[numthreads(128, 1, 1)]
[outputtopology("triangle")]
void ms_main(
    uint gtid : SV_GroupThreadID,
    uint3 group_id : SV_GroupID,
    in payload MeshletPayload p,
    OutputVertices<DepthOutput, 64> out_verts,
    OutputIndices<uint3, 124> out_tris
) {
    uint meshlet_global_idx = p.meshlet_indices[group_id.x];
    MeshletDescriptor desc = meshlets[meshlet_global_idx];
    InstanceData inst = instanceData[p.instance_idx];

    SetMeshOutputCounts(desc.vertex_count, desc.triangle_count);

    if (gtid < desc.vertex_count) {
        uint global_vertex_idx = meshletVertexIndices[desc.vertex_offset + gtid];
        Vertex v = vertices[global_vertex_idx];

        MeshVertexOutput mv = compute_mesh_vertex(v, inst, view, proj);

        DepthOutput o;
        o.clip_pos = mv.clip_pos;
        out_verts[gtid] = o;
    }

    if (gtid < desc.triangle_count) {
        out_tris[gtid] = load_meshlet_triangle(meshletTriangles, desc.triangle_offset + gtid * 3);
    }
}

[shader("fragment")]
void fs_main(DepthOutput input) {
}
