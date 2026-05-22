void bloom_upsample(uint3 dispatch_id) {
    uint2 dst_size;
    bloom_up_out.GetDimensions(dst_size.x, dst_size.y);
    if (dispatch_id.x >= dst_size.x || dispatch_id.y >= dst_size.y) {
        return;
    }

    float r = pc.radius;
    bloom_up_out[dispatch_id.xy] = float4(r, 0.0, 0.0, 1.0);
}
