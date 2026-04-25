module gse.graphics;

import std;

import :ui_renderer;
import :texture;
import :font;

import gse.os;
import gse.assets;
import gse.gpu;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.math;

auto gse::renderer::ui::add_sprite_quad(linear_vector<vertex>& vertices, linear_vector<std::uint32_t>& indices, const unified_command& cmd) -> void {
    if (vertices.size() + 4 > max_vertices || indices.size() + 6 > max_indices) {
        return;
    }

    const auto base_index = static_cast<std::uint32_t>(vertices.size());

    const vec2f top_left = cmd.rect.top_left();
    const vec2f size = cmd.rect.size();
    const vec2f center = {
        top_left.x() + size.x() * 0.5f,
        top_left.y() - size.y() * 0.5f
    };

    const vec2f half = { size.x() * 0.5f, size.y() * 0.5f };
    const vec2f l0 = { -half.x(),  half.y() };
    const vec2f l1 = {  half.x(),  half.y() };
    const vec2f l2 = {  half.x(), -half.y() };
    const vec2f l3 = { -half.x(), -half.y() };

    const vec2f p0 = center + rotate(l0, cmd.rotation);
    const vec2f p1 = center + rotate(l1, cmd.rotation);
    const vec2f p2 = center + rotate(l2, cmd.rotation);
    const vec2f p3 = center + rotate(l3, cmd.rotation);

    const float u0 = cmd.uv_rect.x();
    const float v0 = cmd.uv_rect.y();
    const float u1 = cmd.uv_rect.x() + cmd.uv_rect.z();
    const float v1 = cmd.uv_rect.y() + cmd.uv_rect.w();

    vertices.push_back(vertex{ p0, { u0, v0 }, cmd.color, l0, half, cmd.corner_radius, 0.f });
    vertices.push_back(vertex{ p1, { u1, v0 }, cmd.color, l1, half, cmd.corner_radius, 0.f });
    vertices.push_back(vertex{ p2, { u1, v1 }, cmd.color, l2, half, cmd.corner_radius, 0.f });
    vertices.push_back(vertex{ p3, { u0, v1 }, cmd.color, l3, half, cmd.corner_radius, 0.f });

    indices.push_back(base_index + 0);
    indices.push_back(base_index + 2);
    indices.push_back(base_index + 1);
    indices.push_back(base_index + 0);
    indices.push_back(base_index + 3);
    indices.push_back(base_index + 2);
}

auto gse::renderer::ui::add_text_quads(linear_vector<vertex>& vertices, linear_vector<std::uint32_t>& indices, const unified_command& cmd) -> void {
    const float font_px_range = cmd.font->pixel_range() * (cmd.scale / cmd.font->glyph_cell_size());

    for (const auto& [screen_rect, uv_rect] : cmd.font->text_layout(cmd.text, cmd.position, cmd.scale)) {
        if (vertices.size() + 4 > max_vertices || indices.size() + 6 > max_indices) {
            break;
        }

        const auto base_index = static_cast<std::uint32_t>(vertices.size());

        const vec2f top_left = screen_rect.top_left();
        const vec2f sz = screen_rect.size();

        const vec2f p0 = top_left;
        const vec2f p1 = { top_left.x() + sz.x(), top_left.y() };
        const vec2f p2 = { top_left.x() + sz.x(), top_left.y() - sz.y() };
        const vec2f p3 = { top_left.x(), top_left.y() - sz.y() };

        const float u0 = uv_rect.x();
        const float v0 = uv_rect.y();
        const float u1 = uv_rect.x() + uv_rect.z();
        const float v1 = uv_rect.y() + uv_rect.w();

        vertices.push_back({ p0, { u0, v1 }, cmd.color, {}, {}, 0.f, font_px_range });
        vertices.push_back({ p1, { u1, v1 }, cmd.color, {}, {}, 0.f, font_px_range });
        vertices.push_back({ p2, { u1, v0 }, cmd.color, {}, {}, 0.f, font_px_range });
        vertices.push_back({ p3, { u0, v0 }, cmd.color, {}, {}, 0.f, font_px_range });

        indices.push_back(base_index + 0);
        indices.push_back(base_index + 2);
        indices.push_back(base_index + 1);
        indices.push_back(base_index + 0);
        indices.push_back(base_index + 3);
        indices.push_back(base_index + 2);
    }
}

