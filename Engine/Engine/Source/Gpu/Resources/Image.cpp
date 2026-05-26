module gse.gpu;

import std;

import gse.concurrency;
import gse.math;

auto gse::gpu::transition_image_to(gpu::device& dev, image& img) -> sync_token {
	const auto aspect_flags = vulkan::image_aspect_for(img.format());
	const bool is_depth = aspect_flags.test(image_aspect_flag::depth);
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
	commands(cmd.handle()).pipeline_barrier(dep);

	return submit(dev, std::move(cmd), queue_id::graphics).submit_sync();
}

auto gse::gpu::upload_image_2d(gpu::device& dev, image& img, const void* pixel_data) -> sync_token {
	const auto extent3 = img.extent();
	const vec2u extent2{ extent3.x(), extent3.y() };
	const void* ptrs[] = { pixel_data };
	vulkan::host_upload_image_layers(dev.vulkan_device(), img.handle(), ptrs, extent2);
	return {};
}
