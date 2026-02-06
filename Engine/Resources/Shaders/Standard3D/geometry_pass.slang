import common;
import standard_3d;

struct VS_OUTPUT {
    float4 clip_pos    : SV_Position;
    float3 view_pos    : TEXCOORD0;
    float3 view_normal : TEXCOORD1;
    float2 tex_coord   : TEXCOORD2;
};

[Layout(LayoutKind::Standard3D)]
[shader("vertex")]
VS_OUTPUT vs_main(float3 in_position: POSITION, float3 in_normal: NORMAL, float2 in_tex_coord: TEXCOORD, uint instance_id : SV_InstanceID, uint base_instance : SV_StartInstanceLocation) {
    VS_OUTPUT o;

    InstanceData inst = instanceData[base_instance + instance_id];

    float4 world_pos_h = mul(inst.model_matrix, float4(in_position, 1.0));
    float4 view_pos_h  = mul(view, world_pos_h);
    float4 clip_pos_h  = mul(proj, view_pos_h);

    float3 world_n = normalize(mul((float3x3)inst.normal_matrix, in_normal));
    float3 view_n  = normalize(mul((float3x3)view, world_n));

    o.clip_pos    = clip_pos_h;
    o.view_pos    = view_pos_h.xyz;
    o.view_normal = view_n;
    o.tex_coord   = in_tex_coord;

    return o;
}

float2 encode_octahedron(float3 n) {
    n /= abs(n.x) + abs(n.y) + abs(n.z);
    float2 enc = n.xy;
    if (n.z < 0.0) {
        enc = (1.0 - abs(enc.yx)) * float2(
            enc.x >= 0.0 ? 1.0 : -1.0,
            enc.y >= 0.0 ? 1.0 : -1.0
        );
    }
    return enc;
}

struct FS_OUTPUT {
    float3 out_albedo : SV_Target0;
    float2 out_normal : SV_Target1;
};

[Layout(LayoutKind::Standard3D)]
[shader("fragment")]
FS_OUTPUT fs_main(VS_OUTPUT input) {
    FS_OUTPUT o;

    float3 base_color = diffuseSampler.Sample(input.tex_coord).rgb;

    o.out_albedo = base_color;
    o.out_normal = encode_octahedron(normalize(input.view_normal));

    return o;
}
