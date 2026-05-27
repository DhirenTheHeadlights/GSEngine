static const int transmittance_steps = 40;

float3 integrate_transmittance(float radius, float view_zenith_cos) {
    float t_max = ray_to_top_distance(radius, view_zenith_cos);
    float dt = t_max / float(transmittance_steps);
    float3 total_extinction = float3(0.0, 0.0, 0.0);

    for (int i = 0; i < transmittance_steps; ++i) {
        float t = dt * (float(i) + 0.5);
        float r_t = sqrt(t * t + 2.0 * radius * view_zenith_cos * t + radius * radius);
        medium_sample m = sample_medium(r_t);
        total_extinction += m.extinction * dt;
    }

    return exp(-total_extinction);
}

void atmosphere_transmittance(uint3 dispatch_id) {
    uint2 size;
    transmittance_out.GetDimensions(size.x, size.y);
    if (dispatch_id.x >= size.x || dispatch_id.y >= size.y) {
        return;
    }

    float2 uv = (float2(dispatch_id.xy) + 0.5) / float2(size);
    float radius;
    float view_zenith_cos;
    transmittance_uv_to_r_mu(uv, radius, view_zenith_cos);

    float3 transmittance = integrate_transmittance(radius, view_zenith_cos);
    transmittance_out[dispatch_id.xy] = float4(transmittance, 1.0);
}
