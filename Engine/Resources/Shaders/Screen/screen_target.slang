float2 screen_target_tap(float2 target_uv, float2 uv_max) {
    return min(target_uv, uv_max);
}

float2 screen_target_uv(float2 screen_uv, float2 uv_scale, float2 uv_max) {
    return screen_target_tap(screen_uv * uv_scale, uv_max);
}

bool screen_uv_inside(float2 screen_uv) {
    return all(screen_uv == saturate(screen_uv));
}
