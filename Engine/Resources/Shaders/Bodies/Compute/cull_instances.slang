static const uint cullable_count_byte_offset = 4;

bool is_aabb_in_frustum(float3 aabb_min, float3 aabb_max) {
    for (uint i = 0; i < 6; ++i) {
        float3 positive_vertex;
        positive_vertex.x = planes[i].x >= 0.0 ? aabb_max.x : aabb_min.x;
        positive_vertex.y = planes[i].y >= 0.0 ? aabb_max.y : aabb_min.y;
        positive_vertex.z = planes[i].z >= 0.0 ? aabb_max.z : aabb_min.z;

        float distance = dot(planes[i].xyz, positive_vertex) + planes[i].w;
        if (distance < 0.0) {
            return false;
        }
    }
    return true;
}

void cull_instances(uint3 group_id) {
    uint batch_idx = group_id.x;
    batch_info batch = batches[pc.batch_offset + batch_idx];

    bool batch_visible = true;
    if (batch.instance_count > 0) {
        batch_visible = is_aabb_in_frustum(batch.aabb_min, batch.aabb_max);
    }

    uint cullable_count = batch_visible ? batch.instance_count : 0;
    uint byte_offset = batch_idx * pc.indirect_stride + cullable_count_byte_offset;
    indirect_commands.Store(byte_offset, cullable_count);
}
