export module gse.gpu:image;

import std;

import :handles;
import :gpu_task;
import :transient_api;
import :types;
import :vulkan_device;
import :vulkan_image;
import :vulkan_commands;
import :device;

import gse.assert;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.math;

export namespace gse::gpu {
	auto transition_image_to(
		gpu::device& dev,
		vulkan::image& img,
		image_layout target
	) -> void;

	auto upload_image_2d(
		gpu::device& dev,
		vulkan::image& img,
		const void* pixel_data,
		std::size_t data_size
	) -> void;

	auto upload_image_layers(
		gpu::device& dev,
		vulkan::image& img,
		const std::vector<const void*>& face_data,
		std::size_t bytes_per_face
	) -> void;
}

namespace gse {
	auto transition_image_async(
		gpu::device& dev,
		gpu::handle<vulkan::image> img,
		gpu::image_layout target_layout,
		gpu::image_aspect_flag aspect,
		std::uint32_t layers,
		bool is_depth
	) -> async::task<>;

	auto upload_image_2d_async(
		gpu::device& dev,
		vulkan::image& resource,
		const void* pixel_data,
		std::size_t data_size,
		vec2u extent
	) -> async::task<>;

	auto upload_image_layers_async(
		gpu::device& dev,
		vulkan::image& resource,
		std::vector<const void*> face_data,
		std::size_t bytes_per_face,
		vec2u extent
	) -> async::task<>;
}
