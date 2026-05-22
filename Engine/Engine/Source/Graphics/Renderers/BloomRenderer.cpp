module gse.graphics;

import std;

import :bloom_renderer;
import :atmosphere_renderer;
import :forward_renderer;
import :physics_debug_renderer;
import :render_targets;
import :sdf_grid_renderer;
import :world_text_renderer;

import gse.gpu;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.math;
import gse.meta;

namespace gse::renderer::bloom {
	struct [[
		= shaders::binding<0, 0>{},
		= shaders::sampler2d
	]] bloom_in {};

	struct [[
		= shaders::binding<0, 1>{},
		= shaders::storage_image
	]] bloom_out {
		using element = vec4f;
	};

	struct [[
		= shaders::binding<0, 0>{},
		= shaders::storage_image
	]] bloom_up_out {
		using element = vec4f;
	};

	struct [[= shaders::shader_struct]] downsample_push_constants {
		std::uint32_t use_karis_average;
	};

	struct [[= shaders::shader_struct]] upsample_push_constants {
		float radius;
	};

	using downsample_bindings = type_pack<bloom_in, bloom_out>;
	using upsample_bindings = type_pack<bloom_up_out>;

	using downsample_entry = gpu::compute_entry<gpu::body_path<"Compute/bloom_downsample">, gpu::layout<"bloom_downsample">, gpu::bindings<downsample_bindings>, gpu::push_constant<downsample_push_constants>, gpu::threads<8, 8, 1>, gpu::system_values<gpu::dispatch_thread_id>>;

	using upsample_entry = gpu::compute_entry<gpu::body_path<"Compute/bloom_upsample">, gpu::layout<"bloom_upsample">, gpu::bindings<upsample_bindings>, gpu::push_constant<upsample_push_constants>, gpu::threads<8, 8, 1>, gpu::system_values<gpu::dispatch_thread_id>>;

	auto mips_for_quality(
		quality_level q
	) -> std::uint32_t;

	auto compute_mip_chain(
		vec2u screen_extent,
		quality_level q
	) -> std::
		pair<std::uint32_t, std::array<vec2u, max_mip_count>>;

	auto recreate_mip_chain(
		const gpu::context::data& gpu_s,
		system::data& d
	) -> void;

	auto rewrite_descriptors(
		const gpu::context::data& gpu_s,
		system::data& d
	) -> void;
}

auto gse::renderer::bloom::mips_for_quality(const quality_level q) -> std::uint32_t {
	switch (q) {
		case quality_level::off:
			return 0;
		case quality_level::low:
			return 4;
		case quality_level::medium:
			return 6;
		case quality_level::high:
			return max_mip_count;
	}
	return 0;
}

auto gse::renderer::bloom::compute_mip_chain(const vec2u screen_extent, const quality_level q) -> std::pair<std::uint32_t, std::array<vec2u, max_mip_count>> {
	std::array<vec2u, max_mip_count> extents{};
	const std::uint32_t requested = mips_for_quality(q);
	if (requested == 0 || screen_extent.x() == 0 || screen_extent.y() == 0) {
		return { 0, extents };
	}

	vec2u current{ std::max(screen_extent.x() / 2u, 1u), std::max(screen_extent.y() / 2u, 1u) };
	std::uint32_t produced = 0;
	for (std::uint32_t i = 0; i < requested; ++i) {
		if (current.x() < min_mip_extent || current.y() < min_mip_extent) {
			break;
		}
		extents[i] = current;
		++produced;
		current = vec2u{ std::max(current.x() / 2u, 1u), std::max(current.y() / 2u, 1u) };
	}
	return { produced, extents };
}

auto gse::renderer::bloom::recreate_mip_chain(const gpu::context::data& gpu_s, system::data& d) -> void {
	const auto [count, extents] = compute_mip_chain(gpu_s.render_graph->extent(), d.bloom_quality);
	d.active_mip_count = count;
	d.mip_extents = extents;

	for (std::uint32_t i = 0; i < max_mip_count; ++i) {
		if (i < count) {
			d.mips_down[i] = gpu::image::create(
				gpu_s.device->allocator(),
				{
					.size = extents[i],
					.format = gpu::image_format::r16g16b16a16_sfloat,
					.usage = gpu::image_flag::storage | gpu::image_flag::sampled,
				},
				std::format("bloom_down_{}", i)
			);
			gpu::transition_image_to(*gpu_s.device, d.mips_down[i]);
		}
		else {
			d.mips_down[i] = {};
		}
		d.mips_up[i] = {};
	}
}

