
// entry:
uint tid = dispatch_id.x;
if (tid >= pc.mapping_count) {
    return;
}

physics_mapping m = mapping_data[tid];
if (m.body_index >= pc.body_count) {
    return;
}

gpu_body body = body_data[m.body_index];

float3 axis_x = rotate_vector(body.orientation, float3(1.0, 0.0, 0.0));
float3 axis_y = rotate_vector(body.orientation, float3(0.0, 1.0, 0.0));
float3 axis_z = rotate_vector(body.orientation, float3(0.0, 0.0, 1.0));
float3 pivot = m.center_of_mass;
float3 translation = body.position.xyz - axis_x * pivot.x - axis_y * pivot.y - axis_z * pivot.z;

float4x4 model_matrix;
model_matrix[0] = float4(axis_x.x, axis_y.x, axis_z.x, translation.x);
model_matrix[1] = float4(axis_x.y, axis_y.y, axis_z.y, translation.y);
model_matrix[2] = float4(axis_x.z, axis_y.z, axis_z.z, translation.z);
model_matrix[3] = float4(0.0, 0.0, 0.0, 1.0);

float4x4 normal_matrix;
normal_matrix[0] = float4(axis_x.x, axis_y.x, axis_z.x, 0.0);
normal_matrix[1] = float4(axis_x.y, axis_y.y, axis_z.y, 0.0);
normal_matrix[2] = float4(axis_x.z, axis_y.z, axis_z.z, 0.0);
normal_matrix[3] = float4(0.0, 0.0, 0.0, 1.0);

instance_data_buffer[m.instance_index].model_matrix = model_matrix;
instance_data_buffer[m.instance_index].normal_matrix = normal_matrix;
