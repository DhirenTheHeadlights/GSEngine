import common;

public [[vk::binding(0, 0)]] Sampler2D g_albedo;
public [[vk::binding(1, 0)]] Sampler2D g_normal;
public [[vk::binding(2, 0)]] Sampler2D g_depth;
public [[vk::binding(3, 0)]] Sampler2D g_position;

public [[vk::binding(4, 0)]]
cbuffer CameraParams {
    float4x4 inv_proj;
    float4x4 inv_view;
    float3 view_pos;
    float _pad_cam0;
};

public [[vk::binding(5, 0)]] StructuredBuffer<Light> lights_ssbo;
public [[vk::binding(6, 0)]] Sampler2D extraTex0;
public [[vk::binding(7, 0)]] Sampler2D extraTex1;

public [[vk::binding(8, 0)]]
cbuffer ShadowParams {
    float4x4 light_matrices[MAX_LIGHTS];
    int shadow_light_count;
    float3 _pad_shadow;
};

public [[vk::binding(9, 0)]] Sampler2D shadow_maps[MAX_LIGHTS];
