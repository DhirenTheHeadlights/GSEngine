float3 material_diffuse(uint diffuse_index, float2 uv) {
    return textures[NonUniformResourceIndex(diffuse_index)].Sample(textures_sampler, uv).rgb;
}
