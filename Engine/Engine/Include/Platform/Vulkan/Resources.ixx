export module gse.platform.vulkan:resources;

import std;
import vulkan_hpp;

import gse.assert;



export namespace gse::vulkan::transient_allocator {
	struct allocation {
		vk::DeviceMemory memory;
		vk::DeviceSize size = 0, offset = 0;
		void* mapped = nullptr;
	};
}