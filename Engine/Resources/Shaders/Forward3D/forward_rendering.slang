import common;
import forward_3d_layout;

[[vk::push_constant]]
cbuffer PushConstants
{
    float4x4 view;
    float4x4 projection;
    float4x4 model;
    float3 view_pos;
    float3 color;
    bool use_texture;
    int num_lights; 
};


struct VS_OUTPUT
{
    float4 clip_pos     : SV_Position;
    float3 world_pos    : WORLD_POS;
    float3 world_normal : NORMAL;
    float2 tex_coords   : TEXCOORD;
};


[shader("vertex")]
VS_OUTPUT vs_main(
    float3 in_position   : POSITION,
    float3 in_normal     : NORMAL,
    float2 in_tex_coords : TEXCOORD)
{
    VS_OUTPUT output;

    output.world_pos = mul(float4(in_position, 1.0), model).xyz;
    output.world_normal = normalize(mul(in_normal, (float3x3)transpose(inverse(model))));
    output.tex_coords = in_tex_coords;
    
    float4x4 mvp = mul(mul(model, view), projection);
    output.clip_pos = mul(float4(in_position, 1.0), mvp);

    return output;
}


[shader("fragment")]
float4 fs_main(VS_OUTPUT input) : SV_Target0
{
    float3 surface_albedo = use_texture ? diffuse_texture.Sample(g_sampler, input.tex_coords).rgb : color;
    float specular_strength = albedo_spec.Sample(g_sampler, input.tex_coords).a;

    float3 view_dir = normalize(view_pos - input.world_pos);
    float3 result_color = float3(0.0, 0.0, 0.0);

    for (int i = 0; i < num_lights; ++i)
    {
        Light light = lights[i];
        float3 light_dir;
        float attenuation = 1.0;
        float spotlight_intensity = 1.0;
        
        // --- Calculate Light Direction & Attenuation ---
        if (light.light_type == 0) // Directional Light
        {
            light_dir = normalize(-light.direction);
        }
        else // Point or Spot Light
        {
            light_dir = normalize(light.position - input.world_pos);
            float distance = length(light.position - input.world_pos);
            attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
            
            if (light.light_type == 2) // Spot Light
            {
                float theta = dot(light_dir, normalize(-light.direction));
                float epsilon = light.cut_off - light.outer_cut_off;
                spotlight_intensity = clamp((theta - light.outer_cut_off) / epsilon, 0.0, 1.0);
            }
        }
        
        float3 ambient = light.ambient_strength * light.color;

        float diff = max(dot(input.world_normal, light_dir), 0.0);
        float3 diffuse = diff * light.color;

        float3 halfway_dir = normalize(light_dir + view_dir);
        float spec = pow(max(dot(input.world_normal, halfway_dir), 0.0), 32.0); 
        float3 specular_component = spec * light.color * specular_strength;

        float3 final_light = (ambient + (diffuse + specular_component) * attenuation * spotlight_intensity) * light.intensity;
        result_color += final_light * surface_albedo;
    }
    
    return float4(result_color, 1.0);
}