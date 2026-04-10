import common;
import vbd_shared;
import vbd_physics;

[[vk::push_constant]]
ConstantBuffer<vbd_push_constants> pc;

[Layout(LayoutKind::VBDPhysics)]
[shader("compute")]
[numthreads(1, 1, 1)]
void cs_main() {
    uint contact_count = min(collision_state[0], MAX_CONTACTS);
    indirect_args[1].x = max((contact_count + 63) / 64, 1);
    indirect_args[1].y = 1;
    indirect_args[1].z = 1;
}
