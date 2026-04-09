import common;
import vbd_shared;
import vbd_physics;

[[vk::push_constant]]
ConstantBuffer<vbd_push_constants> pc;

[Layout(LayoutKind::VBDPhysics)]
[shader("compute")]
[numthreads(64, 1, 1)]
void cs_main(uint3 dispatch_id : SV_DispatchThreadID) {
    uint bi = dispatch_id.x;

    if (bi == 0) {
        collision_pairs[0] = 0;
        collision_state[0] = 0;
        collision_state[1] = 0;
        collision_state[2] = 0;
        collision_state[3] = 0;
        collision_state[4] = 0;
        collision_state[5] = 0;
        collision_state[6] = 0;
    }

    if (bi < pc.body_count) {
        body_contact_map[bi] = 0;
        body_contact_map[MAX_BODIES + bi] = 0;
    }

    if (bi < MAX_CONTACTS) {
        contact_data[bi].body_a = 0;
        contact_data[bi].body_b = 0;
    }
}
