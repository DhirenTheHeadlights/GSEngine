struct ShadowOutput {
    precise float4 clip_pos : SV_Position;
};

[shader("vertex")]
ShadowOutput vs_main(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID) {
    vertex v = vertices_buffer[vertex_id];
    instance_data inst = instance_data_buffer[pc.first_instance + instance_id];
    ShadowOutput o;
    o.clip_pos = mul(tiles[pc.tile_index].view_projection, mul(inst.model_matrix, float4(v.position, 1.0)));
    return o;
}

[shader("fragment")]
void fs_main(ShadowOutput input) {
}
