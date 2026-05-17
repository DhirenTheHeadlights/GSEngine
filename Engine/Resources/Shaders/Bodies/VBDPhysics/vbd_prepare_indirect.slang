
uint pair_count = min(collision_pairs[0], max_collision_pairs);
indirect_args[0].x = max((pair_count + workgroup_size - 1) / workgroup_size, 1);
indirect_args[0].y = 1;
indirect_args[0].z = 1;
