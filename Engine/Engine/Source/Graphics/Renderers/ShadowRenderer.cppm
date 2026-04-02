export module gse.graphics:shadow_renderer;

import std;

import :render_component;
import :mesh;
import :model;
import :spot_light;
import :directional_light;
import :point_light;
import :cube_map;
import :shadow_data;
import :geometry_collector;

import gse.platform;
import gse.physics;
import gse.math;
import gse.utility;

export namespace gse::renderer::shadow {
	struct system {
		static auto initialize(const initialize_phase& phase, state& s) -> void;
		static auto update(update_phase& phase, state& s) -> void;
		static auto render(const render_phase& phase, const state& s) -> void;
	};
}

auto gse::renderer::shadow::state::shadow_map(const std::size_t index) const -> const gpu::image& {
	return shadow_maps[index];
}

auto gse::renderer::shadow::state::point_shadow_gpu_image(const std::size_t index) const -> const gpu::image& {
	return point_shadow_cubemaps[index].gpu_image();
}

auto gse::renderer::shadow::state::point_shadow_gpu_sampler(const std::size_t index) const -> const gpu::sampler& {
	return point_shadow_cubemaps[index].gpu_sampler();
}

auto gse::renderer::shadow::state::point_shadow_face_view(const std::size_t index, const std::size_t face) const -> gpu::image_view {
	return point_shadow_cubemaps[index].face_view(face);
}

auto gse::renderer::shadow::state::shadow_texel_size() const -> vec2f {
	return vec2f(
		1.0f / static_cast<float>(shadow_extent.x()),
		1.0f / static_cast<float>(shadow_extent.y())
	);
}

auto gse::renderer::shadow::system::initialize(const initialize_phase& phase, state& s) -> void {
	auto& ctx = phase.get<gpu::context>();

	s.shader_handle = ctx.get<shader>("Shaders/Deferred3D/shadow_pass");
	ctx.instantly_load(s.shader_handle);

	s.pipeline = gpu::create_graphics_pipeline(ctx.device_ref(), *s.shader_handle, {
		.rasterization = {
			.cull = gpu::cull_mode::back,
			.depth_bias = true,
			.depth_bias_constant = 1.25f,
			.depth_bias_clamp = 0.0f,
			.depth_bias_slope = 1.75f
		},
		.depth = {
			.test = true,
			.write = true,
			.compare = gpu::compare_op::less_or_equal
		},
		.color = gpu::color_format::none,
		.push_constant_block = "push_constants"
	});

	for (auto& img : s.shadow_maps) {
		img = gpu::create_image(ctx.device_ref(), {
			.size = s.shadow_extent,
			.format = gpu::image_format::d32_sfloat,
			.usage = gpu::image_flag::depth_attachment | gpu::image_flag::sampled,
			.ready_layout = gpu::image_layout::general
		});
	}

	for (auto& cm : s.point_shadow_cubemaps) {
		cm.create(ctx, static_cast<int>(s.point_shadow_extent.x()), true);
	}
}

auto gse::renderer::shadow::system::update(update_phase& phase, state& s) -> void {
	render_data data;

	std::size_t next_shadow_index = 0;

	const auto dir_view = phase.registry.view<directional_light_component>();
	const auto spot_view = phase.registry.view<spot_light_component>();
	const auto point_view = phase.registry.view<point_light_component>();

	for (const auto& comp : dir_view) {
		if (next_shadow_index >= max_shadow_lights) {
			break;
		}

		shadow_light_entry entry;
		entry.shadow_index = static_cast<int>(next_shadow_index);

		auto dir = comp.direction;
		vec3<length> light_pos = -dir * 10.0f;

		entry.proj = orthographic(
			meters(-100.0f),
			meters(100.0f),
			meters(-100.0f),
			meters(100.0f),
			comp.near_plane,
			comp.far_plane
		);

		auto up = ensure_non_collinear_up(dir, axis_y);

		entry.view = look_at(
			light_pos,
			light_pos + vec3<length>(dir),
			up
		);

		entry.ignore_list_ids = comp.ignore_list_ids;

		data.lights.push_back(std::move(entry));
		++next_shadow_index;
	}

	for (const auto& comp : spot_view) {
		if (next_shadow_index >= max_shadow_lights) {
			break;
		}

		shadow_light_entry entry;
		entry.shadow_index = static_cast<int>(next_shadow_index);

		const vec3<length> pos = vec3<length>(comp.position - vec3<position>{});
		auto dir = comp.direction;
		const auto cutoff = comp.cut_off;

		entry.proj = perspective(
			cutoff,
			1.0f,
			comp.near_plane,
			comp.far_plane
		);

		auto up = ensure_non_collinear_up(dir, axis_y);

		entry.view = look_at(
			pos,
			pos + vec3<length>(dir),
			up
		);

		entry.ignore_list_ids = comp.ignore_list_ids;

		data.lights.push_back(std::move(entry));
		++next_shadow_index;
	}

	std::size_t next_point_shadow_index = 0;

	for (const auto& comp : point_view) {
		if (next_point_shadow_index >= max_point_shadow_lights) {
			break;
		}

		point_shadow_light_entry entry;
		entry.point_shadow_index = static_cast<int>(next_point_shadow_index);
		entry.world_position = vec3<length>(comp.position - vec3<position>{});
		entry.near_plane = comp.near_plane;
		entry.far_plane = comp.far_plane;

		entry.ignore_list_ids = comp.ignore_list_ids;

		const auto proj = perspective(
			degrees(90.0f),
			1.0f,
			comp.near_plane,
			comp.far_plane
		);

		const vec3<length> pos = vec3<length>(comp.position - vec3<position>{});

		entry.face_view_proj[0] = proj * look_at(
			pos, pos + vec3<length>(meters(1.0f), meters(0.0f), meters(0.0f)),
			-axis_y
		);
		entry.face_view_proj[1] = proj * look_at(
			pos, pos + vec3<length>(meters(-1.0f), meters(0.0f), meters(0.0f)),
			-axis_y
		);
		entry.face_view_proj[2] = proj * look_at(
			pos, pos + vec3<length>(meters(0.0f), meters(1.0f), meters(0.0f)),
			axis_z
		);
		entry.face_view_proj[3] = proj * look_at(
			pos, pos + vec3<length>(meters(0.0f), meters(-1.0f), meters(0.0f)),
			-axis_z
		);
		entry.face_view_proj[4] = proj * look_at(
			pos, pos + vec3<length>(meters(0.0f), meters(0.0f), meters(1.0f)),
			-axis_y
		);
		entry.face_view_proj[5] = proj * look_at(
			pos, pos + vec3<length>(meters(0.0f), meters(0.0f), meters(-1.0f)),
			-axis_y
		);

		data.point_lights.push_back(std::move(entry));
		++next_point_shadow_index;
	}

	phase.channels.push(std::move(data));
}

