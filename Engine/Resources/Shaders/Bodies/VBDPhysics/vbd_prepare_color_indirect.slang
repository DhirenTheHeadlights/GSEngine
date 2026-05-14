
uint color = dispatch_id.x;
if (color >= MAX_COLORS) return;

uint count = color_data[MAX_COLORS + color];
indirect_args[2 + color].x = (count + 63) / 64;
indirect_args[2 + color].y = 1;
indirect_args[2 + color].z = 1;
