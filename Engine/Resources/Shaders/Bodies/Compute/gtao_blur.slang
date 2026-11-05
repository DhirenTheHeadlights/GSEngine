void gtao_blur(uint3 dispatch_id) {
    int2 pixel = int2(dispatch_id.xy);
    if (any(dispatch_id.xy >= pc.extent)) {
        return;
    }

    float center_depth = scene_depth(pixel);
    if (center_depth <= 0.0) {
        occlusion_out[pixel] = float4(1.0, 1.0, 1.0, 1.0);
        return;
    }
    float center_z = view_position(pixel).z;

    float sum = 0.0;
    float weight_sum = 0.0;
    for (int y = -2; y <= 2; ++y) {
        for (int x = -2; x <= 2; ++x) {
            int2 tap = clamp(pixel + int2(x, y), int2(0, 0), int2(pc.extent) - 1);
            if (scene_depth(tap) <= 0.0) {
                continue;
            }
            float tap_z = view_position(tap).z;
            float weight = exp(-abs(tap_z - center_z) / max(abs(center_z) * 0.02, 1e-3));
            sum += occlusion_in.Load(int3(tap, 0)).r * weight;
            weight_sum += weight;
        }
    }

    float ao = weight_sum > 0.0 ? sum / weight_sum : 1.0;
    occlusion_out[pixel] = float4(ao, ao, ao, 1.0);
}
