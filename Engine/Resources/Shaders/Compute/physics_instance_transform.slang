import vbd_shared;
import meshlet_common;

struct PhysicsMapping {
    uint body_index;
    uint instance_index;
    float3 center_of_mass;
};

[[vk::binding(0, 0)]]
StructuredBuffer<GpuBody> body_data;

[[vk::binding(1, 0)]]
StructuredBuffer<PhysicsMapping> mapping_data;

[[vk::binding(2, 0)]]
RWStructuredBuffer<InstanceData> instance_data;

struct push_constants {
    uint mapping_count;
    uint body_count;
};

[[vk::push_constant]]
ConstantBuffer<push_constants> pc;

[shader("compute")]
[numthreads(64, 1, 1)]
void cs_main(uint3 dispatch_id : SV_DispatchThreadID) {
    uint tid = dispatch_id.x;
    if (tid >= pc.mapping_count) {
        return;
    }

    PhysicsMapping m = mapping_data[tid];
    if (m.body_index >= pc.body_count) {
        return;
    }

    GpuBody body = body_data[m.body_index];
    InstanceData inst = instance_data[m.instance_index];

    float3 scale = body.half_extents.xyz * 2.0;
    float3 axis_x = rotate_vector(body.orientation, float3(scale.x, 0.0, 0.0));
    float3 axis_y = rotate_vector(body.orientation, float3(0.0, scale.y, 0.0));
    float3 axis_z = rotate_vector(body.orientation, float3(0.0, 0.0, scale.z));
    float3 pivot = m.center_of_mass;
    float3 translation = body.position.xyz - axis_x * pivot.x - axis_y * pivot.y - axis_z * pivot.z;

    inst.model_matrix[0] = float4(axis_x.x, axis_y.x, axis_z.x, translation.x);
    inst.model_matrix[1] = float4(axis_x.y, axis_y.y, axis_z.y, translation.y);
    inst.model_matrix[2] = float4(axis_x.z, axis_y.z, axis_z.z, translation.z);
    inst.model_matrix[3] = float4(0.0, 0.0, 0.0, 1.0);

    float3 inv_scale = float3(1.0, 1.0, 1.0) / max(abs(scale), float3(1e-6, 1e-6, 1e-6));
    float3 normal_x = rotate_vector(body.orientation, float3(inv_scale.x, 0.0, 0.0));
    float3 normal_y = rotate_vector(body.orientation, float3(0.0, inv_scale.y, 0.0));
    float3 normal_z = rotate_vector(body.orientation, float3(0.0, 0.0, inv_scale.z));

    inst.normal_matrix[0] = float4(normal_x.x, normal_y.x, normal_z.x, 0.0);
    inst.normal_matrix[1] = float4(normal_x.y, normal_y.y, normal_z.y, 0.0);
    inst.normal_matrix[2] = float4(normal_x.z, normal_y.z, normal_z.z, 0.0);
    inst.normal_matrix[3] = float4(0.0, 0.0, 0.0, 1.0);

    instance_data[m.instance_index] = inst;
}
