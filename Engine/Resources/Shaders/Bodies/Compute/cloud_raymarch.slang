static const int max_view_steps = 40;
static const int light_steps = 6;
static const float light_march_min_transmittance = 0.1;
static const float light_march_opaque_density = 3.0;
static const float mean_detail_erosion = 0.5;

float3 reconstruct_world_ray(float2 ndc) {
    float4 wn = mul(pc.inv_view_proj, float4(ndc, 0.0, 1.0));
    float4 wf = mul(pc.inv_view_proj, float4(ndc, 1.0, 1.0));
    float3 ro = wn.xyz / wn.w;
    float3 rt = wf.xyz / wf.w;
    return normalize(rt - ro);
}

float3 sample_atmosphere_transmittance(float radius, float view_zenith_cos) {
    float2 uv = transmittance_r_mu_to_uv(radius, view_zenith_cos);
    return transmittance_in.SampleLevel(transmittance_sampler, uv, 0).rgb;
}

float3 sample_ambient_sky(float3 dir) {
    float3 d = dir;
    if (d.y < 0.0) {
        d.y = 0.0;
        d = normalize(d);
    }
    float2 uv = sky_view_dir_to_uv(d);
    return sky_view_in.SampleLevel(sky_view_sampler_binding, uv, 0).rgb;
}

float sample_cloud_coverage(float3 world_pos_km, float height_fraction) {
    float3 shape_uvw = (world_pos_km + pc.wind_offset) * cloud_ubo.shape_scale;
    float shape = cloud_shape_in.SampleLevel(noise_sampler_binding, shape_uvw, 0).r;

    float gradient = cloud_altitude_gradient(height_fraction, cloud_ubo.cloud_type);
    return saturate(remap(shape * gradient, 1.0 - cloud_ubo.cloud_coverage, 1.0, 0.0, 1.0));
}

float carve_cloud_density(float base, float erosion) {
    float carved = base - erosion * cloud_ubo.detail_strength * (1.0 - base);
    return saturate(carved) * cloud_ubo.density_multiplier;
}

float sample_cloud_density(float3 world_pos_km, float height_fraction) {
    float base = sample_cloud_coverage(world_pos_km, height_fraction);

    if (base <= 0.0) {
        return 0.0;
    }

    float3 detail_uvw = (world_pos_km + pc.wind_offset * 1.7) * cloud_ubo.detail_scale;
    float detail = cloud_detail_in.SampleLevel(noise_sampler_binding, detail_uvw, 0).r;
    float erosion = lerp(detail, 1.0 - detail, saturate(height_fraction * 5.0));

    return carve_cloud_density(base, erosion);
}

float sample_cloud_density_shape_only(float3 world_pos_km, float height_fraction) {
    float base = sample_cloud_coverage(world_pos_km, height_fraction);

    if (base <= 0.0) {
        return 0.0;
    }

    return carve_cloud_density(base, mean_detail_erosion);
}

float sample_light_transmittance(float3 world_pos_km, float3 sun_dir, float layer_thickness_km) {
    float total_density = 0.0;
    float step_km = layer_thickness_km * 0.05;

    for (int i = 0; i < light_steps; ++i) {
        if (total_density * cloud_ubo.light_extinction > light_march_opaque_density) {
            break;
        }
        float t = step_km * (float(i) + 0.5);
        float3 sample_pos = world_pos_km + sun_dir * t;
        float sample_altitude_km = length(sample_pos) - atmosphere_ubo.bottom_radius;
        float h = saturate((sample_altitude_km - cloud_ubo.cloud_bottom) / max(layer_thickness_km, 1e-6));
        total_density += sample_cloud_density_shape_only(sample_pos, h) * step_km;
    }
    return exp(-total_density * cloud_ubo.light_extinction);
}

