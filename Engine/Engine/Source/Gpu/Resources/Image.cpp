module gse.gpu;

import std;

import gse.concurrency;
import gse.math;

namespace gse::gpu {
	auto upload_pre_barrier(
		handle<command_buffer> cmd,
		handle<vulkan::image> img,
		std::uint32_t layer_count
	) -> void;

	auto upload_post_barrier(
		handle<command_buffer> cmd,
		handle<vulkan::image> img,
		std::uint32_t layer_count
	) -> void;
}

auto gse::gpu::transition_image_to(gpu::device& dev, vulkan::image& img) -> sync_token {
	const auto fmt = static_cast<image_format>(img.format());
	const bool is_depth = fmt == image_format::d32_sfloat;
	const auto aspect = is_depth ? image_aspect_flag::depth : image_aspect_flag::color;

	auto cmd_awaiter = begin_transient(dev, queue_id::graphics, "transient.image_transition");
	auto cmd = cmd_awaiter.await_resume();

	const auto dst_stages = is_depth
		? (pipeline_stage_flag::early_fragment_tests | pipeline_stage_flag::late_fragment_tests)
		: pipeline_stage_flags{ pipeline_stage_flag::all_commands };
	const auto dst_access = is_depth
		? (access_flag::depth_stencil_attachment_write | access_flag::depth_stencil_attachment_read)
		: access_flags{ access_flag::shader_read };

	const image_barrier barrier{
		.src_stages = pipeline_stage_flag::top_of_pipe,
		.src_access = {},
		.dst_stages = dst_stages,
		.dst_access = dst_access,
		.old_layout = image_layout::undefined,
		.new_layout = image_layout::general,
		.image = img.handle(),
		.aspects = aspect,
	};

	const dependency_info dep{
		.image_barriers = std::span(&barrier, 1)
	};
	vulkan::commands(cmd.handle()).pipeline_barrier(dep);

	return submit(dev, std::move(cmd), queue_id::graphics).submit_sync();
}

auto gse::gpu::upload_image_2d(gpu::device& dev, vulkan::image& img, const void* pixel_data, const std::size_t data_size) -> sync_token {
	const auto extent3 = img.extent();
	const vec2u extent2{ extent3.x(), extent3.y() };

	if (dev.vulkan_device().host_image_copy_enabled()) {
		const void* ptrs[] = { pixel_data };
		vulkan::host_upload_image_layers(dev.vulkan_device(), img.handle(), ptrs, extent2);
		return {};
	}

	auto staging = dev.allocator().create_buffer(
		buffer_create_info{
			.size = data_size,
			.usage = buffer_flag::transfer_src,
		},
		pixel_data
	);

	auto cmd_awaiter = begin_transient(dev, queue_id::graphics, "transient.image_upload_2d");
	auto cmd = cmd_awaiter.await_resume();

	upload_pre_barrier(cmd.handle(), img.handle(), 1);

	const buffer_image_copy_region region{
		.buffer_offset = 0,
		.image_subresource = {
			.aspects = image_aspect_flag::color,
			.mip_level = 0,
			.base_array_layer = 0,
			.layer_count = 1,
		},
		.image_extent = vec3u{ extent3.x(), extent3.y(), 1 },
	};

	vulkan::commands(cmd.handle())
		.copy_buffer_to_image(staging.handle(), img.handle(), image_layout::general, std::span(&region, 1));

	upload_post_barrier(cmd.handle(), img.handle(), 1);

	return submit(dev, std::move(cmd), queue_id::graphics).retain(std::move(staging)).submit_sync();
}

