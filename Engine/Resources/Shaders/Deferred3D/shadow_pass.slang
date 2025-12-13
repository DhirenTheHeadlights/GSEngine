import common;

[[vk::push_constant]]
cbuffer PushConstants {
    float4x4 light_view_proj;
    float4x4 model;
};

[Layout(LayoutKind::Deferred3D)]
[shader("vertex")]
float4 vs_main(float3 in_position : POSITION) : SV_Position {
    float4 pos_world = mul(float4(in_position, 1.0), model);
    return mul(pos_world, light_view_proj);
}

[Layout(LayoutKind::Deferred3D)]
[shader("fragment")]
void fs_main() {}