void cloud_raymarch(uint3 dispatch_id) {
    uint2 size;
    cloud_out.GetDimensions(size.x, size.y);
    if (dispatch_id.x >= size.x || dispatch_id.y >= size.y) {
        return;
    }

    float2 screen_uv = (float2(dispatch_id.xy) + 0.5) / float2(size);
    float2 ndc = screen_uv * 2.0 - 1.0;
    float3 view_dir = reconstruct_world_ray(ndc);

    if (view_dir.y < 0.01) {
        cloud_out[dispatch_id.xy] = float4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    float3 atm_camera = float3(0.0, atmosphere_ubo.bottom_radius + pc.camera_altitude, 0.0);
    float inner_radius = atmosphere_ubo.bottom_radius + cloud_ubo.cloud_bottom;
    float outer_radius = atmosphere_ubo.bottom_radius + cloud_ubo.cloud_top;
    float layer_thickness = cloud_ubo.cloud_top - cloud_ubo.cloud_bottom;

    float2 t_bottom = ray_layer_intersect(atm_camera, view_dir, inner_radius);
    float2 t_top = ray_layer_intersect(atm_camera, view_dir, outer_radius);

    float t_enter;
    float t_exit;
    if (pc.camera_altitude < cloud_ubo.cloud_bottom) {
        t_enter = t_bottom.y;
        t_exit = t_top.y;
    } else if (pc.camera_altitude > cloud_ubo.cloud_top) {
        t_enter = t_top.x;
        t_exit = t_bottom.x > 0.0 ? t_bottom.x : t_top.y;
    } else {
        t_enter = 0.0;
        t_exit = t_top.y > 0.0 ? t_top.y : t_bottom.y;
    }

    if (t_enter < 0.0 || t_exit <= t_enter) {
        cloud_out[dispatch_id.xy] = float4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    t_exit = min(t_exit, t_enter + cloud_ubo.max_distance);

    float dt = (t_exit - t_enter) / float(max_view_steps);

    float mu_view_sun = dot(view_dir, pc.sun_direction);
    float phase = dual_henyey_greenstein(mu_view_sun, cloud_ubo.phase_g_forward, cloud_ubo.phase_g_back, cloud_ubo.phase_blend);

    float view_zenith_cos = view_dir.y;
    float3 sun_transmittance_atm = sample_atmosphere_transmittance(atmosphere_ubo.bottom_radius + pc.camera_altitude, view_zenith_cos);
    float3 ambient_sky = sample_ambient_sky(float3(0.0, 1.0, 0.0)) * pc.sun_irradiance;

    float3 accumulated = float3(0.0, 0.0, 0.0);
    float transmittance = 1.0;

    float jitter = hash13(float3(dispatch_id.xy, pc.frame_index)) - 0.5;
    float t = t_enter + dt * jitter;
    float light_trans = 1.0;

    for (int i = 0; i < max_view_steps; ++i) {
        if (transmittance < 0.01) {
            break;
        }

        float3 sample_pos = atm_camera + view_dir * t;
        float sample_altitude_km = length(sample_pos) - atmosphere_ubo.bottom_radius;
        float h = saturate((sample_altitude_km - cloud_ubo.cloud_bottom) / max(layer_thickness, 1e-6));

        float density = sample_cloud_density(sample_pos, h);

        if (density > 0.0) {
            if (transmittance > light_march_min_transmittance) {
                light_trans = sample_light_transmittance(sample_pos, pc.sun_direction, layer_thickness);
            }

            float3 sun_contribution = pc.sun_irradiance * sun_transmittance_atm * phase * light_trans;
            float3 ambient_contribution = ambient_sky * cloud_ubo.ambient_strength * (0.3 + 0.7 * h);

            float3 luminance = sun_contribution + ambient_contribution;
            float bp = beer_powder(density, dt);

            accumulated += transmittance * luminance * density * dt * bp;
            transmittance *= exp(-density * dt * cloud_ubo.view_extinction);
        }

        t += dt;
        if (t >= t_exit) {
            break;
        }
    }

    cloud_out[dispatch_id.xy] = float4(accumulated, transmittance);
}
