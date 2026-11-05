[shader("vertex")]
CaptureOutput vs_main(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID) {
    uint views = pc.probe_count * 6u;
    vertex v = vertices_buffer[vertex_id];
    instance_data inst = instance_data_buffer[pc.first_instance + instance_id / views];
    float3 world = mul(inst.model_matrix, float4(v.position, 1.0)).xyz;
    return capture_vertex(world, instance_id % views, inst.material_index);
}

[shader("fragment")]
float4 fs_main(CaptureOutput input) : SV_Target0 {
    return shade_capture(input);
}
