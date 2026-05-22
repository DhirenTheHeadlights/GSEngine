
void vbd_prepare_color_indirect(uint3 dispatch_id) {
    uint color = dispatch_id.x;
    if (color >= max_colors) return;

    uint count = color_data[max_colors + color];
    indirect_args[2 + color].x = (count + workgroup_size - 1) / workgroup_size;
    indirect_args[2 + color].y = 1;
    indirect_args[2 + color].z = 1;
}
