static const uint latch_model_none = 0u;
static const uint latch_model_orbit = 2u;

float3x3 latch_basis(float yaw, float pitch) {
    float cy = cos(yaw);
    float sy = sin(yaw);
    float cp = cos(pitch);
    float sp = sin(pitch);
    return float3x3(
        float3(cy, 0.0, -sy),
        float3(sy * sp, cp, cy * sp),
        float3(sy * cp, -sp, cy * cp)
    );
}

bool same_view(float4x4 a, float4x4 b) {
    return all(a[0] == b[0]) && all(a[1] == b[1]) && all(a[2] == b[2]) && all(a[3] == b[3]);
}

camera_data latched_camera(camera_data cam) {
    int dx = int(mouse_totals[0] - pc.consumed_x);
    int dy = int(mouse_totals[1] - pc.consumed_y);
    if (pc.model == latch_model_none || (dx == 0 && dy == 0)) {
        return cam;
    }

    float yaw = pc.yaw + float(dx) * pc.per_count.x;
    float pitch = clamp(pc.pitch + float(dy) * pc.per_count.y, pc.min_pitch, pc.max_pitch);

    float3x3 base = latch_basis(pc.yaw, pc.pitch);
    float3x3 latched = latch_basis(yaw, pitch);

    float3 eye = mul(cam.inv_view, float4(0.0, 0.0, 0.0, 1.0)).xyz;
    if (pc.model == latch_model_orbit) {
        eye += (latched[2] - base[2]) * pc.distance;
    }

    float3 right = latched[0];
    float3 up = latched[1];
    float3 back = latched[2];

    float4x4 view = float4x4(
        float4(right, -dot(right, eye)),
        float4(up, -dot(up, eye)),
        float4(back, -dot(back, eye)),
        float4(0.0, 0.0, 0.0, 1.0)
    );
    float4x4 inv_view = float4x4(
        float4(right.x, up.x, back.x, eye.x),
        float4(right.y, up.y, back.y, eye.y),
        float4(right.z, up.z, back.z, eye.z),
        float4(0.0, 0.0, 0.0, 1.0)
    );

    float4x4 inv_proj = mul(cam.view, cam.inv_view_proj);
    cam.inv_view_proj = mul(inv_view, inv_proj);
    cam.view = view;
    cam.inv_view = inv_view;
    return cam;
}

void camera_latch(uint3 dispatch_id) {
    camera_data cpu = camera_in;
    camera_data cam = latched_camera(cpu);

    latch_history last = history[0];
    if (same_view(last.cpu_view, cpu.prev_view) || same_view(last.cpu_view, cpu.view)) {
        cam.prev_view = last.latched_view;
    }

    latch_history next;
    next.cpu_view = cpu.view;
    next.latched_view = cam.view;
    history[0] = next;

    camera_out[0] = cam;
}
