static const float3 light_cone_kernel[6] = {
    float3(0.38051305, 0.92453449, -0.02111345),
    float3(-0.50625799, -0.03590792, -0.86163418),
    float3(-0.32509218, -0.94557439, 0.01428793),
    float3(0.09026238, -0.27376545, 0.95755165),
    float3(0.28128598, 0.42443639, -0.86065785),
    float3(-0.16852403, 0.14748697, 0.97460106)
};

float3 reconstruct_world_ray(float2 ndc) {
    float4 wn = mul(cloud_view_ubo.inv_view_proj, float4(ndc, 1.0, 1.0));
    float4 wf = mul(cloud_view_ubo.inv_view_proj, float4(ndc, 0.0, 1.0));
    return normalize(wf.xyz / wf.w - wn.xyz / wn.w);
}

float3 sample_atmosphere_transmittance(float radius, float view_zenith_cos) {
    if (ray_occluded_by_ground(radius, view_zenith_cos)) {
        return float3(0.0, 0.0, 0.0);
    }
    return transmittance_in.SampleLevel(transmittance_sampler, transmittance_r_mu_to_uv(radius, view_zenith_cos), 0).rgb;
}

float3 sample_sky(float3 dir) {
    return sky_view_in.SampleLevel(sky_view_sampler_binding, sky_view_dir_to_uv(dir), 0).rgb;
}

bool scene_occludes_footprint(float2 screen_uv, float2 texel) {
    float nearest = 1.0;
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            float2 tap = screen_uv + (float2(x, y) - 1.5) * texel;
            nearest = min(nearest, scene_depth_in.SampleLevel(scene_depth_sampler, screen_target_uv(tap, pc.depth_uv_scale, pc.depth_uv_max), 0).r);
        }
    }
    return nearest > scene_background_depth;
}

static const float3x3 detail_rotation = {
    float3(0.79335334, -0.36324140, 0.48860985),
    float3(0.51541850, 0.81975850, -0.24974780),
    float3(-0.32982770, 0.44274920, 0.83376350)
};

float sample_cloud_full(float3 world_pos_km, float height_fraction, float base, float ray_distance, float step_length) {
    float erosion = cloud_ubo.detail_erosion * (1.0 - smoothstep(detail_fade_start, detail_fade_end, ray_distance));
    if (erosion <= 0.0) {
        return base;
    }
    float3 detail_uvw = mul(detail_rotation, (world_pos_km + pc.wind_offset * 1.7) * cloud_ubo.detail_scale);
    float3 detail = cloud_detail_in.SampleLevel(noise_sampler_binding, detail_uvw, 0).rgb;
    return cloud_erode(base, cloud_detail_resolvable(detail, cloud_ubo.detail_scale, step_length), height_fraction, erosion);
}

float light_optical_depth(float3 world_pos_km, float3 sun_dir, float layer_thickness_km, int samples) {
    float total = layer_thickness_km;
    float normaliser = 1.0 / (exp2(float(samples)) - 1.0);
    float optical_depth = 0.0;
    float previous = 0.0;

    for (int i = 0; i < samples; ++i) {
        float boundary = total * (exp2(float(i + 1)) - 1.0) * normaliser;
        float segment = boundary - previous;
        float along = previous + segment * 0.5;
        previous = boundary;

        float3 sample_pos = world_pos_km + sun_dir * along + light_cone_kernel[i] * (along * light_cone_spread);
        float h = cloud_height_fraction(sample_pos);
        optical_depth += sample_cloud_base(sample_pos, h, pc.wind_offset) * segment;
    }

    float3 long_pos = world_pos_km + sun_dir * (total * light_long_sample_scale);
    optical_depth += sample_cloud_base(long_pos, cloud_height_fraction(long_pos), pc.wind_offset) * total;

    return optical_depth * cloud_ubo.extinction;
}

