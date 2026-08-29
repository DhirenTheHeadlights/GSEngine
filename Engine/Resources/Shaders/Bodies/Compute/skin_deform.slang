void skin_deform(uint3 dispatch_id) {
    uint vertex_index = dispatch_id.x;
    if (vertex_index >= pc.vertex_count) {
        return;
    }

    vertex source = bind_vertices[vertex_index];
    skin_influence influence = influence_data[vertex_index];

    float3 position = float3(0.0, 0.0, 0.0);
    float3 normal = float3(0.0, 0.0, 0.0);
    float total = 0.0;

    for (uint i = 0; i < 4; ++i) {
        float weight = influence.bone_weights[i];
        if (weight <= 0.0) {
            continue;
        }

        uint slot = influence.bone_slots[i];
        if (slot >= pc.bone_count) {
            continue;
        }

        skin_matrix entry = palette_data[pc.bone_offset + slot];
        if (entry.valid == 0) {
            continue;
        }

        position += mul(entry.skin, float4(source.position, 1.0)).xyz * weight;
        normal += mul((float3x3)entry.skin, source.normal) * weight;
        total += weight;
    }

    if (total <= 0.0) {
        deformed_vertices[pc.vertex_offset + vertex_index] = source;
        return;
    }

    vertex result = source;
    result.position = position / total;
    result.normal = normalize(normal);
    deformed_vertices[pc.vertex_offset + vertex_index] = result;
}
