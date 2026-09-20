module gse.gpu:image_impl;

import std;

import :image;
import :gpu_task;
import :sync_token;
import :device;
import :command_contract;

import gse.concurrency;
import gse.math;

auto gse::gpu::transition_image_to(device& dev, image& img) -> sync_token {
	const auto aspect_flags = image_aspect_for(img.format());
	const bool is_depth = aspect_flags.test(image_aspect_flag::depth);
	const auto aspect = is_depth ? image_aspect_flag::depth : image_aspect_flag::color;

	auto cmd = begin_transient(dev, queue_id::graphics, "transient.image_transition");

	const auto dst_stages = is_depth
		? pipeline_stage_flags{ pipeline_stage_flag::early_fragment_tests, pipeline_stage_flag::late_fragment_tests }
		: pipeline_stage_flags{ pipeline_stage_flag::all_commands };
	const auto dst_access = is_depth
		? access_flags{ access_flag::depth_stencil_attachment_write, access_flag::depth_stencil_attachment_read }
		: access_flags{ access_flag::shader_read };

	const image_discard barrier{
		.src_stages = pipeline_stage_flag::top_of_pipe,
		.src_access = {},
		.dst_stages = dst_stages,
		.dst_access = dst_access,
		.image = img.handle(),
		.aspects = aspect,
	};

	const dependency_info dep{
		.image_discards = std::span(&barrier, 1)
	};
	dev.recorder(cmd.handle()).pipeline_barrier(dep);

	return submit(dev, std::move(cmd), queue_id::graphics).submit_sync();
}