void cloud_raymarch(uint3 dispatch_id) {
    uint stride = pc.interleave_stride;
    uint phase = (pc.interleave_offset.x + pc.interleave_offset.y * stride + dispatch_id.z) % (stride * stride);
    uint2 pixel = dispatch_id.xy * stride + uint2(phase % stride, phase / stride);

    uint2 size = max(uint2(pc.full_extent) / pc.target_divisor, uint2(1, 1));
    if (pixel.x >= size.x || pixel.y >= size.y) {
        return;
    }

    float2 screen_uv = (float2(pixel) + pc.subpixel_offset) / float2(size);
    float3 view_dir = reconstruct_world_ray(screen_uv * 2.0 - 1.0);

    if (view_dir.y < 0.01) {
        cloud_out[pixel] = float4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    if (dispatch_id.z != 0) {
        float4 previous_clip = mul(cloud_view_ubo.prev_view_proj, float4(view_dir, 0.0));
        float2 previous_uv = (previous_clip.xy / previous_clip.w) * 0.5 + 0.5;
        if (previous_clip.w > 1e-6 && screen_uv_inside(previous_uv)) {
            cloud_out[pixel] = cloud_history_in.SampleLevel(cloud_history_sampler, screen_target_uv(previous_uv + (0.5 - cloud_view_ubo.history_subpixel_offset) / float2(size), pc.low_uv_scale, pc.low_uv_max), 0);
            return;
        }
    }

    if (scene_occludes_footprint(screen_uv, 1.0 / float2(size))) {
        cloud_out[pixel] = float4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    float3 camera_pos = float3(0.0, atmosphere_ubo.bottom_radius + pc.camera_altitude, 0.0);
    float layer_thickness = cloud_ubo.cloud_top - cloud_ubo.cloud_bottom;

    float2 t_bottom = ray_layer_intersect(camera_pos, view_dir, atmosphere_ubo.bottom_radius + cloud_ubo.cloud_bottom);
    float2 t_top = ray_layer_intersect(camera_pos, view_dir, atmosphere_ubo.bottom_radius + cloud_ubo.cloud_top);

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
        cloud_out[pixel] = float4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    t_exit = min(t_exit, t_enter + cloud_ubo.max_distance);

    float cos_theta = dot(view_dir, pc.sun_direction);
    float3 sun_radiance = pc.sun_irradiance * sample_atmosphere_transmittance(atmosphere_ubo.bottom_radius + pc.camera_altitude, pc.sun_direction.y);
    float3 sky_zenith = sample_sky(float3(0.0, 1.0, 0.0)) * pc.sun_irradiance;
    float2 horizontal = view_dir.xz / max(length(view_dir.xz), 1e-4);
    float3 sky_horizon = sample_sky(float3(horizontal.x, 0.0, horizontal.y)) * pc.sun_irradiance;

    float jitter = interleaved_gradient_noise(float2(pixel), pc.frame_index);
    float t = t_enter + coarse_step * jitter;

    float3 radiance = float3(0.0, 0.0, 0.0);
    float transmittance = 1.0;
    float light_od = 0.0;
    bool coarse = true;
    int empty_steps = 0;
    int density_steps = 0;

    for (int i = 0; i < max_iterations; ++i) {
        if (t >= t_exit || transmittance < view_march_min_transmittance) {
            break;
        }

        float step_scale = 1.0 + t / step_growth_distance;
        float3 sample_pos = camera_pos + view_dir * t;
        float h = cloud_height_fraction(sample_pos);
        float base = sample_cloud_base(sample_pos, h, pc.wind_offset);

        if (coarse) {
            if (base > 0.0) {
                coarse = false;
                empty_steps = 0;
                t = max(t_enter, t - coarse_step * step_scale);
            } else {
                t += coarse_step * step_scale;
            }
            continue;
        }

        float dt = fine_step * step_scale * lerp(thin_step_scale, 1.0, saturate(base / thin_density));
        float density = base > 0.0 ? sample_cloud_full(sample_pos, h, base, t, dt) : 0.0;

        if (density > 0.0) {
            empty_steps = 0;
            float sigma_t = density * cloud_ubo.extinction;
            float light_lod = saturate(t / light_lod_distance);
            int light_reuse = light_lod >= 1.0 ? 2 : 1;

            if (transmittance > light_march_min_transmittance && density_steps % light_reuse == 0) {
                int samples = int(round(lerp(float(light_cone_samples), float(light_far_cone_samples), light_lod)));
                light_od = light_optical_depth(sample_pos, pc.sun_direction, layer_thickness, samples);
            }
            ++density_steps;

            float scatter = cloud_multi_scatter(light_od, cos_theta, cloud_ubo.phase_g_forward, cloud_ubo.phase_g_back, cloud_ubo.phase_blend, cloud_ubo.multi_scatter_strength, cloud_ubo.multi_scatter_extinction, multi_scatter_octaves);
            float powder = cloud_powder(sigma_t * powder_depth, cloud_ubo.powder_strength);
            float ambient_occlusion = exp(-sigma_t * ambient_depth * (1.0 - h));
            float3 ambient = lerp(sky_horizon, sky_zenith, h) * (cloud_ubo.ambient_strength * ambient_occlusion);

            float3 source = cloud_ubo.albedo * sigma_t * (sun_radiance * scatter * powder + ambient);
            float step_transmittance = exp(-sigma_t * dt);
            radiance += transmittance * (source - source * step_transmittance) / sigma_t;
            transmittance *= step_transmittance;
        } else if (++empty_steps >= empty_steps_to_coarse) {
            coarse = true;
        }

        t += dt;
    }

    cloud_out[pixel] = float4(radiance, transmittance);
}
