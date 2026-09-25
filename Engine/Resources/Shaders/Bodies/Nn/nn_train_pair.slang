void nn_train_pair(uint3 group_id, uint3 group_thread_id) {
    uint tile = group_id.x;
    uint j = group_thread_id.x;
    uint tiles = nn_train_tiles(pc.sample_count);
    if (tile < tiles) {
        nn_train_tile_net(nn_state, nn_scratch, tile, j, pc.out_dim, pc.param_count, 0, pc.target_offset);
        return;
    }
    tile -= tiles;
    if (tile >= tiles) {
        return;
    }
    nn_train_tile_net(nn_state_b, nn_scratch_b, tile, j, pc.out_dim_b, pc.param_count_b, 1, pc.target_offset_b);
}
