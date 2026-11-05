
[shader("vertex")]
DepthOutput vs_main(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID) {
    vertex v = vertices_buffer[vertex_id];
    instance_data inst = instance_data_buffer[pc.first_instance + instance_id];
    return make_depth_output(v, vertex_id, inst);
}

[shader("fragment")]
float4 fs_main(DepthOutput input) : SV_Target0 {
    return float4(depth_velocity(input), 0.0, 0.0);
}
