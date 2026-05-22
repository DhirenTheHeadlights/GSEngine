
void vbd_prepare_contact_indirect() {
    uint contact_count = min(collision_state[state_contact_count_index], max_contacts);
    indirect_args[1].x = max((contact_count + workgroup_size - 1) / workgroup_size, 1);
    indirect_args[1].y = 1;
    indirect_args[1].z = 1;
}
