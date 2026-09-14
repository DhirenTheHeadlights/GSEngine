static const uint nn_hidden = 128;
static const uint nn_max_in = 64;
static const uint nn_max_out = 16;
static const uint nn_max_samples = 65536;
static const uint nn_max_minibatch = 512;
static const uint nn_max_params = nn_max_in * nn_hidden + nn_hidden + nn_hidden * nn_hidden + nn_hidden + nn_max_out * nn_hidden + nn_max_out + nn_max_out;
static const uint nn_state_extra = 4;
static const uint nn_reduce_width = 256;
static const uint nn_grad_chunk = 32;

static const uint nn_h1_off = 0;
static const uint nn_h2_off = nn_h1_off + nn_max_samples * nn_hidden;
static const uint nn_out_off = nn_h2_off + nn_max_samples * nn_hidden;
static const uint nn_out_delta_off = nn_out_off + nn_max_samples * nn_max_out;
static const uint nn_h1_pre_off = nn_out_delta_off + nn_max_minibatch * nn_max_out;
static const uint nn_h2_pre_off = nn_h1_pre_off + nn_max_minibatch * nn_hidden;
static const uint nn_ls_terms_off = nn_h2_pre_off + nn_max_minibatch * nn_hidden;
static const uint nn_sample_loss_off = nn_ls_terms_off + nn_max_minibatch * nn_max_out;
static const uint nn_grads_off = nn_sample_loss_off + nn_max_samples;
static const uint nn_scalars_off = nn_grads_off + nn_max_params;
static const uint nn_r1_off = nn_scalars_off + 16;
static const uint nn_r1_stride = nn_max_in + 6 * nn_hidden;
static const uint nn_r1_penalty_off = nn_r1_off + nn_max_minibatch * nn_r1_stride;
static const uint nn_scratch_floats = nn_r1_penalty_off + nn_max_minibatch;

static const uint nn_r1_g = 0;
static const uint nn_r1_dz1 = nn_max_in;
static const uint nn_r1_zb1 = nn_r1_dz1 + nn_hidden;
static const uint nn_r1_dz2 = nn_r1_zb1 + nn_hidden;
static const uint nn_r1_da1_bar = nn_r1_dz2 + nn_hidden;
static const uint nn_r1_z2_bar = nn_r1_da1_bar + nn_hidden;
static const uint nn_r1_dz2_bar = nn_r1_z2_bar + nn_hidden;

static const uint nn_scalar_apply = 3;
static const uint nn_scalar_bc1 = 4;
static const uint nn_scalar_bc2 = 5;
static const uint nn_scalar_mean_real = 6;
static const uint nn_scalar_mean_fake = 7;
static const uint nn_scalar_r1 = 8;
static const uint nn_scalar_mean_style = 9;
static const uint nn_scalar_adv_mean = 10;
static const uint nn_scalar_adv_std = 11;
static const uint nn_scalar_loss_acc = 12;

static const float nn_log_2pi = 1.8378770664093453;
static const float nn_gaussian_entropy_const = 1.4189385332046727;
static const float nn_adam_beta1 = 0.9;
static const float nn_adam_beta2 = 0.999;
static const float nn_adam_eps = 1e-8;

uint nn_w1_off(uint in_dim) {
    return 0;
}

uint nn_b1_off(uint in_dim) {
    return nn_hidden * in_dim;
}

uint nn_w2_off(uint in_dim) {
    return nn_b1_off(in_dim) + nn_hidden;
}

uint nn_b2_off(uint in_dim) {
    return nn_w2_off(in_dim) + nn_hidden * nn_hidden;
}

uint nn_w3_off(uint in_dim) {
    return nn_b2_off(in_dim) + nn_hidden;
}

uint nn_b3_off(uint in_dim, uint out_dim) {
    return nn_w3_off(in_dim) + out_dim * nn_hidden;
}

uint nn_ls_off(uint in_dim, uint out_dim) {
    return nn_b3_off(in_dim, out_dim) + out_dim;
}

uint nn_step_off(uint param_count) {
    return 3 * param_count;
}

uint nn_w1t_off(uint param_count) {
    return 3 * param_count + nn_state_extra;
}

uint nn_w2t_off(uint param_count, uint in_dim) {
    return nn_w1t_off(param_count) + nn_hidden * in_dim;
}

uint nn_w3t_off(uint param_count, uint in_dim) {
    return nn_w2t_off(param_count, in_dim) + nn_hidden * nn_hidden;
}

bool nn_update_applies() {
    return nn_scratch[nn_scalars_off + nn_scalar_apply] != 0.0;
}
