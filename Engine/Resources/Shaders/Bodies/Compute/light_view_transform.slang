void light_view_transform(uint3 dispatch_id) {
    uint index = dispatch_id.x;
    if (index >= pc.light_count) {
        return;
    }

    light l = lights[index];
    l.position = mul(camera_ubo.view, float4(l.world_position, 1.0)).xyz;
    l.direction = mul((float3x3)camera_ubo.view, l.world_direction);
    lights[index] = l;
}
