void nn_smoke(uint3 dispatch_id) {
    uint i = dispatch_id.x;
    if (i >= pc.count) {
        return;
    }
    smoke_out[i] = i * 2654435761u + pc.generation;
}
