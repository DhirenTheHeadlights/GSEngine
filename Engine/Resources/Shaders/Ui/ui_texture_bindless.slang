float4 ui_sample(uint tex_idx, float2 uv) {
    return textures[NonUniformResourceIndex(tex_idx)].Sample(textures_sampler, uv);
}
