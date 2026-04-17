import common;
import vbd_shared;
import vbd_physics;

[[vk::push_constant]]
ConstantBuffer<vbd_push_constants> pc;

[Layout(LayoutKind::VBDPhysics)]
[shader("compute")]
[numthreads(MAX_COLORS, 1, 1)]
void cs_main(uint3 dispatch_id : SV_DispatchThreadID) {
    uint color = dispatch_id.x;
    if (color >= MAX_COLORS) return;

    uint count = color_data[MAX_COLORS + color];
    indirect_args[2 + color].x = (count + 63) / 64;
    indirect_args[2 + color].y = 1;
    indirect_args[2 + color].z = 1;
}
