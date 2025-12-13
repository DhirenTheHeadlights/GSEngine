import common;
import deferred_3d;

struct VS_OUTPUT {
    float4 position : SV_Position;
    float2 uv       : UV;
};

[Layout(LayoutKind::Deferred3D)]
[shader("vertex")]
VS_OUTPUT vs_main(uint vidx : SV_VertexID) {
    VS_OUTPUT output;
    float2 p = float2((vidx << 1) & 2, vidx & 2);
    output.uv = p * 0.5;
    output.position = float4(p * 2.0 - 1.0, 0.0, 1.0);
    return output;
}

[[vk::push_constant]]
cbuffer PushConstants {
    int num_lights;
};

[Layout(LayoutKind::Deferred3D)]
[shader("fragment")]
float4 fs_main(VS_OUTPUT input) : SV_Target0 {
    float depth = g_depth.SampleLevel(input.uv, 0).r;
    if (depth >= 1.0 - 1e-5) {
        return float4(0.0, 0.0, 0.0, 1.0);
    }

    float2 ndc_xy = input.uv * 2.0 - 1.0;
    float ndc_z = depth * 2.0 - 1.0;
    float4 ndc = float4(ndc_xy, ndc_z, 1.0);

    float4 view_pos_h = mul(inv_proj, ndc);
    view_pos_h /= view_pos_h.w;

    float4 world_pos_h = mul(inv_view, view_pos_h);
    float3 pos_w = world_pos_h.xyz;

    float3 scaled = pos_w * 0.1;
    float3 wrapped = frac(scaled + 0.5);

    return float4(wrapped, 1.0);
}
