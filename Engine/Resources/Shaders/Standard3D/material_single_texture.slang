float3 material_diffuse(uint diffuse_index, float2 uv) {
    return material_diffuse_in.Sample(material_diffuse_sampler, uv).rgb;
}
