import common;

static const uint TILE_SIZE = 16;
static const uint MAX_LIGHTS_PER_TILE = 64;

[[vk::binding(0, 0)]] Sampler2D g_depth;

[[vk::binding(1, 0)]]
cbuffer CullingParams {
    float4x4 projection;
    float4x4 inv_proj;
    uint2 screen_size;
    uint num_lights;
};

[[vk::binding(2, 0)]] StructuredBuffer<Light> lights;

[[vk::binding(3, 0)]] RWStructuredBuffer<uint> light_index_list;

[[vk::binding(4, 0)]] RWStructuredBuffer<uint2> tile_light_table;

groupshared uint tile_light_count;
groupshared uint tile_light_indices[MAX_LIGHTS_PER_TILE];
groupshared uint tile_depth_min;
groupshared uint tile_depth_max;

float2 screen_uv(float2 pixel) {
    return pixel / float2(screen_size);
}

float3 reconstruct_view_position(float2 uv, float depth) {
    float2 ndc_xy = uv * 2.0 - 1.0;
    float4 clip = float4(ndc_xy, depth, 1.0);
    float4 view_pos_h = mul(inv_proj, clip);
    return view_pos_h.xyz / view_pos_h.w;
}

struct frustum {
    float4 planes[4];
};

float4 make_plane_from_rays(float3 a, float3 b) {
    return float4(normalize(cross(a, b)), 0.0);
}

frustum build_tile_frustum(uint2 tile_id) {
    uint2 pixel_min = tile_id * TILE_SIZE;
    uint2 pixel_max = min(pixel_min + uint2(TILE_SIZE, TILE_SIZE), screen_size);

    float3 top_left = normalize(reconstruct_view_position(screen_uv(float2(pixel_min.x, pixel_min.y)), 1.0));
    float3 top_right = normalize(reconstruct_view_position(screen_uv(float2(pixel_max.x, pixel_min.y)), 1.0));
    float3 bottom_left = normalize(reconstruct_view_position(screen_uv(float2(pixel_min.x, pixel_max.y)), 1.0));
    float3 bottom_right = normalize(reconstruct_view_position(screen_uv(float2(pixel_max.x, pixel_max.y)), 1.0));

    frustum f;
    f.planes[0] = make_plane_from_rays(bottom_left, top_left);
    f.planes[1] = make_plane_from_rays(top_right, bottom_right);
    f.planes[2] = make_plane_from_rays(top_left, top_right);
    f.planes[3] = make_plane_from_rays(bottom_right, bottom_left);
    return f;
}

bool sphere_inside_frustum(float3 center, float radius, frustum f, float z_near, float z_far) {
    float center_depth = -center.z;
    if (center_depth + radius < z_near || center_depth - radius > z_far) {
        return false;
    }

    for (uint i = 0; i < 4; ++i) {
        if (dot(f.planes[i].xyz, center) + f.planes[i].w < -radius) {
            return false;
        }
    }

    return true;
}

float light_radius(Light light) {
    float max_channel = max(light.color.r, max(light.color.g, light.color.b)) * light.intensity;
    float threshold = 0.01;
    float a = light.quadratic;
    float b = light.linear;
    float c = light.constant - max_channel / threshold;

    if (a < 1e-6) {
        if (b > 1e-6) return -c / b;
        return 100.0;
    }

    float discriminant = b * b - 4.0 * a * c;
    if (discriminant < 0.0) return 100.0;
    return (-b + sqrt(discriminant)) / (2.0 * a);
}

[numthreads(TILE_SIZE, TILE_SIZE, 1)]
void cs_main(
    uint3 group_id : SV_GroupID,
    uint3 local_id : SV_GroupThreadID,
    uint local_index : SV_GroupIndex
) {
    uint2 tile_count = (screen_size + TILE_SIZE - 1) / TILE_SIZE;
    uint tile_index = group_id.y * tile_count.x + group_id.x;
    uint2 pixel = group_id.xy * TILE_SIZE + local_id.xy;

    if (local_index == 0) {
        tile_light_count = 0;
        tile_depth_min = 0x7F7FFFFF;
        tile_depth_max = 0;
    }

    GroupMemoryBarrierWithGroupSync();

    float depth = 1.0;
    if (pixel.x < screen_size.x && pixel.y < screen_size.y) {
        float2 uv = screen_uv(float2(pixel) + 0.5);
        depth = g_depth.SampleLevel(uv, 0).r;
    }

    if (depth < 1.0 - 1e-5) {
        float view_depth = -reconstruct_view_position(screen_uv(float2(pixel) + 0.5), depth).z;
        uint d_bits = asuint(view_depth);
        InterlockedMin(tile_depth_min, d_bits);
        InterlockedMax(tile_depth_max, d_bits);
    }

    GroupMemoryBarrierWithGroupSync();

    frustum tile_frustum = build_tile_frustum(group_id.xy);
    float z_near = tile_depth_min == 0x7F7FFFFF ? 0.0 : asfloat(tile_depth_min);
    float z_far  = tile_depth_min == 0x7F7FFFFF ? 1e30 : asfloat(tile_depth_max);

    for (uint i = local_index; i < num_lights; i += TILE_SIZE * TILE_SIZE) {
        Light light = lights[i];
        bool visible;
        if (light.light_type == LightType::Directional) {
            visible = true;
        } else {
            float radius = light_radius(light);
            visible = sphere_inside_frustum(light.position, radius, tile_frustum, z_near, z_far);
        }
        if (visible) {
            uint slot;
            InterlockedAdd(tile_light_count, 1, slot);
            if (slot < MAX_LIGHTS_PER_TILE) {
                tile_light_indices[slot] = i;
            }
        }
    }

    GroupMemoryBarrierWithGroupSync();

    uint count = min(tile_light_count, MAX_LIGHTS_PER_TILE);
    uint list_offset = tile_index * MAX_LIGHTS_PER_TILE;

    if (local_index == 0) {
        tile_light_table[tile_index] = uint2(list_offset, count);
    }

    for (uint j = local_index; j < count; j += TILE_SIZE * TILE_SIZE) {
        light_index_list[list_offset + j] = tile_light_indices[j];
    }
}
