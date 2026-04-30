module gse.gpu;

import std;

import gse.concurrency;
import gse.math;

auto gse::transition_image_async(gpu::device& dev, gpu::handle<vulkan::image> img, gpu::image_layout target_layout, gpu::image_aspect_flag aspect, std::uint32_t layers, bool is_depth) -> async::task<> {
	auto cmd = co_await begin_transient(dev, gpu::queue_id::graphics);

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
		.new_layout = target_layout,
		.image = img,
		.aspects = aspect,
		.base_mip_level = 0,
		.level_count = 1,
		.base_array_layer = 0,
		.layer_count = layers,
	};

	const gpu::dependency_info dep{ .image_barriers = std::span(&barrier, 1) };
	vulkan::commands(cmd.handle()).pipeline_barrier(dep);

	co_await submit(dev, std::move(cmd), gpu::queue_id::graphics);
}

auto gse::gpu::transition_image_to(gpu::device& dev, vulkan::image& img, const image_layout target_layout) -> void {
	if (target_layout == image_layout::undefined) {
		return;
	}

	const auto fmt = static_cast<image_format>(img.format());
	const bool is_depth = fmt == image_format::d32_sfloat;
	const auto aspect = is_depth ? image_aspect_flag::depth : image_aspect_flag::color;
	const auto handle = img.handle();

	dispatch(dev, transition_image_async(dev, handle, target_layout, aspect, 1u, is_depth));

	img.set_layout(target_layout);
}

auto gse::upload_image_2d_async(gpu::device& dev, vulkan::image& resource, const void* pixel_data, std::size_t data_size, vec2u extent) -> async::task<> {
	auto staging = dev.allocator().create_buffer(
		gpu::buffer_create_info{
			.size = data_size,
			.usage = gpu::buffer_flag::transfer_src,
		},
		pixel_data
	);

	auto cmd = co_await begin_transient(dev, gpu::queue_id::graphics);

	vulkan::transition_image_layout(
		resource,
		cmd.handle(),
		gpu::image_layout::transfer_dst,
		gpu::image_aspect_flag::color,
		gpu::pipeline_stage_flag::top_of_pipe,
		{},
		gpu::pipeline_stage_flag::transfer,
		gpu::access_flag::transfer_write
	);

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

	vulkan::commands(cmd.handle()).copy_buffer_to_image(
		staging.handle(),
		resource.handle(),
		gpu::image_layout::transfer_dst,
		std::span(&region, 1)
	);

	vulkan::transition_image_layout(
		resource,
		cmd.handle(),
		gpu::image_layout::shader_read_only,
		gpu::image_aspect_flag::color,
		gpu::pipeline_stage_flag::transfer,
		gpu::access_flag::transfer_write,
		gpu::pipeline_stage_flag::fragment_shader,
		gpu::access_flag::shader_read
	);

	co_await submit(dev, std::move(cmd), gpu::queue_id::graphics)
		.retain(std::move(staging));
}

auto gse::upload_image_layers_async(gpu::device& dev, vulkan::image& resource, std::vector<const void*> face_data, std::size_t bytes_per_face, vec2u extent) -> async::task<> {
	const std::uint32_t layer_count = static_cast<std::uint32_t>(face_data.size());
	const std::size_t total_size = layer_count * bytes_per_face;

	auto staging = dev.allocator().create_buffer(
		gpu::buffer_create_info{
			.size = total_size,
			.usage = gpu::buffer_flag::transfer_src,
		}
	);

	const auto mapped = staging.mapped();
	for (std::size_t i = 0; i < layer_count; ++i) {
		memcpy(mapped + i * bytes_per_face, face_data[i], bytes_per_face);
	}

	auto cmd = co_await begin_transient(dev, gpu::queue_id::graphics);

	vulkan::transition_image_layout(
		resource,
		cmd.handle(),
		gpu::image_layout::transfer_dst,
		gpu::image_aspect_flag::color,
		gpu::pipeline_stage_flag::top_of_pipe,
		{},
		gpu::pipeline_stage_flag::transfer,
		gpu::access_flag::transfer_write,
		1,
		layer_count
	);

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

	vulkan::commands(cmd.handle()).copy_buffer_to_image(
		staging.handle(),
		resource.handle(),
		gpu::image_layout::transfer_dst,
		regions
	);

	vulkan::transition_image_layout(
		resource,
		cmd.handle(),
		gpu::image_layout::shader_read_only,
		gpu::image_aspect_flag::color,
		gpu::pipeline_stage_flag::transfer,
		gpu::access_flag::transfer_write,
		gpu::pipeline_stage_flag::fragment_shader,
		gpu::access_flag::shader_read,
		1,
		layer_count
	);

	co_await submit(dev, std::move(cmd), gpu::queue_id::graphics)
		.retain(std::move(staging));
}

auto gse::gpu::upload_image_2d(gpu::device& dev, vulkan::image& img, const void* pixel_data, const std::size_t data_size) -> void {
	const auto extent3 = img.extent();
	dispatch(dev, upload_image_2d_async(dev, img, pixel_data, data_size, vec2u{ extent3.x(), extent3.y() }));
	img.set_layout(image_layout::shader_read_only);
}

auto gse::gpu::upload_image_layers(gpu::device& dev, vulkan::image& img, const std::vector<const void*>& face_data, const std::size_t bytes_per_face) -> void {
	const auto extent3 = img.extent();
	dispatch(dev, upload_image_layers_async(dev, img, face_data, bytes_per_face, vec2u{ extent3.x(), extent3.y() }));
	img.set_layout(image_layout::shader_read_only);
}
