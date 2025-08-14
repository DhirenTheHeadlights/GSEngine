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
    
    output.uv = float2((vidx << 1) & 2, vidx & 2);
    output.position = float4(output.uv * 2.0 - 1.0, 0.0, 1.0);
    
    return output;
}

[[vk::push_constant]]
cbuffer PushConstants {
    int num_lights;
};

float3 decode_octa(float2 e) {
    float3 n = float3(e.xy, 1.0 - abs(e.x) - abs(e.y));
    if (n.z < 0.0) {
        n.xy = (1.0 - abs(n.yx)) * sign(n.xy);
    }
    return normalize(n);
}

[Layout(LayoutKind::Deferred3D)]
[shader("fragment")]
float4 fs_main(VS_OUTPUT input) : SV_Target0 {
    float z = g_depth.Sample(input.uv).r;
    float4 ndc = float4(input.uv * 2.0 - 1.0, z, 1.0);
    float4 pos_w_h = mul(ndc, inv_pv);
    float3 pos_w = pos_w_h.xyz / pos_w_h.w;

    float3 albedo = g_albedo.Sample(input.uv).rgb;
    float3 normal = decode_octa(g_normal.Sample(input.uv).rg);
    float specular_power = 32.0;

    float3 result_color = float3(0.0, 0.0, 0.0);
    float3 view_dir = normalize(view_pos - pos_w);

    for (int i = 0; i < num_lights; ++i)
    {
        Light light = lights_ssbo[i];
        
        float3 ambient = light.ambient_strength * light.color * albedo;
        float3 light_dir;
        float attenuation = 1.0;
        float spotlight_effect = 1.0;

        if (light.light_type == LightType::Directional) {
            light_dir = normalize(-light.direction);
        }
        else {
            light_dir = normalize(light.position - pos_w);
            float distance = length(light.position - pos_w);
            attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));

            if (light.light_type == LightType::Spot)
            {
                float theta = dot(light_dir, normalize(-light.direction));
                float epsilon = light.cut_off - light.outer_cut_off;
                spotlight_effect = clamp((theta - light.outer_cut_off) / epsilon, 0.0, 1.0);
            }
        }

        float diff = max(dot(normal, light_dir), 0.0);
        float3 diffuse = diff * light.color;

        float3 halfway_dir = normalize(light_dir + view_dir);
        float spec = pow(max(dot(normal, halfway_dir), 0.0), specular_power);
        float3 specular = spec * light.color;

        result_color += ambient + (attenuation * light.intensity * spotlight_effect * (diffuse * albedo + specular));
    }
    
    result_color += 0.03 * albedo;

    return float4(result_color, 1.0);
}