import common;
import standard_3d;

[[vk::push_constant]]
cbuffer ModelPC {
    float4x4 model;
    float4x4 normal_matrix; 
};

struct VS_OUTPUT {
    float4 clip_pos : SV_Position;
    float3 normal : NORMAL;
    float2 tex_coord : TEXCOORD;
    float3 dbg_world_pos : TEXCOORD1; // DEBUG: Output for world position
};

[Layout(LayoutKind::Standard3D)]
[shader("vertex")]
VS_OUTPUT vs_main(float3 in_position: POSITION, float3 in_normal: NORMAL, float2 in_tex_coord: TEXCOORD) {
    VS_OUTPUT output;

    float4x4 mvp = mul(proj, mul(view, model));

    output.clip_pos = mul(mvp, float4(in_position, 1.0));
    output.normal = normalize(mul((float3x3)normal_matrix, in_normal));
    output.tex_coord = in_tex_coord;

    return output;
}

float2 encode_octahedron(float3 n) {
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    if (n.z < 0.0) {
        n.xy = (1.0 - abs(n.yx)) * sign(n.xy);
    }
    return n.xy;
}

struct FS_OUTPUT {
    float3 out_albedo : SV_Target0;
    float2 out_normal : SV_Target1;
};

[Layout(LayoutKind::Standard3D)]
[shader("fragment")]
FS_OUTPUT fs_main(VS_OUTPUT input) {
    FS_OUTPUT output;

    output.out_albedo = diffuseSampler.Sample(input.tex_coord).rgb;
    float3 normal_view = normalize(input.normal);
    output.out_normal = encode_octahedron(normal_view);
    
    return output;
}