struct DepthOutput {
    precise float4 clip_pos : SV_Position;
    precise float4 curr_clip_pos : TEXCOORD0;
    precise float4 prev_clip_pos : TEXCOORD1;
};

float2 depth_velocity(DepthOutput input) {
    float2 curr_ndc = input.curr_clip_pos.xy / input.curr_clip_pos.w + camera_ubo.jitter_ndc;
    float2 prev_ndc = input.prev_clip_pos.xy / input.prev_clip_pos.w + camera_ubo.prev_jitter_ndc;
    return (curr_ndc - prev_ndc) * float2(0.5, -0.5);
}
