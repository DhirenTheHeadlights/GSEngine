import common;

[[vk::binding(0, 0)]]
public cbuffer CameraUBO {
    public float4x4 view;
    public float4x4 proj;
};

struct Vertex {
    float3 position;
    float3 normal;
    float2 tex_coords;
};

struct MeshletDescriptor {
    uint vertex_offset;
    uint triangle_offset;
    uint vertex_count;
    uint triangle_count;
};

struct MeshletBounds {
    float3 center;
    float radius;
    float3 cone_axis;
    float cone_cutoff;
};

struct InstanceData {
    float4x4 model_matrix;
    float4x4 normal_matrix;
    uint skin_offset;
    uint joint_count;
    uint _pad0;
    uint _pad1;
};

[[vk::binding(0, 1)]] StructuredBuffer<Vertex> vertices;
[[vk::binding(1, 1)]] StructuredBuffer<MeshletDescriptor> meshlets;
[[vk::binding(2, 1)]] StructuredBuffer<uint> meshletVertexIndices;
[[vk::binding(3, 1)]] ByteAddressBuffer meshletTriangles;
[[vk::binding(4, 1)]] StructuredBuffer<MeshletBounds> meshletBounds;
[[vk::binding(5, 1)]] StructuredBuffer<InstanceData> instanceData;
[[vk::binding(6, 1)]] Sampler2D diffuseSampler;

[[vk::push_constant]]
cbuffer PushConstants {
    uint meshlet_offset;
    uint meshlet_count;
    uint instance_index;
};

struct MeshletPayload {
    uint meshlet_indices[32];
    uint count;
    uint instance_idx;
};

groupshared MeshletPayload s_payload;

bool frustum_cull_sphere(float3 center, float radius) {
    float4 c = float4(center, 1.0);

    float4x4 vp = mul(proj, view);

    for (int i = 0; i < 6; i++) {
        float4 plane;
        if (i == 0) plane = vp[3] + vp[0];
        else if (i == 1) plane = vp[3] - vp[0];
        else if (i == 2) plane = vp[3] + vp[1];
        else if (i == 3) plane = vp[3] - vp[1];
        else if (i == 4) plane = vp[3] + vp[2];
        else plane = vp[3] - vp[2];

        float len = length(plane.xyz);
        if (len > 0.0) {
            plane /= len;
        }

        if (dot(plane.xyz, center) + plane.w + radius < 0.0) {
            return true;
        }
    }
    return false;
}

[shader("amplification")]
[numthreads(32, 1, 1)]
void as_main(uint gtid : SV_GroupThreadID, uint gid : SV_GroupID) {
    uint meshlet_idx = gid * 32 + gtid;
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

        visible = !frustum_cull_sphere(world_center, world_radius);

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

struct MeshOutput {
    float4 clip_pos    : SV_Position;
    float3 view_pos    : TEXCOORD0;
    float3 view_normal : TEXCOORD1;
    float2 tex_coord   : TEXCOORD2;
};

[shader("mesh")]
[numthreads(64, 1, 1)]
[outputtopology("triangle")]
void ms_main(
    uint gtid : SV_GroupThreadID,
    uint3 group_id : SV_GroupID,
    in payload MeshletPayload p,
    OutputVertices<MeshOutput, 64> out_verts,
    OutputIndices<uint3, 124> out_tris
) {
    uint meshlet_global_idx = p.meshlet_indices[group_id.x];
    MeshletDescriptor desc = meshlets[meshlet_global_idx];
    InstanceData inst = instanceData[p.instance_idx];

    SetMeshOutputCounts(desc.vertex_count, desc.triangle_count);

    if (gtid < desc.vertex_count) {
        uint global_vertex_idx = meshletVertexIndices[desc.vertex_offset + gtid];
        Vertex v = vertices[global_vertex_idx];

        float4 world_pos = mul(inst.model_matrix, float4(v.position, 1.0));
        float4 view_pos = mul(view, world_pos);
        float4 clip_pos = mul(proj, view_pos);

        float3 world_n = normalize(mul((float3x3)inst.normal_matrix, v.normal));
        float3 view_n = normalize(mul((float3x3)view, world_n));

        MeshOutput mo;
        mo.clip_pos = clip_pos;
        mo.view_pos = view_pos.xyz;
        mo.view_normal = view_n;
        mo.tex_coord = v.tex_coords;
        out_verts[gtid] = mo;
    }

    if (gtid < desc.triangle_count) {
        uint byte_offset = (desc.triangle_offset + gtid * 3);
        uint word_offset = byte_offset & ~3u;
        uint byte_in_word = byte_offset & 3u;

        uint word0 = meshletTriangles.Load(word_offset);
        uint word1 = meshletTriangles.Load(word_offset + 4);

        uint combined = (word1 << (32 - byte_in_word * 8)) | (word0 >> (byte_in_word * 8));
        uint i0 = combined & 0xFF;
        uint i1 = (combined >> 8) & 0xFF;
        uint i2 = (combined >> 16) & 0xFF;

        out_tris[gtid] = uint3(i0, i1, i2);
    }
}

float2 encode_octahedron(float3 n) {
    n /= abs(n.x) + abs(n.y) + abs(n.z);
    float2 enc = n.xy;
    if (n.z < 0.0) {
        enc = (1.0 - abs(enc.yx)) * float2(
            enc.x >= 0.0 ? 1.0 : -1.0,
            enc.y >= 0.0 ? 1.0 : -1.0
        );
    }
    return enc;
}

struct FS_OUTPUT {
    float3 out_albedo : SV_Target0;
    float2 out_normal : SV_Target1;
};

[shader("fragment")]
FS_OUTPUT fs_main(MeshOutput input) {
    FS_OUTPUT o;
    o.out_albedo = diffuseSampler.Sample(input.tex_coord).rgb;
    o.out_normal = encode_octahedron(normalize(input.view_normal));
    return o;
}
