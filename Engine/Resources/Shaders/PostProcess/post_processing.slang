import common;
import post_process;

[[vk::push_constant]]
cbuffer PushConstants {
    float exposure;
    bool  hdr;
    bool  bloom;
};

struct VSIn {
    float2 position   : POSITION;
    float2 tex_coords : TEXCOORD;
};

struct VSOut {
    float4 clip_pos   : SV_Position;
    float2 tex_coords : TEXCOORD;
};

[Layout(LayoutKind::PostProcess)]
[shader("vertex")]
VSOut vs_main(float2 in_position: POSITION, float2 in_tex_coords: TEXCOORD) {
    VSOut o;
    o.tex_coords = in_tex_coords;
    o.clip_pos   = float4(in_position, 0.0, 1.0);
    return o;
}

[Layout(LayoutKind::PostProcess)]
[shader("fragment")]
float4 fs_main(VSOut i) : SV_Target0 {
    float3 result = inputTex0.Sample(i.tex_coords).rgb;

    if (bloom) {
        float3 bloom_color = inputTex1.Sample(i.tex_coords).rgb;
        result += bloom_color;
    }

    const float gamma = 2.2;

    if (hdr) {
        float3 mapped = float3(1.0, 1.0, 1.0) - exp(-result * exposure);
        mapped = pow(mapped, 1.0 / gamma);
        return float4(mapped, 1.0);
    }
    else {
        float3 mapped = pow(result, 1.0 / gamma);
        return float4(mapped, 1.0);
    }
}
