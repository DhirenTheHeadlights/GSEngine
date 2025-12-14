import common;
import deferred_3d;

struct VS_OUTPUT {
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

[Layout(LayoutKind::Deferred3D)]
[shader("vertex")]
VS_OUTPUT vs_main(uint vidx : SV_VertexID) {
    VS_OUTPUT o;

    float2 verts[3] = {
        float2(-1.0, -1.0),
        float2( 3.0, -1.0),
        float2(-1.0,  3.0)
    };

    float2 clip = verts[vidx];
    o.position = float4(clip, 0.0, 1.0);
    o.uv = clip * 0.5 + 0.5;

    return o;
}

[[vk::push_constant]]
cbuffer PushConstants {
    int num_lights;
};

float3 decode_octahedron(float2 e) {
    float3 n = float3(e.x, e.y, 1.0 - abs(e.x) - abs(e.y));
    float t = saturate(-n.z);
    n.x += n.x >= 0.0 ? -t : t;
    n.y += n.y >= 0.0 ? -t : t;
    return normalize(n);
}

[Layout(LayoutKind::Deferred3D)]
[shader("fragment")]
float4 fs_main(VS_OUTPUT input) : SV_Target0 {
    float depth = g_depth.SampleLevel(input.uv, 0).r;
    if (depth >= 1.0 - 1e-5) {
        return float4(0.0, 0.0, 0.0, 1.0);
    }

    float3 albedo = g_albedo.SampleLevel(input.uv, 0).rgb;
    float2 enc_n = g_normal.SampleLevel(input.uv, 0).rg;
    float3 n = decode_octahedron(enc_n);
    float3 pos_w = g_position.SampleLevel(input.uv, 0).rgb;

    float3 result = 0.0.xxx;

    for (int i = 0; i < num_lights; ++i) {
        Light L = lights_ssbo[i];
        if (L.light_type == 0) {
            float3 Ldir = normalize(-L.direction);
            float ndotl = saturate(dot(n, Ldir));
            float3 diffuse = ndotl * L.color * L.intensity;
            float3 ambient = L.ambient_strength * L.color;
            result += (ambient + diffuse) * albedo;
        } else if (L.light_type == 1) {
            float3 Lvec = L.position - pos_w;
            float dist = length(Lvec);
            float3 Ldir = Lvec / max(dist, 1e-4);
            float ndotl = saturate(dot(n, Ldir));
            float atten = 1.0 / (L.constant + L.linear * dist + L.quadratic * dist * dist);
            float3 diffuse = ndotl * L.color * L.intensity * atten;
            float3 ambient = L.ambient_strength * L.color;
            result += (ambient + diffuse) * albedo;
        } else if (L.light_type == 2) {
            float3 Lvec = L.position - pos_w;
            float dist = length(Lvec);
            float3 Ldir = Lvec / max(dist, 1e-4);
            float theta = dot(Ldir, -normalize(L.direction));
            float epsilon = max(L.cut_off - L.outer_cut_off, 1e-4);
            float spot = saturate((theta - L.outer_cut_off) / epsilon);
            float ndotl = saturate(dot(n, Ldir));
            float atten = 1.0 / (L.constant + L.linear * dist + L.quadratic * dist * dist);
            float3 diffuse = ndotl * L.color * L.intensity * atten * spot;
            float3 ambient = L.ambient_strength * L.color;
            result += (ambient + diffuse) * albedo;
        }
    }

    return float4(result, 1.0);
}
