
uint pair_count = min(collision_pairs[0], MAX_COLLISION_PAIRS);
indirect_args[0].x = max((pair_count + 63) / 64, 1);
indirect_args[0].y = 1;
indirect_args[0].z = 1;
