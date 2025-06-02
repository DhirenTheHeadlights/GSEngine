export module gse.platform.vulkan.resources;

import vulkan_hpp;

export namespace gse::vulkan::persistent_allocator {
	struct sub_allocation {
		vk::DeviceSize offset;
		vk::DeviceSize size;
		bool in_use = false;
	};

	struct allocation {
		vk::DeviceMemory memory;
		vk::DeviceSize size = 0, offset = 0;
		void* mapped = nullptr;
		sub_allocation* owner = nullptr;
	};

	struct image_resource {
		vk::Image image;
		vk::ImageView view;
		allocation allocation;
	};

	struct buffer_resource {
		vk::Buffer buffer;
		allocation allocation;
	};
}

export namespace gse::vulkan::transient_allocator {
	struct allocation {
		vk::DeviceMemory memory;
		vk::DeviceSize size = 0, offset = 0;
		void* mapped = nullptr;
	};
}