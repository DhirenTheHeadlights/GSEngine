
float4 back_rotate(float4 orientation, float3 angular_velocity, float lag) {
    float3 axis_angle = -angular_velocity * lag;
    float angle = length(axis_angle);
    if (angle < 1e-6) {
        return orientation;
    }
    float half_angle = angle * 0.5;
    float4 delta = float4(cos(half_angle), sin(half_angle) * axis_angle / angle);
    return quat_normalize(quat_mul(delta, orientation));
}

float4x4 compose_model_matrix(float3 position, float4 orientation, float3 scale_axes, float3 pivot) {
    float3 axis_x = rotate_vector(orientation, float3(1.0, 0.0, 0.0)) * scale_axes.x;
    float3 axis_y = rotate_vector(orientation, float3(0.0, 1.0, 0.0)) * scale_axes.y;
    float3 axis_z = rotate_vector(orientation, float3(0.0, 0.0, 1.0)) * scale_axes.z;
    float3 translation = position - axis_x * pivot.x - axis_y * pivot.y - axis_z * pivot.z;

    float4x4 model_matrix;
    model_matrix[0] = float4(axis_x.x, axis_y.x, axis_z.x, translation.x);
    model_matrix[1] = float4(axis_x.y, axis_y.y, axis_z.y, translation.y);
    model_matrix[2] = float4(axis_x.z, axis_y.z, axis_z.z, translation.z);
    model_matrix[3] = float4(0.0, 0.0, 0.0, 1.0);
    return model_matrix;
}

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
    float3 scale_axes = float3(
        length(float3(existing[0].x, existing[1].x, existing[2].x)),
        length(float3(existing[0].y, existing[1].y, existing[2].y)),
        length(float3(existing[0].z, existing[1].z, existing[2].z))
    );

    float3 pivot = m.center_of_mass;
    float3 velocity = body.velocity.xyz;
    float3 angular_velocity = body.angular_velocity.xyz;

    float3 curr_com = body.position.xyz - velocity * pc.interpolation_lag;
    float4 curr_orientation = back_rotate(body.orientation, angular_velocity, pc.interpolation_lag);
    float3 curr_position = body_origin(curr_com, curr_orientation, body.com_local.xyz);

    float3 prev_com = curr_com - velocity * pc.frame_delta;
    float4 prev_orientation = back_rotate(curr_orientation, angular_velocity, pc.frame_delta);
    float3 prev_position = body_origin(prev_com, prev_orientation, body.com_local.xyz);

    float3 axis_x = rotate_vector(curr_orientation, float3(1.0, 0.0, 0.0));
    float3 axis_y = rotate_vector(curr_orientation, float3(0.0, 1.0, 0.0));
    float3 axis_z = rotate_vector(curr_orientation, float3(0.0, 0.0, 1.0));

    float4x4 normal_matrix;
    normal_matrix[0] = float4(axis_x.x, axis_y.x, axis_z.x, 0.0);
    normal_matrix[1] = float4(axis_x.y, axis_y.y, axis_z.y, 0.0);
    normal_matrix[2] = float4(axis_x.z, axis_y.z, axis_z.z, 0.0);
    normal_matrix[3] = float4(0.0, 0.0, 0.0, 1.0);

    instance_data_buffer[m.instance_index].model_matrix = compose_model_matrix(curr_position, curr_orientation, scale_axes, pivot);
    instance_data_buffer[m.instance_index].normal_matrix = normal_matrix;
    instance_data_buffer[m.instance_index].prev_model_matrix = compose_model_matrix(prev_position, prev_orientation, scale_axes, pivot);
}