auto gse::gpu::upload_image_layers(gpu::device& dev, vulkan::image& img, const std::vector<const void*>& face_data, const std::size_t bytes_per_face) -> sync_token {
	const auto extent3 = img.extent();
	const vec2u extent2{ extent3.x(), extent3.y() };
	const std::uint32_t layer_count = static_cast<std::uint32_t>(face_data.size());

	if (dev.vulkan_device().host_image_copy_enabled()) {
		vulkan::host_upload_image_layers(dev.vulkan_device(), img.handle(), face_data, extent2);
		return {};
	}

	const std::size_t total_size = layer_count * bytes_per_face;

	auto staging = dev.allocator().create_buffer(
		buffer_create_info{
			.size = total_size,
			.usage = buffer_flag::transfer_src,
		}
	);

	for (std::size_t i = 0; i < layer_count; ++i) {
		staging.host_write(face_data[i], bytes_per_face, i * bytes_per_face);
	}
	staging.clear_host_dirty();

	auto cmd_awaiter = begin_transient(dev, queue_id::graphics, "transient.image_upload_layers");
	auto cmd = cmd_awaiter.await_resume();

	upload_pre_barrier(cmd.handle(), img.handle(), layer_count);

	std::vector<buffer_image_copy_region> regions;
	regions.reserve(layer_count);
	for (std::uint32_t i = 0; i < layer_count; ++i) {
		regions.emplace_back(buffer_image_copy_region{
			.buffer_offset = i * bytes_per_face,
			.image_subresource = {
				.aspects = image_aspect_flag::color,
				.mip_level = 0,
				.base_array_layer = i,
				.layer_count = 1,
			},
			.image_extent = vec3u{ extent3.x(), extent3.y(), 1 },
		});
	}

	vulkan::commands(cmd.handle())
		.copy_buffer_to_image(staging.handle(), img.handle(), image_layout::general, regions);

	upload_post_barrier(cmd.handle(), img.handle(), layer_count);

	return submit(dev, std::move(cmd), queue_id::graphics).retain(std::move(staging)).submit_sync();
}

auto gse::transition_image_async(gpu::device& dev, gpu::handle<vulkan::image> img, gpu::image_aspect_flag aspect, std::uint32_t layers, bool is_depth) -> async::task<gpu::sync_token> {
	auto cmd = co_await begin_transient(dev, gpu::queue_id::graphics, "transient.image_transition");

	const auto dst_stages = is_depth
		? (gpu::pipeline_stage_flag::early_fragment_tests | gpu::pipeline_stage_flag::late_fragment_tests)
		: gpu::pipeline_stage_flags{ gpu::pipeline_stage_flag::all_commands };
	const auto dst_access = is_depth
		? (gpu::access_flag::depth_stencil_attachment_write | gpu::access_flag::depth_stencil_attachment_read)
		: gpu::access_flags{ gpu::access_flag::shader_read };

	const gpu::image_barrier barrier{
		.src_stages = gpu::pipeline_stage_flag::top_of_pipe,
		.src_access = {},
		.dst_stages = dst_stages,
		.dst_access = dst_access,
		.old_layout = gpu::image_layout::undefined,
		.new_layout = gpu::image_layout::general,
		.image = img,
		.aspects = aspect,
		.base_mip_level = 0,
		.level_count = 1,
		.base_array_layer = 0,
		.layer_count = layers,
	};

	const gpu::dependency_info dep{
		.image_barriers = std::span(&barrier, 1)
	};
	vulkan::commands(cmd.handle()).pipeline_barrier(dep);

	co_return co_await submit(dev, std::move(cmd), gpu::queue_id::graphics);
}

auto gse::upload_image_2d_async(gpu::device& dev, vulkan::image& resource, const void* pixel_data, std::size_t data_size, vec2u extent) -> async::task<gpu::sync_token> {
	if (dev.vulkan_device().host_image_copy_enabled()) {
		const void* ptrs[] = { pixel_data };
		vulkan::host_upload_image_layers(dev.vulkan_device(), resource.handle(), ptrs, extent);
		co_return gpu::sync_token{};
	}

	auto staging = dev.allocator().create_buffer(
		gpu::buffer_create_info{
			.size = data_size,
			.usage = gpu::buffer_flag::transfer_src,
		},
		pixel_data
	);

	auto cmd = co_await begin_transient(dev, gpu::queue_id::graphics, "transient.image_upload_2d");

	gpu::upload_pre_barrier(cmd.handle(), resource.handle(), 1);

	const gpu::buffer_image_copy_region region{
		.buffer_offset = 0,
		.image_subresource = {
			.aspects = gpu::image_aspect_flag::color,
			.mip_level = 0,
			.base_array_layer = 0,
			.layer_count = 1,
		},
		.image_extent = vec3u{ extent.x(), extent.y(), 1 },
	};

	vulkan::commands(cmd.handle())
		.copy_buffer_to_image(
			staging.handle(),
			resource.handle(),
			gpu::image_layout::general,
			std::span(&region, 1)
		);

	gpu::upload_post_barrier(cmd.handle(), resource.handle(), 1);

	co_return co_await submit(dev, std::move(cmd), gpu::queue_id::graphics).retain(std::move(staging));
}