auto gse::renderer::shadow::system::render(const render_phase& phase, const state& s) -> void {
    auto& ctx = phase.get<gpu::context>();

    const auto& shadow_items = phase.read_channel<render_data>();
    if (shadow_items.empty()) {
        return;
    }

    const auto& geom_items = phase.read_channel<geometry_collector::render_data>();
    if (geom_items.empty()) {
        return;
    }

    if (!ctx.graph().frame_in_progress()) {
        return;
    }

    const auto& lights = shadow_items[0].lights;
    const auto& point_lights = shadow_items[0].point_lights;
    const auto& geom_data = geom_items[0];

    struct shadow_draw_batch {
        std::vector<render_queue_entry> draw_list;
        view_projection_matrix light_view_proj;
        gpu::image_view depth_view;
        vec2u extent;
    };

    std::vector<shadow_draw_batch> batches;

    for (const auto& [view, proj, ignore_list_ids, shadow_index] : lights) {
        if (shadow_index < 0 || static_cast<std::size_t>(shadow_index) >= s.shadow_maps.size()) {
            continue;
        }

        auto draw_list = geometry_collector::filter_render_queue(geom_data, ignore_list_ids);
        if (draw_list.empty()) {
            continue;
        }

        batches.push_back({
            .draw_list = std::move(draw_list),
            .light_view_proj = proj * view,
            .depth_view = s.shadow_maps[static_cast<std::size_t>(shadow_index)].view(),
            .extent = s.shadow_extent
        });
    }

    for (const auto& pl : point_lights) {
        if (pl.point_shadow_index < 0 || static_cast<std::size_t>(pl.point_shadow_index) >= s.point_shadow_cubemaps.size()) {
            continue;
        }

        auto draw_list = geometry_collector::filter_render_queue(geom_data, pl.ignore_list_ids);
        if (draw_list.empty()) {
            continue;
        }

        const auto cubemap_index = static_cast<std::size_t>(pl.point_shadow_index);

        for (std::size_t face = 0; face < 6; ++face) {
            batches.push_back({
                .draw_list = draw_list,
                .light_view_proj = pl.face_view_proj[face],
                .depth_view = s.point_shadow_face_view(cubemap_index, face),
                .extent = s.point_shadow_extent
            });
        }
    }

    if (batches.empty()) {
        return;
    }

    ctx.graph()
        .add_pass<state>()
        .record([&s, batches = std::move(batches)](gpu::recording_context& ctx) {
            for (const auto& batch : batches) {
                ctx.begin_rendering(batch.depth_view, batch.extent);

                ctx.bind(s.pipeline);

                ctx.set_viewport(batch.extent);
                ctx.set_scissor(batch.extent);

                for (const auto& e : batch.draw_list) {
                    auto pc = s.shader_handle->cache_push_block("push_constants");
                    pc.set("light_view_proj", batch.light_view_proj);
                    pc.set("model", e.model_matrix);
                    ctx.push(s.pipeline, pc);

                    const auto& mesh = e.model->meshes()[e.index];
                    ctx.bind_vertex(mesh.vertex_gpu_buffer());
                    ctx.bind_index(mesh.index_gpu_buffer());
                    ctx.draw_indexed(static_cast<std::uint32_t>(mesh.indices().size()));
                }

                ctx.end_rendering();
            }
        });
}

auto gse::renderer::ensure_non_collinear_up(const vec3f& direction, const vec3f& up) -> vec3f {
	auto normalized_direction = normalize(direction);
	auto normalized_up = normalize(up);
	if (const float dot_product = dot(normalized_direction, normalized_up); std::abs(dot_product) > 1.0f - std::numeric_limits<float>::epsilon()) {
		if (std::abs(normalized_direction.y()) > 0.9f) {
			normalized_up = axis_z;
		} else {
			normalized_up = axis_y;
		}
	}
	return normalized_up;
}
