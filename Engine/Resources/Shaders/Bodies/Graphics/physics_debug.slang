struct VS_OUTPUT {
    float4 clip_pos : SV_Position;
    float3 color    : COLOR;
};

[shader("vertex")]
VS_OUTPUT vs_main(uint vid : SV_VertexID) {
    debug_vertex v = line_vertex_buffer[vid];
    VS_OUTPUT output;

    float4 world_pos = float4(v.position, 1.0);
    float4 view_pos  = mul(view, world_pos);
    output.clip_pos  = mul(proj, view_pos);
    output.color     = v.color;

    return output;
}

[shader("fragment")]
float4 fs_main(VS_OUTPUT input) : SV_Target {
    return float4(input.color, 1.0);
}
