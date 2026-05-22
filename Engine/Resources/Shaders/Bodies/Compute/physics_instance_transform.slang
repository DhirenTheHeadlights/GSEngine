
void physics_instance_transform(uint3 dispatch_id) {
    uint tid = dispatch_id.x;
    if (tid >= pc.mapping_count) {
        return;
    }

    physics_mapping m = mapping_data[tid];
    if (m.body_index >= pc.body_count) {
        return;
    }

    body_state body = body_data[m.body_index];

    float4x4 existing = instance_data_buffer[m.instance_index].model_matrix;
    float scale_x = length(float3(existing[0].x, existing[1].x, existing[2].x));
    float scale_y = length(float3(existing[0].y, existing[1].y, existing[2].y));
    float scale_z = length(float3(existing[0].z, existing[1].z, existing[2].z));

    float3 axis_x = rotate_vector(body.orientation, float3(1.0, 0.0, 0.0));
    float3 axis_y = rotate_vector(body.orientation, float3(0.0, 1.0, 0.0));
    float3 axis_z = rotate_vector(body.orientation, float3(0.0, 0.0, 1.0));
    float3 scaled_axis_x = axis_x * scale_x;
    float3 scaled_axis_y = axis_y * scale_y;
    float3 scaled_axis_z = axis_z * scale_z;
    float3 pivot = m.center_of_mass;
    float3 translation = body.position.xyz - scaled_axis_x * pivot.x - scaled_axis_y * pivot.y - scaled_axis_z * pivot.z;

    float4x4 model_matrix;
    model_matrix[0] = float4(scaled_axis_x.x, scaled_axis_y.x, scaled_axis_z.x, translation.x);
    model_matrix[1] = float4(scaled_axis_x.y, scaled_axis_y.y, scaled_axis_z.y, translation.y);
    model_matrix[2] = float4(scaled_axis_x.z, scaled_axis_y.z, scaled_axis_z.z, translation.z);
    model_matrix[3] = float4(0.0, 0.0, 0.0, 1.0);

    float4x4 normal_matrix;
    normal_matrix[0] = float4(axis_x.x, axis_y.x, axis_z.x, 0.0);
    normal_matrix[1] = float4(axis_x.y, axis_y.y, axis_z.y, 0.0);
    normal_matrix[2] = float4(axis_x.z, axis_y.z, axis_z.z, 0.0);
    normal_matrix[3] = float4(0.0, 0.0, 0.0, 1.0);

    instance_data_buffer[m.instance_index].model_matrix = model_matrix;
    instance_data_buffer[m.instance_index].normal_matrix = normal_matrix;
}