auto gse::renderer::ui::system::initialize(const init_context& phase, resources& r, frame_data& fd, state& s) -> void {
    auto& ctx = phase.get<gpu::context>();

    r.sprite_shader = ctx.get<shader>("Shaders/Standard2D/sprite");
    ctx.instantly_load(r.sprite_shader);

    r.sprite_pipeline = gpu::create_graphics_pipeline(ctx, *r.sprite_shader, {
        .rasterization = { .cull = gpu::cull_mode::none },
        .depth = { .test = false, .write = false },
        .blend = gpu::blend_preset::alpha_premultiplied,
        .depth_fmt = gpu::depth_format::none,
        .push_constant_block = "push_constants",
    });

    r.text_shader = ctx.get<shader>("Shaders/Standard2D/msdf");
    ctx.instantly_load(r.text_shader);

    r.text_pipeline = gpu::create_graphics_pipeline(ctx, *r.text_shader, {
        .rasterization = { .cull = gpu::cull_mode::none },
        .depth = { .test = false, .write = false },
        .blend = gpu::blend_preset::alpha_premultiplied,
        .depth_fmt = gpu::depth_format::none,
        .push_constant_block = "push_constants",
    });

    constexpr std::size_t vertex_buffer_size = max_vertices * sizeof(vertex);
    constexpr std::size_t index_buffer_size = max_indices * sizeof(std::uint32_t);

    for (auto& [vertex_buffer, index_buffer] : r.gpu_frames) {
        vertex_buffer = gpu::create_buffer(ctx, {
            .size = vertex_buffer_size,
            .usage = gpu::buffer_flag::vertex,
        });

        index_buffer = gpu::create_buffer(ctx, {
            .size = index_buffer_size,
            .usage = gpu::buffer_flag::index,
        });
    }
}

auto gse::renderer::ui::system::update(const update_context& ctx, const resources& r, frame_data& fd, state& s) -> async::task<> {
    const auto& sprite_commands = ctx.read_channel<sprite_command>();
    const auto& text_commands = ctx.read_channel<text_command>();

    if (sprite_commands.empty() && text_commands.empty()) {
        co_return;
    }

    auto& [vertices, indices, batches] = fd.data.write();
    vertices.clear();
    indices.clear();
    batches.clear();

    std::vector<unified_command> unified;
    unified.reserve(sprite_commands.size() + text_commands.size());

    for (const auto& [rect, color, texture, uv_rect, clip_rect, rotation, layer, z_order, corner_radius] : sprite_commands) {
        if (!texture.valid()) {
            continue;
        }

        unified.push_back({
            .type = command_type::sprite,
            .layer = layer,
            .z_order = z_order,
            .clip_rect = clip_rect,
            .texture = texture,
            .rect = rect,
            .color = color,
            .uv_rect = uv_rect,
            .rotation = rotation,
            .corner_radius = corner_radius,
            .font = {},
            .text = {},
            .position = {},
            .scale = 1.0f,
        });
    }

    for (const auto& [font, text, position, scale, color, clip_rect, layer, z_order] : text_commands) {
        if (!font.valid() || text.empty()) {
            continue;
        }

        unified.push_back({
            .type = command_type::text,
            .layer = layer,
            .z_order = z_order,
            .clip_rect = clip_rect,
            .texture = {},
            .rect = {},
            .color = color,
            .uv_rect = {},
            .rotation = {},
            .font = font,
            .text = text,
            .position = position,
            .scale = scale,
        });
    }

    std::ranges::stable_sort(unified, [](const unified_command& a, const unified_command& b) {
        if (a.layer != b.layer) {
            return static_cast<std::uint8_t>(a.layer) < static_cast<std::uint8_t>(b.layer);
        }

        if (a.z_order != b.z_order) {
            return a.z_order < b.z_order;
        }

        if (a.type != b.type) {
            return a.type < b.type;
        }

        if (a.type == command_type::sprite) {
            return a.texture.id().number() < b.texture.id().number();
        }
        return a.font.id().number() < b.font.id().number();
    });

    auto current_type = command_type::sprite;
    resource::handle<texture> current_texture;
    resource::handle<font> current_font;
    std::optional<rect_t<vec2f>> current_clip;
    std::uint32_t batch_index_start = 0;

    auto flush_batch = [&] {
        if (indices.size() > batch_index_start) {
            batches.push_back({
                .type = current_type,
                .index_offset = batch_index_start,
                .index_count = static_cast<std::uint32_t>(indices.size() - batch_index_start),
                .clip_rect = current_clip,
                .texture = current_texture,
                .font = current_font,
            });
        }
        batch_index_start = static_cast<std::uint32_t>(indices.size());
    };

    for (const auto& cmd : unified) {
        bool needs_flush = false;

        if (cmd.type != current_type) {
            needs_flush = true;
        } else if (cmd.clip_rect != current_clip) {
            needs_flush = true;
        } else if (cmd.type == command_type::sprite && cmd.texture.id() != current_texture.id()) {
            needs_flush = true;
        } else if (cmd.type == command_type::text && cmd.font.id() != current_font.id()) {
            needs_flush = true;
        }

        if (needs_flush) {
            flush_batch();
            current_type = cmd.type;
            current_clip = cmd.clip_rect;
            current_texture = cmd.texture;
            current_font = cmd.font;
        }

        if (cmd.type == command_type::sprite) {
            add_sprite_quad(vertices, indices, cmd);
        } else {
            add_text_quads(vertices, indices, cmd);
        }
    }

    flush_batch();

    fd.data.publish();

    co_return;
}

