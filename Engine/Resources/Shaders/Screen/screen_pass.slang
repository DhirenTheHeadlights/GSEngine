static const float screen_depth_far = 0.0;
static const float screen_depth_near = 1.0;

struct screen_vs_out {
    float4 clip_pos : SV_Position;
    float2 uv       : TEXCOORD0;
};

float2 screen_ndc(float2 screen_uv) {
    return screen_uv * 2.0 - 1.0;
}

screen_vs_out screen_pass_vertex(uint vid, float clip_depth) {
    float2 p = float2((vid << 1) & 2, vid & 2);
    screen_vs_out o;
    o.uv = p;
    o.clip_pos = float4(screen_ndc(p), clip_depth, 1.0);
    return o;
}
