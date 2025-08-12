import common;

[[vk::binding(0, 0)]]
cbuffer LightUBO {
    float4x4 light_view;
    float4x4 light_projection;
};

[[vk::binding(1, 0)]]
cbuffer ModelUBO {
    float4x4 model;
};

[shader("vertex")]
float4 vs_main(float3 in_position : POSITION) : SV_Position
{
    float4 pos_world = mul(float4(in_position, 1.0), model);
    float4 pos_light_view = mul(pos_world, light_view);
    return mul(pos_light_view, light_projection);
}

[shader("fragment")]
void fs_main()
{
}