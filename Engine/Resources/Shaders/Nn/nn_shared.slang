static const uint nn_grad_chunk = 32;

static const uint nn_r1_g = 0;
static const uint nn_r1_dz1 = nn_max_in;
static const uint nn_r1_zb1 = nn_r1_dz1 + nn_hidden;
static const uint nn_r1_dz2 = nn_r1_zb1 + nn_hidden;
static const uint nn_r1_da1_bar = nn_r1_dz2 + nn_hidden;
static const uint nn_r1_z2_bar = nn_r1_da1_bar + nn_hidden;
static const uint nn_r1_dz2_bar = nn_r1_z2_bar + nn_hidden;

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

float nn_mirror_sign(float code) {
    return code < 0.0 ? -1.0 : 1.0;
}

uint nn_mirror_source(float code) {
    return uint(abs(code)) - 1;
}

bool nn_update_applies() {
    return nn_scratch[nn_scalars_off + nn_scalar_apply] != 0.0;
}