auto gse::renderer::ui::system::frame(frame_context& ctx, const resources& r, frame_data& fd, const state& s) -> async::task<> {
    auto& gpu = ctx.get<gpu::context>();

    if (!gpu.graph().frame_in_progress()) {
        co_return;
    }

    const auto& [vertices, indices, batches] = fd.data.read();

    if (batches.empty()) {
        co_return;
    }
    const auto frame_index = gpu.graph().current_frame();
    auto& [vertex_buffer, index_buffer] = r.gpu_frames[frame_index];

    gse::memcpy(vertex_buffer.mapped(), vertices);
    gse::memcpy(index_buffer.mapped(), indices);

    const auto ext = gpu.graph().extent();
    const auto width = ext.x();
    const auto height = ext.y();
    const vec2f window_size = { static_cast<float>(width), static_cast<float>(height) };

    const auto projection = orthographic(
        meters(0.0f),
        meters(static_cast<float>(width)),
        meters(0.0f),
        meters(static_cast<float>(height)),
        meters(-1.0f),
        meters(1.0f)
    );

    auto sprite_pc = r.sprite_shader->cache_push_block("push_constants");
    sprite_pc.set("projection", projection);

    auto text_pc = r.text_shader->cache_push_block("push_constants");
    text_pc.set("projection", projection);

    auto pass = gpu.graph().add_pass<ui::state>();
    pass.track(vertex_buffer);
    pass.track(index_buffer);

    const vec2u ext_size{ width, height };
    const auto& bindless_region = gpu.bindless_textures().region();

    pass.color_output_load();

    auto& rec = co_await pass.record();

    rec.bind_vertex(vertex_buffer);
    rec.bind_index(index_buffer);

    rec.set_viewport(ext_size);
    rec.set_scissor(ext_size);

    auto bound_type = command_type::sprite;
    bool first_batch = true;

    for (const auto& [type, index_offset, index_count, clip_rect, texture, font] : batches) {
        if (index_count == 0) {
            continue;
        }

        if (first_batch || type != bound_type) {
            if (type == command_type::sprite) {
                rec.bind(r.sprite_pipeline);
                rec.bind_descriptors(r.sprite_pipeline, bindless_region, 2);
            } else {
                rec.bind(r.text_pipeline);
                rec.bind_descriptors(r.text_pipeline, bindless_region, 2);
            }
            bound_type = type;
            first_batch = false;
        }

        std::uint32_t tex_idx = 0;
        bool has_texture = false;
        if (type == command_type::sprite) {
            if (texture.valid() && texture->bindless_slot()) {
                tex_idx = texture->bindless_slot().index;
                has_texture = true;
            }
        } else {
            if (font.valid() && font->texture()->bindless_slot()) {
                tex_idx = font->texture()->bindless_slot().index;
                has_texture = true;
            }
        }

        if (!has_texture) {
            continue;
        }

        if (type == command_type::sprite) {
            sprite_pc.set("tex_idx", tex_idx);
            rec.push(r.sprite_pipeline, sprite_pc);
        } else {
            text_pc.set("tex_idx", tex_idx);
            rec.push(r.text_pipeline, text_pc);
        }

        if (clip_rect) {
            const float left = std::max(0.0f, clip_rect->left());
            const float right = std::min(window_size.x(), clip_rect->right());
            const float bottom = std::max(0.0f, clip_rect->bottom());
            const float top = std::min(window_size.y(), clip_rect->top());
            rec.set_scissor(
                static_cast<std::int32_t>(left),
                static_cast<std::int32_t>(window_size.y() - top),
                static_cast<std::uint32_t>(std::max(0.0f, right - left)),
                static_cast<std::uint32_t>(std::max(0.0f, top - bottom))
            );
        } else {
            rec.set_scissor(ext_size);
        }

        rec.draw_indexed(index_count, 1, index_offset, 0, 0);
    }
}
