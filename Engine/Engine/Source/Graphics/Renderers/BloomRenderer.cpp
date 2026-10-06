module gse.graphics:bloom_renderer_impl;

import std;

import :bloom_renderer;
import :atmosphere_renderer;
import :forward_renderer;
import :physics_debug_renderer;
import :render_targets;
import :sdf_grid_renderer;
import :taa_renderer;
import :world_text_renderer;


import gse.gpu;
import gse.gpu_record;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.math;
import gse.meta;
import gse.log;
import gse.time;

namespace gse::renderer::bloom {
	struct [[= shaders::texture2d]] bloom_in {
		using element = vec4f;
	};

	struct [[= shaders::storage_image]] bloom_out {
		using element = vec4f;
	};

	struct [[= shaders::sampler_state]] bloom_sampler {};

	struct [[= shaders::texture2d]] bloom_up_in {
		using element = vec4f;
	};

	struct [[= shaders::texture2d]] bloom_up_dn {
		using element = vec4f;
	};

	struct [[= shaders::storage_image]] bloom_up_out {
		using element = vec4f;
	};

	struct [[= shaders::sampler_state]] bloom_up_sampler {};

	struct [[= shaders::shader_struct]] downsample_push_constants {
		vec2u dst_active_extent;
		vec2f inv_src_allocated_extent;
		vec2f src_uv_scale;
		vec2f src_uv_max;
		std::uint32_t use_karis_average;
	};

	struct [[= shaders::shader_struct]] upsample_push_constants {
		vec2u dst_active_extent;
		vec2f inv_src_allocated_extent;
		vec2f src_uv_scale;
		vec2f src_uv_max;
		vec2f dn_uv_scale;
		vec2f dn_uv_max;
		float radius;
	};

	using downsample_bindings = type_pack<bloom_in, bloom_out, bloom_sampler>;
	using upsample_bindings = type_pack<bloom_up_in, bloom_up_dn, bloom_up_out, bloom_up_sampler>;

	using downsample_entry = gpu::compute_entry<gpu::body_path<"Compute/bloom_downsample">, gpu::bindings<downsample_bindings>, gpu::helpers<"Screen/screen_target">, gpu::push_constant<downsample_push_constants>, gpu::threads<8, 8, 1>, gpu::system_values<gpu::dispatch_thread_id>>;

	using upsample_entry = gpu::compute_entry<gpu::body_path<"Compute/bloom_upsample">, gpu::bindings<upsample_bindings>, gpu::helpers<"Screen/screen_target">, gpu::push_constant<upsample_push_constants>, gpu::threads<8, 8, 1>, gpu::system_values<gpu::dispatch_thread_id>>;

	auto mips_for_quality(
		quality_level q
	) -> std::uint32_t;

	auto compute_mip_chain(
		vec2u screen_extent,
		quality_level q
	) -> std::pair<std::uint32_t, std::array<vec2u, max_mip_count>>;

	auto recreate_mip_chain(
		shared_view<gpu::context::data> gpu_s,
		data& d
	) -> void;

