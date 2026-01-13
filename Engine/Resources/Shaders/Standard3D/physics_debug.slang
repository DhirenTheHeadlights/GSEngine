import common;
import standard_3d;

struct VS_OUTPUT {
    float4 clip_pos : SV_Position;
    float3 color    : COLOR;
};

[Layout(LayoutKind::Standard3D)]
[shader("vertex")]
VS_OUTPUT vs_main(float3 in_position : POSITION, float3 in_color : COLOR) {
    VS_OUTPUT output;

    float4 world_pos = float4(in_position, 1.0);
    float4 view_pos  = mul(view, world_pos);
    output.clip_pos  = mul(proj, view_pos);
    output.color     = in_color;

    return output;
}

[Layout(LayoutKind::Standard3D)]
[shader("fragment")]
float4 fs_main(VS_OUTPUT input) : SV_Target {
    return float4(input.color, 1.0);
}
