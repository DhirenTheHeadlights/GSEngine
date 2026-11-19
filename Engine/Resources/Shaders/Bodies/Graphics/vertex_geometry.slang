
[shader("vertex")]
MeshOutput vs_main(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID) {
    uint instance_idx = pc.first_instance + instance_id;
    vertex v = vertices_buffer[vertex_id];
    instance_data inst = instance_data_buffer[instance_idx];

    MeshVertexOutput mv = compute_mesh_vertex(v, inst, camera_ubo.view, camera_ubo.proj);
    return make_mesh_output(mv, instance_idx);
}

[shader("fragment")]
float4 fs_main(MeshOutput input) : SV_Target0 {
    return shade_surface(input);
}
