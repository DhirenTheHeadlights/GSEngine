import common;

[[vk::push_constant]]
cbuffer PushConstants {
    float4x4 light_view_proj;
    float4x4 model;
};

[Layout(LayoutKind::Deferred3D)]
[shader("vertex")]
float4 vs_main(float3 in_position : POSITION) : SV_Position {
    float4 pos_world = mul(model, float4(in_position, 1.0));
    return mul(light_view_proj, pos_world);
}

[Layout(LayoutKind::Deferred3D)]
[shader("fragment")]
void fs_main() {}
