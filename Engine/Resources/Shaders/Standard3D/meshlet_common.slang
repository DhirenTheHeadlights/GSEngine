public struct Vertex {
    public float3 position;
    public float3 normal;
    public float2 tex_coords;
};

public struct MeshletDescriptor {
    public uint vertex_offset;
    public uint triangle_offset;
    public uint vertex_count;
    public uint triangle_count;
};

public struct MeshletBounds {
    public float3 center;
    public float radius;
    public float3 cone_axis;
    public float cone_cutoff;
};

public struct InstanceData {
    public float4x4 model_matrix;
    public float4x4 normal_matrix;
    public uint skin_offset;
    public uint joint_count;
    public uint _pad0;
    public uint _pad1;
};

public struct MeshletPayload {
    public uint meshlet_indices[32];
    public uint count;
    public uint instance_idx;
};

public bool frustum_cull_sphere(float3 center, float radius, float4x4 view, float4x4 proj) {
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

public struct MeshVertexOutput {
    public precise float4 clip_pos;
    public float3 world_pos;
    public float3 view_pos;
    public float3 view_normal;
    public float2 tex_coord;
};

public MeshVertexOutput compute_mesh_vertex(Vertex v, InstanceData inst, float4x4 view, float4x4 proj) {
    MeshVertexOutput o;

    precise float4 world_pos = mul(inst.model_matrix, float4(v.position, 1.0));
    precise float4 view_pos = mul(view, world_pos);
    precise float4 clip_pos = mul(proj, view_pos);

    float3 world_n = normalize(mul((float3x3)inst.normal_matrix, v.normal));
    float3 view_n = normalize(mul((float3x3)view, world_n));

    o.clip_pos = clip_pos;
    o.world_pos = world_pos.xyz;
    o.view_pos = view_pos.xyz;
    o.view_normal = view_n;
    o.tex_coord = v.tex_coords;
    return o;
}

public uint3 load_meshlet_triangle(ByteAddressBuffer triangles, uint byte_offset) {
    uint word_offset = byte_offset & ~3u;
    uint byte_in_word = byte_offset & 3u;

    uint word0 = triangles.Load(word_offset);
    uint combined = word0 >> (byte_in_word * 8);
    if (byte_in_word > 1) {
        uint word1 = triangles.Load(word_offset + 4);
        combined |= word1 << (32 - byte_in_word * 8);
    }
    uint i0 = combined & 0xFF;
    uint i1 = (combined >> 8) & 0xFF;
    uint i2 = (combined >> 16) & 0xFF;

    return uint3(i0, i1, i2);
}
