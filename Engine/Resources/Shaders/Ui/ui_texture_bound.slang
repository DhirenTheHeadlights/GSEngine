float4 ui_sample(uint tex_idx, float2 uv) {
    if (tex_idx == pc.tex_idx) {
        return ui_texture.Sample(ui_texture_sampler, uv);
    }
    return ui_snapshot.Sample(ui_texture_sampler, uv);
}
