import common;

public[[vk::binding(0, 0)]]
cbuffer CameraUBO {
    public float4x4 view;
    public float4x4 proj;
    public float4x4 inv_view;
};

public[[vk::binding(1, 0)]] StructuredBuffer<Light> lights_ssbo;

public[[vk::binding(2, 0)]]
cbuffer ShadowParams {
    public int shadow_light_count;
    public int shadow_light_indices[MAX_LIGHTS];
    public float4x4 shadow_view_proj[MAX_LIGHTS];
    public float2 shadow_texel_size;
};

public[[vk::binding(3, 0)]] Sampler2D shadow_maps[MAX_LIGHTS];

public[[vk::binding(4, 0)]]
cbuffer PointShadowParams {
    public int point_shadow_count;
    public int point_shadow_light_indices[MAX_POINT_SHADOW_LIGHTS];
    public float3 point_shadow_positions_world[MAX_POINT_SHADOW_LIGHTS];
    public float point_shadow_near[MAX_POINT_SHADOW_LIGHTS];
    public float point_shadow_far[MAX_POINT_SHADOW_LIGHTS];
};

public[[vk::binding(5, 0)]] SamplerCube point_shadow_maps[MAX_POINT_SHADOW_LIGHTS];

public static const uint TILE_SIZE = 16;
public static const uint MAX_LIGHTS_PER_TILE = 64;

public[[vk::binding(6, 0)]] StructuredBuffer<uint> light_index_list;
public[[vk::binding(7, 0)]] StructuredBuffer<uint2> tile_light_table;