	auto rewrite_descriptors(
		shared_view<gpu::context::data> gpu_s,
		data& d
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

auto gse::renderer::bloom::recreate_mip_chain(const shared_view<gpu::context::data> gpu_s, data& d) -> void {
	const auto [count, extents] = compute_mip_chain(gpu_s.render_graph->extent(), d.bloom_quality);
	const auto [allocated_count, allocated_extents] = compute_mip_chain(gpu_s.render_graph->allocated_extent(), d.bloom_quality);
	const auto previous_allocated_extents = d.mip_allocated_extents;
	const auto previous_count = d.active_mip_count;
	d.active_mip_count = std::min(count, allocated_count);
	d.mip_extents = extents;
	d.mip_allocated_extents = allocated_extents;

	if (d.active_mip_count == previous_count && allocated_extents == previous_allocated_extents) {
		return;
	}

	for (std::uint32_t i = 0; i < max_mip_count; ++i) {
		d.mips_down[i] = {};
		d.mips_up[i] = {};
		if (i < d.active_mip_count) {
			d.mips_down[i] = gpu_s.device->create_image(
				{
					.size = allocated_extents[i],
					.format = gpu::image_format::r16g16b16a16_sfloat,
					.usage = { gpu::image_flag::storage, gpu::image_flag::sampled },
					.bindless = true
				},
				std::format("bloom_down_{}", i)
			);
			gpu::transition_image_to(*gpu_s.device, d.mips_down[i]);
			d.mips_up[i] = gpu_s.device->create_image(
				{
					.size = allocated_extents[i],
					.format = gpu::image_format::r16g16b16a16_sfloat,
					.usage = { gpu::image_flag::storage, gpu::image_flag::sampled },
					.bindless = true
				},
				std::format("bloom_up_{}", i)
			);
			gpu::transition_image_to(*gpu_s.device, d.mips_up[i]);
		}
	}
}

auto gse::renderer::bloom::rewrite_descriptors(const shared_view<gpu::context::data> gpu_s, data& d) -> void {
	d.hdr_view = {};

	auto& hdr = gpu_s.render_graph->framebuffer_image<targets::post_taa_color>();
	if (!hdr.handle() || d.active_mip_count == 0) {
		return;
	}

	if (!d.hdr_view.valid()) {
		d.hdr_view = gpu_s.device->allocate_image_slot();
	}
	gpu_s.device->write_sampled_image(d.hdr_view.slot(), hdr);
}

auto gse::renderer::bloom::init(context& ctx, const shared_view<gpu::context::data> gpu_s, data& d) -> async::task<> {
	d.downsample_pipeline = gpu::build_compute_program(*gpu_s.device, downsample_entry::pod);
	d.upsample_pipeline = gpu::build_compute_program(*gpu_s.device, upsample_entry::pod);

	d.sampler = gpu_s.device->register_sampler(
		gpu::sampler_desc{
			.min = gpu::sampler_filter::linear,
			.mag = gpu::sampler_filter::linear,
			.address_u = gpu::sampler_address_mode::clamp_to_edge,
			.address_v = gpu::sampler_address_mode::clamp_to_edge,
			.address_w = gpu::sampler_address_mode::clamp_to_edge,
		}
	);

	recreate_mip_chain(gpu_s, d);
	rewrite_descriptors(gpu_s, d);

	gpu::context::on_swap_chain_recreate(
		gpu_s,
		[gpu_s, &d]() {
			recreate_mip_chain(gpu_s, d);
			rewrite_descriptors(gpu_s, d);
		}
	);

	return {};
}

auto gse::renderer::bloom::frame(const context& ctx, shared_view<gpu::context::data> gpu_s, data& d, const channel_write<gpu::render_pass_request> pass_out) -> async::task<> {
	if (!gpu_s.render_graph->frame_in_progress()) {
		co_return;
	}

	const std::uint32_t count = d.active_mip_count;
	if (count == 0) {
		co_return;
	}

	const auto level_extents = [&d, gpu_s](const std::uint32_t level) -> std::pair<vec2u, vec2u> {
		if (level == 0) {
			return { gpu_s.render_graph->extent(), gpu_s.render_graph->allocated_extent() };
		}
		return { d.mip_extents[level - 1], d.mip_allocated_extents[level - 1] };
	};

	auto& hdr = gpu_s.render_graph->framebuffer_image<targets::post_taa_color>();
	if (!hdr.handle()) {
		co_return;
	}

	if (!d.hdr_view.valid()) {
		d.hdr_view = gpu_s.device->allocate_image_slot();
	}
	gpu_s.device->write_sampled_image(d.hdr_view.slot(), hdr);

	auto rec = co_await gpu::pass<^^downsample_pass>(pass_out)
		.pipeline(d.downsample_pipeline)
		.after<^^forward::frame, ^^atmosphere::sky_raster_pass, ^^physics_debug::frame, ^^sdf_grid::frame, ^^world_text::frame, ^^taa::frame>();

	for (std::uint32_t i = 0; i < count; ++i) {
		const auto source_slot = (i == 0) ? d.hdr_view.slot() : d.mips_down[i - 1].sampled_slot();
		const auto [source_active, source_allocated] = level_extents(i);
		rec.dispatch<downsample_entry>(
			{
				.dst_active_extent = d.mip_extents[i],
				.inv_src_allocated_extent = vec2f{ 1.0f / static_cast<float>(source_allocated.x()), 1.0f / static_cast<float>(source_allocated.y()) },
				.src_uv_scale = gpu::screen_uv_scale_for(source_active, source_allocated),
				.src_uv_max = gpu::screen_uv_max_for(source_active, source_allocated),
				.use_karis_average = i == 0 ? 1u : 0u
			},
			{
				.bloom_in = source_slot,
				.bloom_out = d.mips_down[i].storage_slot(),
				.bloom_sampler = d.sampler.slot(),
			},
			vec3u{
				(d.mip_extents[i].x() + 7u) / 8u,
				(d.mip_extents[i].y() + 7u) / 8u,
				1u,
			}
		);
	}

	if (count < 2) {
		co_return;
	}

	auto up_rec = co_await gpu::pass<^^upsample_pass>(pass_out).pipeline(d.upsample_pipeline).after<^^downsample_pass>();

	for (std::uint32_t i = count - 1; i-- > 0;) {
		const auto up_source = (i + 1 == count - 1) ? d.mips_down[count - 1].sampled_slot() : d.mips_up[i + 1].sampled_slot();
		const auto [source_active, source_allocated] = level_extents(i + 2);
		const auto [dn_active, dn_allocated] = level_extents(i + 1);
		up_rec.dispatch<upsample_entry>(
			{
				.dst_active_extent = d.mip_extents[i],
				.inv_src_allocated_extent = vec2f{ 1.0f / static_cast<float>(source_allocated.x()), 1.0f / static_cast<float>(source_allocated.y()) },
				.src_uv_scale = gpu::screen_uv_scale_for(source_active, source_allocated),
				.src_uv_max = gpu::screen_uv_max_for(source_active, source_allocated),
				.dn_uv_scale = gpu::screen_uv_scale_for(dn_active, dn_allocated),
				.dn_uv_max = gpu::screen_uv_max_for(dn_active, dn_allocated),
				.radius = d.bloom_radius
			},
			{
				.bloom_up_in = up_source,
				.bloom_up_dn = d.mips_down[i].sampled_slot(),
				.bloom_up_out = d.mips_up[i].storage_slot(),
				.bloom_up_sampler = d.sampler.slot(),
			},
			vec3u{
				(d.mip_extents[i].x() + 7u) / 8u,
				(d.mip_extents[i].y() + 7u) / 8u,
				1u,
			}
		);
	}
}
