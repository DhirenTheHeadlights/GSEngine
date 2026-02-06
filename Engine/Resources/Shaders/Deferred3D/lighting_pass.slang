import common;
import deferred_3d;

struct VS_OUTPUT {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

[Layout(LayoutKind::Deferred3D)]
[shader("vertex")]
VS_OUTPUT vs_main(uint vidx: SV_VertexID) {
    VS_OUTPUT o;

    float2 verts[3] = {
        float2(-1.0, -1.0),
        float2(3.0, -1.0),
        float2(-1.0, 3.0)
    };

    float2 clip = verts[vidx];
    o.position = float4(clip, 0.0, 1.0);
    o.uv = float2(clip.x * 0.5 + 0.5, 0.5 - clip.y * 0.5);

    return o;
}

[[vk::push_constant]]
cbuffer PushConstants {
    int num_lights;
};

float3 reconstruct_view_position(float2 uv, float depth) {
    float2 ndc_xy = uv * 2.0 - 1.0;
    float4 clip = float4(ndc_xy.x, ndc_xy.y, depth, 1.0);
    float4 view_pos_h = mul(inv_proj, clip);
    view_pos_h /= view_pos_h.w;
    return view_pos_h.xyz;
}

float3 reconstruct_world_position(float3 pos_vs) {
    float4 pos_vs_h = float4(pos_vs, 1.0);
    float4 pos_world_h = mul(inv_view, pos_vs_h);
    pos_world_h /= pos_world_h.w;
    return pos_world_h.xyz;
}

float attenuation_point(float3 light_pos, float3 frag_pos, float constant, float linear_k, float quadratic) {
    float d = length(light_pos - frag_pos);
    return 1.0 / (constant + linear_k * d + quadratic * d * d);
}

float spot_factor(float3 frag_to_light, float3 spot_dir, float cut_off, float outer_cut_off) {
    float3 L = normalize(frag_to_light);
    float3 D = normalize(spot_dir);
    float cos_theta = dot(-L, D);
    float eps = cut_off - outer_cut_off;
    float t = saturate((cos_theta - outer_cut_off) / max(eps, 1e-4));
    return t;
}

float3 decode_octahedron(float2 e) {
    float3 n;
    n.z = 1.0 - abs(e.x) - abs(e.y);
    n.xy = n.z >= 0.0 ? e.xy : (1.0 - abs(e.yx)) * float2(sign(e.x), sign(e.y));
    return normalize(n);
}

float pcf_shadow(int s, float2 uv, float depth, float bias) {
    float sum = 0.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            float2 duv = float2((float)x, (float)y) * shadow_texel_size;
            float closest = shadow_maps[s].SampleLevel(uv + duv, 0).r;
            sum += (depth > closest + bias) ? 0.0 : 1.0;
        }
    }
    return sum / 9.0;
}

float compute_point_shadow(int light_index, float3 frag_pos_world) {
    for (int s = 0; s < point_shadow_count; ++s) {
        if (point_shadow_light_indices[s] != light_index) {
            continue;
        }

        float3 light_to_frag = frag_pos_world - point_shadow_positions_world[s];
        float sampled_depth = point_shadow_maps[s].SampleLevel(light_to_frag, 0).r;

        float3 abs_dir = abs(light_to_frag);
        float d = max(abs_dir.x, max(abs_dir.y, abs_dir.z));

        float n = point_shadow_near[s];
        float f = point_shadow_far[s];
        float current_depth = f * (d - n) / (d * (f - n));

        float bias = 0.05;
        return (current_depth > sampled_depth + bias) ? 0.0 : 1.0;
    }
    return 1.0;
}

float compute_shadow_for_light(int light_index, float3 pos_world) {
    for (int s = 0; s < shadow_light_count; ++s) {
        if (shadow_light_indices[s] != light_index) {
            continue;
        }

        float4 light_clip = mul(shadow_view_proj[s], float4(pos_world, 1.0));

        if (light_clip.w <= 0.0) {
            return 1.0;
        }

        float3 light_ndc = light_clip.xyz / light_clip.w;

        float2 shadow_uv = light_ndc.xy * 0.5 + 0.5;

        float depth = light_ndc.z;

        if (shadow_uv.x < 0.0 || shadow_uv.x > 1.0 || shadow_uv.y < 0.0 || shadow_uv.y > 1.0) {
            return 1.0;
        }
        if (depth < 0.0 || depth > 1.0) {
            return 1.0;
        }

        float bias = 0.005;
        return pcf_shadow(s, shadow_uv, depth, bias);
    }

    return 1.0;
}

[Layout(LayoutKind::Deferred3D)]
[shader("fragment")]
float4 fs_main(VS_OUTPUT input) : SV_Target0 {
    float2 uv = input.uv;
    uv.y = 1.0 - uv.y;

    float depth = g_depth.SampleLevel(uv, 0).r;
    if (depth >= 1.0 - 1e-5) {
        return float4(0.0, 0.0, 0.0, 1.0);
    }

    float3 pos_vs = reconstruct_view_position(uv, depth);
    float3 pos_world = reconstruct_world_position(pos_vs);
    float3 albedo = g_albedo.SampleLevel(uv, 0).rgb;
    float2 enc_n = g_normal.SampleLevel(uv, 0).rg;
    float3 normal_vs = normalize(decode_octahedron(enc_n));


    float3 result = float3(0.0, 0.0, 0.0);

    for (int i = 0; i < num_lights; ++i) {
        Light light = lights_ssbo[i];
        float shadow = compute_shadow_for_light(i, pos_world);

        if (light.light_type == 0) {
            float3 L = normalize(-light.direction);
            float NdotL = max(dot(normal_vs, L), 0.0);
            result += albedo * light.color * light.intensity * NdotL * shadow;
            result += albedo * light.color * light.ambient_strength;
        } else if (light.light_type == 1) {
            float3 Lvec = light.position - pos_vs;
            float NdotL = max(dot(normal_vs, normalize(Lvec)), 0.0);
            float att = attenuation_point(light.position, pos_vs, light.constant, light.linear, light.quadratic);
            float point_shadow = compute_point_shadow(i, pos_world);
            result += albedo * light.color * light.intensity * NdotL * att * point_shadow;
            result += albedo * light.color * light.ambient_strength;
        } else if (light.light_type == 2) {
            float3 Lvec = light.position - pos_vs;
            float NdotL = max(dot(normal_vs, normalize(Lvec)), 0.0);
            float att = attenuation_point(light.position, pos_vs, light.constant, light.linear, light.quadratic);
            float sf = spot_factor(Lvec, light.direction, light.cut_off, light.outer_cut_off);
            result += albedo * light.color * light.intensity * NdotL * att * sf * shadow;
            result += albedo * light.color * light.ambient_strength;
        }
    }

    return float4(result, 1.0);
}
