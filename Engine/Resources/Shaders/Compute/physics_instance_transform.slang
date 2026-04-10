import vbd_shared;

struct PhysicsMapping {
    uint body_index;
    uint instance_index;
    float3 center_of_mass;
    float _pad;
};

struct InstanceData {
    float4x4 model_matrix;
    float4x4 normal_matrix;
    uint skin_offset;
    uint joint_count;
    uint _pad0;
    uint _pad1;
};

[[vk::binding(0, 0)]]
StructuredBuffer<GpuBody> body_data;

[[vk::binding(1, 0)]]
StructuredBuffer<PhysicsMapping> mapping_data;

[[vk::binding(2, 0)]]
RWStructuredBuffer<InstanceData> instance_data;

struct push_constants {
    uint mapping_count;
};

[[vk::push_constant]]
ConstantBuffer<push_constants> pc;

float3x3 quat_to_mat3(float4 q) {
    float w = q.x, x = q.y, y = q.z, z = q.w;
    float x2 = x + x, y2 = y + y, z2 = z + z;
    float xx = x * x2, xy = x * y2, xz = x * z2;
    float yy = y * y2, yz = y * z2, zz = z * z2;
    float wx = w * x2, wy = w * y2, wz = w * z2;

    return float3x3(
        float3(1.0 - (yy + zz), xy + wz, xz - wy),
        float3(xy - wz, 1.0 - (xx + zz), yz + wx),
        float3(xz + wy, yz - wx, 1.0 - (xx + yy))
    );
}

[shader("compute")]
[numthreads(64, 1, 1)]
void cs_main(uint3 dispatch_id : SV_DispatchThreadID) {
    uint tid = dispatch_id.x;
    if (tid >= pc.mapping_count) return;

    PhysicsMapping m = mapping_data[tid];
    GpuBody body = body_data[m.body_index];

    float3 pos = body.position.xyz;
    float4 q = body.orientation;
    float3 scale = body.half_extents.xyz * 2.0;

    float3x3 rot = quat_to_mat3(q);

    float3x3 rs = float3x3(
        rot[0] * scale.x,
        rot[1] * scale.y,
        rot[2] * scale.z
    );

    float3 pivot = m.center_of_mass;
    float3 scaled_pivot = float3(
        dot(rs[0], pivot),
        dot(rs[1], pivot),
        dot(rs[2], pivot)
    );

    float4x4 model;
    model[0] = float4(rs[0], 0.0);
    model[1] = float4(rs[1], 0.0);
    model[2] = float4(rs[2], 0.0);
    model[3] = float4(pos - scaled_pivot, 1.0);

    float3 inv_scale = float3(1.0 / scale.x, 1.0 / scale.y, 1.0 / scale.z);
    float3x3 normal_3x3 = float3x3(
        rot[0] * inv_scale.x,
        rot[1] * inv_scale.y,
        rot[2] * inv_scale.z
    );

    float4x4 normal;
    normal[0] = float4(normal_3x3[0], 0.0);
    normal[1] = float4(normal_3x3[1], 0.0);
    normal[2] = float4(normal_3x3[2], 0.0);
    normal[3] = float4(0.0, 0.0, 0.0, 1.0);

    InstanceData inst;
    inst.model_matrix = model;
    inst.normal_matrix = normal;
    inst.skin_offset = 0;
    inst.joint_count = 0;
    inst._pad0 = 0;
    inst._pad1 = 0;

    instance_data[m.instance_index] = inst;
}
