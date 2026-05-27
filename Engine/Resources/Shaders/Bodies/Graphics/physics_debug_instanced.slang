struct VS_OUTPUT {
    float4 clip_pos : SV_Position;
    float3 color    : COLOR;
};

[shader("vertex")]
VS_OUTPUT vs_main(uint vid : SV_VertexID, uint instance_id : SV_InstanceID) {
    float4 in_unit_pos = shape_vertex_buffer[vid];
    VS_OUTPUT output;

    shape_instance inst = shape_instances[instance_id];
    body_state body = body_data[inst.body_index];

    float radial_scale = inst.shape_scale.x;
    float axial_scale  = inst.shape_scale.y;
    float depth_scale  = inst.shape_scale.z;

    float3 local_pos = float3(
        in_unit_pos.x * radial_scale,
        in_unit_pos.y * axial_scale + in_unit_pos.z * radial_scale,
        in_unit_pos.w * depth_scale
    );

    float3 world_pos = body.position.xyz + rotate_vector(body.orientation, local_pos);

    float4 view_pos = mul(view, float4(world_pos, 1.0));
    output.clip_pos = mul(proj, view_pos);
    output.color = inst.color;

    return output;
}

[shader("fragment")]
float4 fs_main(VS_OUTPUT input) : SV_Target {
    return float4(input.color, 1.0);
}
