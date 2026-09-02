module gse.graphics:light_tile_heatmap_renderer_impl;

import std;

import :light_tile_heatmap_renderer;
import :light_culling_renderer;
import :tonemap_renderer;

import gse.gpu;
import gse.gpu_record;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.math;
import gse.meta;

namespace gse::renderer::light_tile_heatmap {
	struct [[
		= shaders::binding<0, 0>{},
		= shaders::ssbo_readonly
	]] tile_light_table {
		using element = vec2u;
	};

	struct [[= shaders::shader_struct]] push_constants {
		vec2u tile_counts;
		std::uint32_t tile_size;
		std::uint32_t max_lights_per_tile;
		float opacity;
	};

	using shader_binding_types = type_pack<tile_light_table>;

	using entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/LightTileHeatmap">,
		gpu::bindings<shader_binding_types>,
		gpu::vertex_stage<"vs_main">,
		gpu::fragment_stage<"fs_main">,
		gpu::push_constant<push_constants>,
		gpu::rasterization<gpu::polygon_mode::fill, gpu::cull_mode::none>,
		gpu::blend<gpu::blend_preset::alpha>,
		gpu::depth<false, false>,
		gpu::depth_target<gpu::depth_format::none>
	>;
}

auto gse::renderer::light_tile_heatmap::init(context& ctx, const shared_view<gpu::context::data> gpu_s, data& d) -> async::task<> {
	d.pipeline = gpu::build_graphics_program(*gpu_s.device, entry::pod);
	return {};
}

auto gse::renderer::light_tile_heatmap::frame(const context& ctx, shared_view<gpu::context::data> gpu_s, const data& d, const channel_write<gpu::render_pass_request> pass_out, shared_view<light_culling::data> lc_r) -> async::task<> {
	if (!d.show_tile_heatmap) {
		co_return;
	}

	if (!gpu_s.render_graph->frame_in_progress() || !gpu_s.render_graph->target_live()) {
		co_return;
	}

	const auto tiles = light_culling::tile_count(lc_r.current_width, lc_r.current_height);
	if (tiles.x() == 0 || tiles.y() == 0) {
		co_return;
	}

	const auto frame_index = gpu_s.render_graph->current_frame();
	const auto ext = gpu_s.render_graph->extent();

	auto rec = co_await gpu::pass<^^frame>(pass_out)
		.pipeline(d.pipeline)
		.color(gpu::load_color())
		.after<^^tonemap::frame>();

	rec.set_viewport(ext);
	rec.set_scissor(ext);
	rec.push_bindings<entry>(
		{
			.tile_counts = tiles,
			.tile_size = light_culling::tile_size,
			.max_lights_per_tile = light_culling::max_lights_per_tile,
			.opacity = d.heatmap_opacity,
		},
		{
			.tile_light_table = gpu_s.device->buffer_slot(lc_r.tile_light_table_buffers[frame_index]),
		}
	);
	rec.draw(3);
}
