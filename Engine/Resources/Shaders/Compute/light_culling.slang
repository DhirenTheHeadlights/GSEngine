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

float linearize_depth(float d) {
    float a = inv_proj[2][2];
    float b = inv_proj[3][2];
    return b / (d - a);
}

struct frustum {
    float4 planes[4];
};

frustum build_tile_frustum(uint2 tile_id, uint2 tile_count) {
    float2 tile_scale = float2(screen_size) / float2(TILE_SIZE);
    float2 tile_bias = tile_scale - 1.0 - 2.0 * float2(tile_id);

    float4 col0 = float4(projection[0][0] * tile_scale.x, 0.0, tile_bias.x, 0.0);
    float4 col1 = float4(0.0, -projection[1][1] * tile_scale.y, tile_bias.y, 0.0);
    float4 col3 = float4(0.0, 0.0, 1.0, 0.0);

    frustum f;
    f.planes[0] = normalize(col3 + col0);
    f.planes[1] = normalize(col3 - col0);
    f.planes[2] = normalize(col3 + col1);
    f.planes[3] = normalize(col3 - col1);
    return f;
}

bool sphere_inside_frustum(float3 center, float radius, frustum f, float z_near, float z_far) {
    if (center.z + radius < z_near || center.z - radius > z_far) {
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
        float2 uv = (float2(pixel) + 0.5) / float2(screen_size);
        depth = g_depth.SampleLevel(uv, 0).r;
    }

    float linear_d = linearize_depth(depth);
    uint d_bits = asuint(linear_d);

    if (depth < 1.0 - 1e-5) {
        InterlockedMin(tile_depth_min, d_bits);
        InterlockedMax(tile_depth_max, d_bits);
    }

    GroupMemoryBarrierWithGroupSync();

    float z_near = asfloat(tile_depth_min);
    float z_far = asfloat(tile_depth_max);

    if (tile_depth_min == 0x7F7FFFFF) {
        if (local_index == 0) {
            tile_light_table[tile_index] = uint2(0, 0);
        }
        return;
    }

    frustum tile_frustum = build_tile_frustum(group_id.xy, tile_count);

    for (uint i = local_index; i < num_lights; i += TILE_SIZE * TILE_SIZE) {
        Light light = lights[i];

        bool intersects = false;

        if (light.light_type == LightType::Directional) {
            intersects = true;
        } else {
            float r = light_radius(light);
            intersects = sphere_inside_frustum(light.position, r, tile_frustum, z_near, z_far);
        }

        if (intersects) {
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
