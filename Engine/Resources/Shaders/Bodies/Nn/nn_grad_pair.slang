void nn_grad_pair(uint3 group_id, uint3 group_thread_id) {
    uint g = group_id.x;
    uint j = group_thread_id.x;
    uint groups_a = nn_grad_groups(pc.in_dim, pc.out_dim);
    if (g < groups_a) {
        nn_weight_grad_net(nn_state, nn_scratch, g, j, pc.out_dim, pc.param_count, 0, pc.adam_step);
        return;
    }
    nn_weight_grad_net(nn_state_b, nn_scratch_b, g - groups_a, j, pc.out_dim_b, pc.param_count_b, 1, pc.adam_step_b);
}
