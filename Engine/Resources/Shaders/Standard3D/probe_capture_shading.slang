struct CaptureOutput {
    precise float4 clip_pos : SV_Position;
    float3 tile_coord : TEXCOORD3;
    float3 world_pos : TEXCOORD0;
    nointerpolation uint view : TEXCOORD1;
    nointerpolation uint material_index : TEXCOORD2;
};

float3 capture_probe_position(uint slot) {
    return pc.origin_world + float3(probe_coord_of(pc.first_probe + slot, pc.grid_dim)) * pc.spacing;
}

CaptureOutput capture_vertex(float3 world, uint view, uint material_index) {
    uint slot = view / 6u;
    uint face = view % 6u;
    cube_face basis = cube_face_basis(face);
    float3 relative = world - capture_probe_position(slot);
    float x = dot(relative, basis.right);
    float y = -dot(relative, basis.up);
    float w = dot(relative, basis.forward);
    float columns = 6.0;
    float rows = float(pc.probe_count);

    CaptureOutput o;
    o.clip_pos = float4(
        ((2.0 * float(face) + 1.0) / columns - 1.0) * w + x / columns,
        ((2.0 * float(slot) + 1.0) / rows - 1.0) * w + y / rows,
        pc.near_plane,
        w
    );
    o.tile_coord = float3(x, y, w);
    o.world_pos = world;
    o.view = view;
    o.material_index = material_index;
    return o;
}

float4 shade_capture(CaptureOutput input) {
    if (abs(input.tile_coord.x) > input.tile_coord.z || abs(input.tile_coord.y) > input.tile_coord.z) {
        discard;
    }

    float3 probe_pos = capture_probe_position(input.view / 6u);
    float3 dir = normalize(input.world_pos - probe_pos);
    float3 N = -dir;
    material_data m = material_palette[input.material_index];

    float3 incident = float3(0.0, 0.0, 0.0);
    for (uint li = 0; li < pc.light_count; ++li) {
        light l = lights[li];
        LightSample ls = evaluate_light(l, l.world_position, l.world_direction, input.world_pos);
        float ndl = max(0.0, dot(N, ls.direction));
        if (ndl <= 0.0 || ls.attenuation <= 0.0) {
            continue;
        }
        incident += l.color * l.intensity * ls.attenuation * ndl;
    }

    float3 sky_radiance = sky_view_in.SampleLevel(sky_view_sampler, sky_view_dir_to_uv(dir), 0).rgb * atmosphere_ubo.sun_irradiance;
    float3 radiance = m.base_color * incident * (1.0 / pi) + m.base_color * sky_radiance * surface_sky_response;
    return float4(radiance, 1.0);
}
