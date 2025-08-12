import standard_3d_layout;

[[vk::push_constant]]
cbuffer ModelPC {
    float4x4 model;
};

struct VS_OUTPUT
{
    float4 clip_pos     : SV_Position;
    float3 normal       : NORMAL;
    float2 tex_coord    : TEXCOORD;
};

[shader("vertex")]
VS_OUTPUT vs_main(
    float3 in_position   : POSITION,
    float3 in_normal     : NORMAL,
    float2 in_tex_coord  : TEXCOORD)
{
    VS_OUTPUT output;

    float4x4 mvp = mul(mul(model, view), proj);
    output.clip_pos = mul(float4(in_position, 1.0), mvp);
    output.normal = in_normal;
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

struct FS_OUTPUT
{
    float3 out_albedo : SV_Target0;
    float2 out_normal : SV_Target1;
};

[shader("fragment")]
FS_OUTPUT fs_main(VS_OUTPUT input)
{
    FS_OUTPUT output;

    output.out_albedo = diffuseSampler.Sample(input.tex_coord).rgb;

    float3x3 normal_matrix = (float3x3)transpose(inverse(mul(model, view)));
    float3 normal_view = normalize(mul(input.normal, normal_matrix));

    output.out_normal = encode_octahedron(normal_view);
    
    return output;
}