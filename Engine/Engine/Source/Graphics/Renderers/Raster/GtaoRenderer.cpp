module gse.graphics:gtao_renderer_impl;

import std;

import :camera_latch_renderer;
import :depth_prepass_renderer;
import :forward_renderer;
import :gtao_renderer;
import :lighting_source;
import :shared_shaders;

import gse.concurrency;
import gse.core;
import gse.ecs;
import gse.gpu;
import gse.gpu_record;
import gse.math;

namespace gse::renderer::gtao {
	struct occlusion_bindings {
		shaders::uniform_block<shaders::common::camera_data> camera_ubo;
		shaders::texture2d<vec4f> depth_in;
		shaders::storage_image<vec4f> occlusion_out;
	};

	struct blur_bindings {
		shaders::uniform_block<shaders::common::camera_data> camera_ubo;
		shaders::texture2d<vec4f> depth_in;
		shaders::texture2d<vec4f> occlusion_in;
		shaders::storage_image<vec4f> occlusion_out;
	};

	struct [[= shaders::shader_struct]]
	occlusion_push_constants {
		vec2u extent;
		std::int32_t ao_quality;
		length radius;
	};

	struct [[= shaders::shader_struct]]
	blur_push_constants {
		vec2u extent;
	};

	using occlusion_entry = gpu::compute_entry<
		gpu::body_path<"Compute/gtao_occlusion">,
		gpu::types<shaders::common::shader_types, shaders::forward::shader_types>,
		gpu::bindings<occlusion_bindings>,
		gpu::helpers<"Screen/depth_reconstruct", "Standard3D/lighting_shared">,
		gpu::threads<8, 8, 1>,
		gpu::push_constant<occlusion_push_constants>
	>;

	using blur_entry = gpu::compute_entry<
		gpu::body_path<"Compute/gtao_blur">,
		gpu::types<shaders::common::shader_types>,
		gpu::bindings<blur_bindings>,
		gpu::helpers<"Screen/depth_reconstruct">,
		gpu::threads<8, 8, 1>,
		gpu::push_constant<blur_push_constants>
	>;

	auto recreate_targets(
		shared_view<gpu::context::data> gpu_s,
		data& d
	) -> void;
}

auto gse::renderer::gtao::recreate_targets(const shared_view<gpu::context::data> gpu_s, data& d) -> void {
	const auto extent = gpu_s.render_graph->allocated_extent();
	const auto create = [&](const std::string_view tag) {
		auto image = gpu_s.device->create_image(
			{
				.size = extent,
				.format = gpu::image_format::r16g16b16a16_sfloat,
				.usage = { gpu::image_flag::storage, gpu::image_flag::sampled },
			},
			tag
		);
		gpu::transition_image_to(*gpu_s.device, image);
		return image;
	};
	d.raw_occlusion = create("gtao.raw");
	d.occlusion = create("gtao.occlusion");

	if (!d.depth_view.valid()) {
		d.depth_view = gpu_s.device->allocate_image_slot();
	}
	gpu_s.device->write_sampled_image(d.depth_view.slot(), gpu_s.render_graph->depth_image());
}

auto gse::renderer::gtao::init(context& ctx, const shared_view<gpu::context::data> gpu_s, data& d) -> async::task<> {
	{
		gpu::program_batch batch(*gpu_s.device);
		batch.add(d.occlusion_pipeline, occlusion_entry::pod);
		batch.add(d.blur_pipeline, blur_entry::pod);
	}

	recreate_targets(gpu_s, d);

	gpu_s.render_graph->on_resize(
		[gpu_s, &d]() {
			recreate_targets(gpu_s, d);
		}
	);

	return {};
}

auto gse::renderer::gtao::frame(context& ctx, shared_view<gpu::context::data> gpu_s, data& d, const channel_write<gpu::render_pass_request> pass_out, const channel_write<lighting_source::screen_ambient_occlusion> occlusion_out, shared_view<camera_latch::data> latch_s, shared_view<forward::data> forward_s) -> async::task<> {
	occlusion_out.push<lighting_source::screen_ambient_occlusion>({
		.occlusion = d.occlusion.sampled_slot(),
		.pass = gpu::pass_id<^^frame>(),
	});

	if (!gpu_s.render_graph->frame_in_progress() || forward_s.ao_quality == forward::ao_quality_level::off) {
		co_return;
	}

	const auto frame_index = gpu_s.render_graph->current_frame();
	const auto ext = gpu_s.render_graph->extent();
	const auto camera_slot = latch_s.camera_buffers[frame_index].slot();
	const length radius = meters(1.f);

	{
		auto rec = co_await gpu::pass<^^occlusion_pass>(pass_out)
			.pipeline(d.occlusion_pipeline)
			.after<^^camera_latch::frame, ^^depth_prepass::frame>();
		rec.dispatch<occlusion_entry>(
			{
				.extent = ext,
				.ao_quality = static_cast<std::int32_t>(forward_s.ao_quality),
				.radius = radius,
			},
			{
				.camera_ubo = camera_slot,
				.depth_in = d.depth_view,
				.occlusion_out = d.raw_occlusion,
			},
			gpu::group_count<occlusion_entry>(ext)
		);
	}

	auto rec = co_await gpu::pass<^^frame>(pass_out)
		.pipeline(d.blur_pipeline)
		.after<^^occlusion_pass>();
	rec.dispatch<blur_entry>(
		{
			.extent = ext,
		},
		{
			.camera_ubo = camera_slot,
			.depth_in = d.depth_view,
			.occlusion_in = d.raw_occlusion,
			.occlusion_out = d.occlusion,
		},
		gpu::group_count<blur_entry>(ext)
	);
}
