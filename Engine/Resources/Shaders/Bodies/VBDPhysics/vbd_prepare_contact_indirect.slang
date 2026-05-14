
uint contact_count = min(collision_state[0], MAX_CONTACTS);
indirect_args[1].x = max((contact_count + 63) / 64, 1);
indirect_args[1].y = 1;
indirect_args[1].z = 1;
