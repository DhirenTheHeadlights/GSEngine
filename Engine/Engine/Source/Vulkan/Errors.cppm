export module gse.vulkan:errors;

import vulkan;

export namespace gse::vulkan {
	using device_lost_error = vk::DeviceLostError;
	using out_of_date_error = vk::OutOfDateKHRError;
}