auto gse::upload_image_layers_async(gpu::device& dev, vulkan::image& resource, std::vector<const void*> face_data, std::size_t bytes_per_face, vec2u extent) -> async::task<gpu::sync_token> {
	const std::uint32_t layer_count = static_cast<std::uint32_t>(face_data.size());

	if (dev.vulkan_device().host_image_copy_enabled()) {
		vulkan::host_upload_image_layers(dev.vulkan_device(), resource.handle(), face_data, extent);
		co_return gpu::sync_token{};
	}

	const std::size_t total_size = layer_count * bytes_per_face;

	auto staging = dev.allocator().create_buffer(
		gpu::buffer_create_info{
			.size = total_size,
			.usage = gpu::buffer_flag::transfer_src,
		}
	);

	for (std::size_t i = 0; i < layer_count; ++i) {
		staging.host_write(face_data[i], bytes_per_face, i * bytes_per_face);
	}
	staging.clear_host_dirty();

	auto cmd = co_await begin_transient(dev, gpu::queue_id::graphics, "transient.image_upload_layers");

	gpu::upload_pre_barrier(cmd.handle(), resource.handle(), layer_count);

	std::vector<gpu::buffer_image_copy_region> regions;
	regions.reserve(layer_count);
	for (std::uint32_t i = 0; i < layer_count; ++i) {
		regions.emplace_back(gpu::buffer_image_copy_region{
			.buffer_offset = i * bytes_per_face,
			.image_subresource = {
				.aspects = gpu::image_aspect_flag::color,
				.mip_level = 0,
				.base_array_layer = i,
				.layer_count = 1,
			},
			.image_extent = vec3u{ extent.x(), extent.y(), 1 },
		});
	}

	vulkan::commands(cmd.handle())
		.copy_buffer_to_image(staging.handle(), resource.handle(), gpu::image_layout::general, regions);

	gpu::upload_post_barrier(cmd.handle(), resource.handle(), layer_count);

	co_return co_await submit(dev, std::move(cmd), gpu::queue_id::graphics).retain(std::move(staging));
}

auto gse::gpu::upload_pre_barrier(const handle<command_buffer> cmd, const handle<vulkan::image> img, const std::uint32_t layer_count) -> void {
	const image_barrier barrier{
		.src_stages = pipeline_stage_flag::top_of_pipe,
		.src_access = {},
		.dst_stages = pipeline_stage_flag::transfer,
		.dst_access = access_flag::transfer_write,
		.old_layout = image_layout::undefined,
		.new_layout = image_layout::general,
		.image = img,
		.aspects = image_aspect_flag::color,
		.base_mip_level = 0,
		.level_count = 1,
		.base_array_layer = 0,
		.layer_count = layer_count,
	};
	vulkan::commands(cmd).pipeline_barrier(dependency_info{
		.image_barriers = std::span(&barrier, 1),
	});
}

auto gse::gpu::upload_post_barrier(const handle<command_buffer> cmd, const handle<vulkan::image> img, const std::uint32_t layer_count) -> void {
	const image_barrier barrier{
		.src_stages = pipeline_stage_flag::transfer,
		.src_access = access_flag::transfer_write,
		.dst_stages = pipeline_stage_flag::fragment_shader,
		.dst_access = access_flag::shader_read,
		.image = img,
		.aspects = image_aspect_flag::color,
		.base_mip_level = 0,
		.level_count = 1,
		.base_array_layer = 0,
		.layer_count = layer_count,
	};
	vulkan::commands(cmd).pipeline_barrier(dependency_info{
		.image_barriers = std::span(&barrier, 1),
	});
}
