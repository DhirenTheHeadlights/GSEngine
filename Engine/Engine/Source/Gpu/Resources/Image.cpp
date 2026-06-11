module gse.gpu:image_impl;

import std;

import :image;
import :gpu_task;
import :sync_token;
import :device;

import gse.vulkan;

import gse.concurrency;
import gse.math;

auto gse::gpu::image_aspect_for(const image_format_value f) -> image_aspect_flags {
	return vulkan::image_aspect_for(f);
}

auto gse::gpu::format_value(const image_format f) -> image_format_value {
	return vulkan::format_value(f);
}

auto gse::gpu::transition_image_to(gpu::device& dev, image& img) -> sync_token {
	const auto aspect_flags = gpu::image_aspect_for(img.format());
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
		.discard_contents = true,
		.image = img.handle(),
		.aspects = aspect,
	};

	const dependency_info dep{
		.image_barriers = std::span(&barrier, 1)
	};
	vulkan::commands(cmd.handle()).pipeline_barrier(dep);

	return submit(dev, std::move(cmd), queue_id::graphics).submit_sync();
}