auto gse::renderer::bloom::rewrite_descriptors(const gpu::context::data& gpu_s, system::data& d) -> void {
	auto& hdr = gpu_s.render_graph->framebuffer_image<targets::hdr_color>();
	if (!hdr.handle() || d.active_mip_count == 0) {
		return;
	}

	for (std::uint32_t i = 0; i < d.active_mip_count; ++i) {
		const gpu::image& source = (i == 0) ? hdr : d.mips_down[i - 1];
		for (std::size_t f = 0; f < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++f) {
			gpu::descriptor_writer(gpu::context::device_handle(*gpu_s.device), d.downsample_descriptors[i][f])
				.combined_image_sampler<bloom_in>(source, d.bloom_sampler)
				.storage_image<bloom_out>(d.mips_down[i])
				.commit();
		}
	}
}

auto gse::renderer::bloom::system::run(run_context& ctx, const gpu::context::data& gpu_s, data& d) -> async::task<> {
	d.downsample_pipeline = gpu::build_compute_pipeline(
		*gpu_s.device,
		*gpu_s.shader_registry,
		*gpu_s.bindless_textures,
		downsample_entry::pod
	);

	d.bloom_sampler = gpu::sampler::create(
		gpu_s.device->allocator(),
		{
			.min = gpu::sampler_filter::linear,
			.mag = gpu::sampler_filter::linear,
			.address_u = gpu::sampler_address_mode::clamp_to_edge,
			.address_v = gpu::sampler_address_mode::clamp_to_edge,
			.address_w = gpu::sampler_address_mode::clamp_to_edge,
		}
	);

	for (std::uint32_t i = 0; i < max_mip_count; ++i) {
		for (std::size_t f = 0; f < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++f) {
			d.downsample_descriptors[i][f] = gpu::allocate_descriptors(
				*gpu_s.shader_registry,
				gpu_s.device->descriptor_heap(),
				downsample_entry::pod
			);
		}
	}

	recreate_mip_chain(gpu_s, d);
	rewrite_descriptors(gpu_s, d);

	gpu::context::on_swap_chain_recreate(gpu_s, [&gpu_s, &d]() {
		recreate_mip_chain(gpu_s, d);
		rewrite_descriptors(gpu_s, d);
	});

	co_return;
}

auto gse::renderer::bloom::system::frame(const frame_context& ctx, shared_view<gpu::context> gpu_s, data& d) -> async::task<> {
	if (!gpu_s.render_graph->frame_in_progress()) {
		co_return;
	}

	const std::uint32_t count = d.active_mip_count;
	if (count == 0) {
		co_return;
	}

	auto& hdr = gpu_s.render_graph->framebuffer_image<targets::hdr_color>();
	if (!hdr.handle()) {
		co_return;
	}

	const auto frame_index = gpu_s.render_graph->current_frame();

	auto rec = co_await gpu::pass<downsample_pass>(ctx)
		.pipeline(d.downsample_pipeline)
		.after<forward::system, atmosphere::sky_raster_pass, physics_debug::system, sdf_grid::system, world_text::system>();
	rec.sample_image(hdr, gpu::pipeline_stage_flag::compute_shader);
	rec.bind_descriptors(d.downsample_pipeline, d.downsample_descriptors[0][frame_index]);
	rec.push(
		d.downsample_pipeline,
		gpu::typed_push_constants<downsample_push_constants>{
			.data = {
				.use_karis_average = 1u
			},
			.stages = gpu::stage_flag::compute,
		}
	);
	rec.dispatch((d.mip_extents[0].x() + 7u) / 8u, (d.mip_extents[0].y() + 7u) / 8u, 1);

	for (std::uint32_t i = 1; i < count; ++i) {
		rec = co_await gpu::pass<downsample_pass>(ctx).pipeline(d.downsample_pipeline);
		rec.bind_descriptors(d.downsample_pipeline, d.downsample_descriptors[i][frame_index]);
		rec.push(
			d.downsample_pipeline,
			gpu::typed_push_constants<downsample_push_constants>{
				.data = {
					.use_karis_average = 0u
				},
				.stages = gpu::stage_flag::compute,
			}
		);
		rec.dispatch((d.mip_extents[i].x() + 7u) / 8u, (d.mip_extents[i].y() + 7u) / 8u, 1);
	}
}
