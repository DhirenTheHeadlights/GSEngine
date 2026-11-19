float scene_depth(int2 pixel) {
    return depth_in.Load(int3(clamp(pixel, int2(0, 0), int2(pc.extent) - 1), 0)).r;
}

float3 view_position_at(float2 uv, float depth) {
    float4 world = mul(camera_ubo.inv_view_proj, float4(uv * 2.0 - 1.0, depth, 1.0));
    world /= world.w;
    return mul(camera_ubo.view, float4(world.xyz, 1.0)).xyz;
}

float3 view_position(int2 pixel) {
    float2 uv = (float2(pixel) + 0.5) / float2(pc.extent);
    return view_position_at(uv, scene_depth(pixel));
}

float3 view_normal(int2 pixel, float3 center) {
    float3 left = view_position(pixel - int2(1, 0));
    float3 right = view_position(pixel + int2(1, 0));
    float3 up = view_position(pixel - int2(0, 1));
    float3 down = view_position(pixel + int2(0, 1));
    float3 dx = abs(right.z - center.z) < abs(center.z - left.z) ? right - center : center - left;
    float3 dy = abs(down.z - center.z) < abs(center.z - up.z) ? down - center : center - up;
    float3 n = normalize(cross(dy, dx));
    return dot(n, -center) < 0.0 ? -n : n;
}
