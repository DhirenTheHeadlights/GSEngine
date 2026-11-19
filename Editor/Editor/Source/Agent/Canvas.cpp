module gse.ide.agent:canvas_impl;

import std;
import gse;

import :canvas;

namespace gse::ide::agent {
	template <typename F>
	concept animatable = std::same_as<F, float> || std::same_as<F, vec2f> || std::same_as<F, vec4f>;

	template <animatable F>
	auto pack(
		const F& value
	) -> vec4f;

	template <animatable F>
	auto unpack(
		const vec4f& packed,
		F& out
	) -> void;

	auto leaf_text(
		const json::value& node
	) -> std::optional<std::string>;

	template <typename F>
	auto bind_field(
		const json::value& node,
		std::string_view at,
		F& out
	) -> std::optional<std::string>;

	template <animatable F>
	auto bind_motion(
		const json::value& spec,
		std::string_view at,
		std::size_t field
	) -> std::expected<canvas_motion, std::string>;

	template <typename T>
	auto bind_motions(
		const json::value& node,
		std::string_view at,
		std::vector<canvas_motion>& out
	) -> std::optional<std::string>;

	template <typename T>
	auto bind_member(
		std::string_view key,
		const json::value& value,
		std::string_view at,
		T& out
	) -> std::optional<std::string>;

	template <typename T>
	auto bind_drawable(
		const json::value& node,
		std::string_view at,
		std::vector<canvas_step>& out
	) -> std::optional<std::string>;

	template <std::meta::info Sketch>
	auto bind_sketch(
		const json::value& node,
		std::string_view at,
		std::vector<canvas_step>& out
	) -> std::optional<std::string>;

	auto bind_step(
		const json::value& node,
		std::string_view at,
		std::vector<canvas_step>& out
	) -> std::optional<std::string>;

	template <std::meta::info Field>
	consteval auto field_index() -> std::size_t;

	template <std::meta::info Field>
	auto motion_of(
		const auto& from,
		const gui::motion_spec& spec
	) -> canvas_motion;

	auto tint(
		vec4f color,
		float alpha
	) -> vec4f;

	template <typename T>
	auto field_list() -> std::string;

	template <typename T>
	auto apply_motions(
		const canvas_frame& frame,
		std::size_t index,
		std::span<const canvas_motion> motions,
		T& shape
	) -> void;

	auto to_screen(
		const canvas_frame& frame,
		vec2f point
	) -> vec2f;

	auto draw_shape(
		const canvas_frame& frame,
		const drawables::rect& shape
	) -> void;

	auto draw_shape(
		const canvas_frame& frame,
		const drawables::circle& shape
	) -> void;

	auto draw_shape(
		const canvas_frame& frame,
		const drawables::line& shape
	) -> void;

	auto draw_shape(
		const canvas_frame& frame,
		const drawables::text& shape
	) -> void;

	auto draw_shape(
		const canvas_frame& frame,
		const drawables::label& shape
	) -> void;

	auto overflows(
		const canvas_frame& frame,
		const drawables::label& shape
	) -> bool;

	auto draw_shape(
		const canvas_frame& frame,
		const drawables::chrome_gallery& shape
	) -> void;
}

template <gse::ide::agent::animatable F>
auto gse::ide::agent::pack(const F& value) -> vec4f {
	if constexpr (std::same_as<F, float>) {
		return { value, 0.f, 0.f, 0.f };
	}
	else if constexpr (std::same_as<F, vec2f>) {
		return { value[0], value[1], 0.f, 0.f };
	}
	else {
		return value;
	}
}

template <gse::ide::agent::animatable F>
auto gse::ide::agent::unpack(const vec4f& packed, F& out) -> void {
	if constexpr (std::same_as<F, float>) {
		out = packed[0];
	}
	else if constexpr (std::same_as<F, vec2f>) {
		out = { packed[0], packed[1] };
	}
	else {
		out = packed;
	}
}

auto gse::ide::agent::leaf_text(const json::value& node) -> std::optional<std::string> {
	if (node.is_string()) {
		return std::string(node.text());
	}
	if (node.is_boolean()) {
		return std::string(node.boolean() ? "true" : "false");
	}
	if (node.is_number()) {
		return node.is_integer() ? std::format("{}", node.integer()) : std::format("{}", node.number());
	}
	if (!node.is_array()) {
		return std::nullopt;
	}

	std::string joined;
	for (const json::value& element : node.elements()) {
		if (element.is_array() || element.is_object()) {
			return std::nullopt;
		}
		if (!joined.empty()) {
			joined += ',';
		}
		joined += *leaf_text(element);
	}
	return joined;
}

template <typename F>
auto gse::ide::agent::bind_field(const json::value& node, const std::string_view at, F& out) -> std::optional<std::string> {
	const std::optional<std::string> text = leaf_text(node);
	if (text && gse::parse(*text, out)) {
		return std::nullopt;
	}
	return std::format("{}: expected {}, got {}", at, meta::unqualified_name<F>(), json::write(node));
}

template <gse::ide::agent::animatable F>
auto gse::ide::agent::bind_motion(const json::value& spec, const std::string_view at, const std::size_t field) -> std::expected<canvas_motion, std::string> {
	canvas_motion motion{ .field = field };
	bool has_from = false;

	const std::span<const std::string> keys = spec.keys();
	const std::span<const json::value> values = spec.elements();
	for (std::size_t s = 0; s < keys.size(); ++s) {
		const std::string key_at = std::format("{}.{}", at, keys[s]);
		std::optional<std::string> failure;
		if (keys[s] == "from") {
			F from{};
			failure = bind_field(values[s], key_at, from);
			motion.from = pack(from);
			has_from = true;
		}
		else if (keys[s] == "duration") {
			failure = bind_field(values[s], key_at, motion.spec.duration);
		}
		else if (keys[s] == "ease") {
			failure = bind_field(values[s], key_at, motion.spec.curve);
		}
		else {
			failure = std::format("{}: unknown key; an animation takes from, duration and ease", key_at);
		}
		if (failure) {
			return std::unexpected(std::move(*failure));
		}
	}

	if (!has_from) {
		return std::unexpected(std::format("{}: needs a 'from' value; the field's own value is where it ends", at));
	}
	return motion;
}

template <typename T>
auto gse::ide::agent::bind_motions(const json::value& node, const std::string_view at, std::vector<canvas_motion>& out) -> std::optional<std::string> {
	if (!node.is_object()) {
		return std::format("{}: expected an object of field: {{from, duration, ease}}, got {}", at, json::write(node));
	}

	const std::span<const std::string> keys = node.keys();
	const std::span<const json::value> values = node.elements();
	for (std::size_t k = 0; k < keys.size(); ++k) {
		const std::string field_at = std::format("{}.{}", at, keys[k]);
		std::optional<std::string> failure = std::format("{}: {} has no field '{}'", field_at, meta::unqualified_name<T>(), keys[k]);
		std::size_t index = 0;

		template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
			using field_type = [:std::meta::type_of(m):];
			const std::size_t field = index++;
			if (std::meta::identifier_of(m) == keys[k]) {
				if constexpr (!animatable<field_type>) {
					failure = std::format("{}: '{}' is a {} and cannot animate; only float, vec2f and vec4f fields can", field_at, keys[k], meta::unqualified_name<field_type>());
				}
				else {
					std::expected<canvas_motion, std::string> bound = bind_motion<field_type>(values[k], field_at, field);
					if (!bound) {
						return std::move(bound.error());
					}
					out.push_back(*bound);
					failure.reset();
				}
			}
		}

		if (failure) {
			return failure;
		}
	}
	return std::nullopt;
}

template <typename T>
auto gse::ide::agent::bind_member(const std::string_view key, const json::value& value, const std::string_view at, T& out) -> std::optional<std::string> {
	std::optional<std::string> failure = std::format("{}: {} has no field '{}'", at, meta::unqualified_name<T>(), key);
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		if (std::meta::identifier_of(m) == key) {
			failure = bind_field(value, at, out.[:m:]);
		}
	}
	return failure;
}

template <typename T>
auto gse::ide::agent::bind_drawable(const json::value& node, const std::string_view at, std::vector<canvas_step>& out) -> std::optional<std::string> {
	T shape{};
	std::vector<canvas_motion> motions;

	const std::span<const std::string> keys = node.keys();
	const std::span<const json::value> values = node.elements();
	for (std::size_t k = 0; k < keys.size(); ++k) {
		if (keys[k] == "draw") {
			continue;
		}

		const std::string field_at = std::format("{}.{}", at, keys[k]);
		std::optional<std::string> failure = keys[k] == "animate"
			? bind_motions<T>(values[k], field_at, motions)
			: bind_member(keys[k], values[k], field_at, shape);
		if (failure) {
			return failure;
		}
	}

	out.push_back({
		.shape = std::move(shape),
		.motions = std::move(motions),
	});
	return std::nullopt;
}

template <std::meta::info Sketch>
auto gse::ide::agent::bind_sketch(const json::value& node, const std::string_view at, std::vector<canvas_step>& out) -> std::optional<std::string> {
	using params_type = std::remove_cvref_t<typename [:std::meta::type_of(std::meta::parameters_of(Sketch)[0]):]>;
	params_type params{};

	const std::span<const std::string> keys = node.keys();
	const std::span<const json::value> values = node.elements();
	for (std::size_t k = 0; k < keys.size(); ++k) {
		if (keys[k] == "sketch") {
			continue;
		}
		if (std::optional<std::string> failure = bind_member(keys[k], values[k], std::format("{}.{}", at, keys[k]), params)) {
			return failure;
		}
	}

	constexpr std::string_view origin = std::define_static_string(std::meta::identifier_of(Sketch));
	for (canvas_step& step : [:Sketch:](params)) {
		step.origin = origin;
		out.push_back(std::move(step));
	}
	return std::nullopt;
}

auto gse::ide::agent::bind_step(const json::value& node, const std::string_view at, std::vector<canvas_step>& out) -> std::optional<std::string> {
	if (!node.is_object()) {
		return std::format("{}: expected a command object, got {}", at, json::write(node));
	}

	if (const json::value* named = node.find("sketch")) {
		if (!named->is_string()) {
			return std::format("{}.sketch: expected a sketch name, got {}", at, json::write(*named));
		}
		template for (constexpr auto s : sketch_list) {
			if (std::meta::identifier_of(s) == named->text()) {
				return bind_sketch<s>(node, at, out);
			}
		}
		return std::format("{}.sketch: no sketch named '{}' (gse_draw list: true shows them)", at, named->text());
	}

	const json::value* kind = node.find("draw");
	if (!kind || !kind->is_string()) {
		return std::format("{}.draw: missing; name a drawable, or a sketch with 'sketch' (gse_draw list: true shows both)", at);
	}

	template for (constexpr auto t : drawable_list) {
		if (std::meta::identifier_of(t) == kind->text()) {
			return bind_drawable<typename [:t:]>(node, at, out);
		}
	}
	return std::format("{}.draw: no drawable named '{}' (gse_draw list: true shows drawables and sketches)", at, kind->text());
}

template <std::meta::info Field>
consteval auto gse::ide::agent::field_index() -> std::size_t {
	const std::vector<std::meta::info> members = std::meta::nonstatic_data_members_of(std::meta::parent_of(Field), std::meta::access_context::unchecked());
	return static_cast<std::size_t>(std::distance(members.begin(), std::ranges::find(members, Field)));
}

template <std::meta::info Field>
auto gse::ide::agent::motion_of(const auto& from, const gui::motion_spec& spec) -> canvas_motion {
	return {
		.field = field_index<Field>(),
		.from = pack(static_cast<const typename [:std::meta::type_of(Field):]&>(from)),
		.spec = spec,
	};
}

auto gse::ide::agent::tint(const vec4f color, const float alpha) -> vec4f {
	return { color[0], color[1], color[2], color[3] * alpha };
}

auto gse::ide::agent::sketches::flow(const flow_params& params) -> std::vector<canvas_step> {
	const float left = params.position[0];
	const float top = params.position[1];
	const float mid = top + params.box[1] * 0.5f;
	const float right_left = left + params.box[0] + params.gap;
	const vec2f start = { left + params.box[0], mid };
	const vec2f end = { right_left, mid };
	const gui::motion_spec draw_in{ .duration = milliseconds(600.f), .curve = ease_kind::cubic_out };

	return {
		{ .shape = drawables::rect{ .position = params.position, .size = params.box, .color = tint(params.color, 0.25f), .radius = 6.f } },
		{ .shape = drawables::rect{ .position = { right_left, top }, .size = params.box, .color = tint(params.color, 0.25f), .radius = 6.f } },
		{ .shape = drawables::label{ .position = params.position, .size = params.box, .value = params.from, .align = gui::layout::halign::center } },
		{ .shape = drawables::label{ .position = { right_left, top }, .size = params.box, .value = params.to, .align = gui::layout::halign::center } },
		{
			.shape = drawables::line{ .from = start, .to = end, .color = params.color },
			.motions = { motion_of<^^drawables::line::to>(start, draw_in) },
		},
		{
			.shape = drawables::circle{ .center = end, .radius = 5.f, .color = params.color },
			.motions = { motion_of<^^drawables::circle::center>(start, draw_in) },
		},
	};
}

auto gse::ide::agent::sketches::before_after(const before_after_params& params) -> std::vector<canvas_step> {
	constexpr float divider_gap = 16.f;
	constexpr float label_inset = 12.f;
	const float left = params.position[0];
	const float top = params.position[1];
	const float after_left = left + params.panel[0] + divider_gap;
	const float divider_x = left + params.panel[0] + divider_gap * 0.5f;
	constexpr float label_h = 24.f;
	const float label_y = top + label_inset;
	const vec2f label_size = { std::max(0.f, params.panel[0] - label_inset * 2.f), label_h };
	const vec4f after_fill = tint(params.after_color, 0.25f);

	return {
		{ .shape = drawables::rect{ .position = params.position, .size = params.panel, .color = tint(params.before_color, 0.25f), .radius = 6.f } },
		{
			.shape = drawables::rect{ .position = { after_left, top }, .size = params.panel, .color = after_fill, .radius = 6.f },
			.motions = { motion_of<^^drawables::rect::color>(tint(after_fill, 0.f), { .duration = milliseconds(500.f), .curve = ease_kind::smoothstep }) },
		},
		{ .shape = drawables::line{ .from = { divider_x, top }, .to = { divider_x, top + params.panel[1] }, .color = { 1.f, 1.f, 1.f, 0.35f }, .thickness = 1.f } },
		{ .shape = drawables::label{ .position = { left + label_inset, label_y }, .size = label_size, .value = params.before, .color = params.before_color } },
		{ .shape = drawables::label{ .position = { after_left + label_inset, label_y }, .size = label_size, .value = params.after, .color = params.after_color } },
	};
}

auto gse::ide::agent::parse_canvas(const std::string_view source) -> std::expected<std::vector<canvas_step>, std::string> {
	const std::expected<json::value, json::parse_error> document = json::parse(source);
	if (!document) {
		return std::unexpected(std::format("payload: {} at byte {}", json::message(document.error().code), document.error().offset));
	}
	if (!document->is_array()) {
		return std::unexpected(std::format("payload: expected an array of commands, got {}", json::write(*document)));
	}

	std::vector<canvas_step> steps;
	steps.reserve(document->size());
	for (std::size_t i = 0; i < document->size(); ++i) {
		if (std::optional<std::string> failure = bind_step(document->elements()[i], std::format("payload[{}]", i), steps)) {
			return std::unexpected(std::move(*failure));
		}
	}
	return steps;
}

template <typename T>
auto gse::ide::agent::apply_motions(const canvas_frame& frame, const std::size_t index, const std::span<const canvas_motion> motions, T& shape) -> void {
	for (const canvas_motion& motion : motions) {
		std::size_t field = 0;
		template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
			using field_type = [:std::meta::type_of(m):];
			if constexpr (animatable<field_type>) {
				if (field == motion.field) {
					const id key = gui::ids::make_from_key(hash_of(frame.canvas.number(), index, field));
					unpack(frame.ctx.motion.sample(key, gui::motion_channel::value, pack(shape.[:m:]), motion.from, motion.spec), shape.[:m:]);
				}
			}
			++field;
		}
	}
}

auto gse::ide::agent::to_screen(const canvas_frame& frame, const vec2f point) -> vec2f {
	return { frame.area.left() + point[0] * frame.scale, frame.area.top() - point[1] * frame.scale };
}

auto gse::ide::agent::draw_shape(const canvas_frame& frame, const drawables::rect& shape) -> void {
	frame.ctx.queue_sprite({
		.rect = rectf::from_position_size(to_screen(frame, shape.position), shape.size * frame.scale),
		.color = shape.color,
		.texture = frame.ctx.blank_texture,
		.clip_rect = frame.clip,
		.corner_radius = { shape.radius * frame.scale, shape.radius * frame.scale, shape.radius * frame.scale, shape.radius * frame.scale },
	});
}

auto gse::ide::agent::draw_shape(const canvas_frame& frame, const drawables::circle& shape) -> void {
	const vec2f center = to_screen(frame, shape.center);
	const float radius = shape.radius * frame.scale;
	frame.ctx.queue_sprite({
		.rect = rectf::from_position_size({ center[0] - radius, center[1] + radius }, { radius * 2.f, radius * 2.f }),
		.color = shape.color,
		.texture = frame.ctx.blank_texture,
		.clip_rect = frame.clip,
		.corner_radius = { radius, radius, radius, radius },
	});
}

auto gse::ide::agent::draw_shape(const canvas_frame& frame, const drawables::line& shape) -> void {
	queue_line(frame.ctx, to_screen(frame, shape.from), to_screen(frame, shape.to), shape.thickness * frame.scale, shape.color, frame.clip);
}

auto gse::ide::agent::queue_line(const gui::draw_context& ctx, const vec2f from, const vec2f to, const float thickness, const vec4f& color, const rectf& clip) -> void {
	const vec2f delta = to - from;
	const float length = std::hypot(delta[0], delta[1]);
	const vec2f center = (from + to) * 0.5f;

	ctx.queue_sprite({
		.rect = rectf::from_position_size({ center[0] - length * 0.5f, center[1] + thickness * 0.5f }, { length, thickness }),
		.color = color,
		.texture = ctx.blank_texture,
		.clip_rect = clip,
		.rotation = radians(std::atan2(delta[1], delta[0])),
	});
}

auto gse::ide::agent::draw_shape(const canvas_frame& frame, const drawables::text& shape) -> void {
	const gui::style& sty = frame.ctx.style;
	const auto face = frame.ctx.fonts.text.resolve();
	const float scale = sty.font_size * shape.scale * frame.scale;
	const vec2f anchor_point = to_screen(frame, shape.position);
	const float line_height = face->line_height(scale);
	const float left = anchor_point[0] - face->width(shape.value, scale) * shape.anchor[0];
	const float top = anchor_point[1] + line_height * shape.anchor[1];

	frame.ctx.queue_text({
		.font = frame.ctx.fonts.text,
		.text = frame.ctx.intern(shape.value),
		.position = { left, top - line_height * 0.5f + face->vertical_center_offset(scale) },
		.scale = scale,
		.color = shape.color,
		.clip_rect = frame.clip,
	});
}

auto gse::ide::agent::draw_shape(const canvas_frame& frame, const drawables::label& shape) -> void {
	const rectf box = rectf::from_position_size(to_screen(frame, shape.position), shape.size * frame.scale);
	gui::draw::label(frame.ctx, box, frame.ctx.intern(shape.value), {
		.face = gui::text_face::text,
		.h = shape.align,
		.size_mult = shape.scale * frame.scale,
	}, {
		.color = shape.color,
		.clip_rect = box.intersection(frame.clip),
	});
}

auto gse::ide::agent::overflows(const canvas_frame& frame, const drawables::label& shape) -> bool {
	const auto face = frame.ctx.fonts.text.resolve();
	return face->width(shape.value, frame.ctx.style.font_size * shape.scale * frame.scale) > shape.size[0] * frame.scale;
}

auto gse::ide::agent::draw_shape(const canvas_frame& frame, const drawables::chrome_gallery& shape) -> void {
	draw_chrome_gallery(frame, shape);
}

auto gse::ide::agent::draw_canvas(const canvas_frame& frame, const std::span<const canvas_step> steps) -> canvas_tally {
	const std::size_t sprites_before = frame.ctx.sprites.size();
	const std::size_t texts_before = frame.ctx.texts.size();
	std::uint32_t clipped = 0;

	for (std::size_t i = 0; i < steps.size(); ++i) {
		const canvas_step& step = steps[i];
		std::visit([&](const auto& shape) {
			if constexpr (std::same_as<std::remove_cvref_t<decltype(shape)>, drawables::label>) {
				clipped += overflows(frame, shape) ? 1 : 0;
			}
			if (step.motions.empty()) {
				draw_shape(frame, shape);
				return;
			}
			auto moving = shape;
			apply_motions(frame, i, step.motions, moving);
			draw_shape(frame, moving);
		}, step.shape);
	}

	const auto emitted = std::span(frame.ctx.sprites).subspan(sprites_before);
	const auto dropped = std::ranges::count_if(emitted, [](const renderer::sprite_command& cmd) {
		const bool untextured = !cmd.texture.valid() && !cmd.image_slot.valid();
		const bool empty = cmd.rect.width() <= 0.f || cmd.rect.height() <= 0.f;
		const bool clipped = cmd.clip_rect.has_value() && !cmd.clip_rect->intersects(cmd.rect);
		return untextured || empty || clipped;
	});
	return {
		.sprites = static_cast<std::uint32_t>(emitted.size()),
		.texts = static_cast<std::uint32_t>(frame.ctx.texts.size() - texts_before),
		.dropped = static_cast<std::uint32_t>(dropped),
		.clipped = clipped,
	};
}

template <typename T>
auto gse::ide::agent::field_list() -> std::string {
	std::string fields;
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		if (!fields.empty()) {
			fields += ", ";
		}
		fields += std::format("{}: {}", std::meta::identifier_of(m), meta::unqualified_name<typename [:std::meta::type_of(m):]>());
	}
	return fields;
}

auto gse::ide::agent::drawable_catalogue() -> std::span<const drawable_entry> {
	static const std::vector<drawable_entry> table = [] {
		std::vector<drawable_entry> built;
		template for (constexpr auto t : drawable_list) {
			built.push_back({
				.name = std::define_static_string(std::meta::identifier_of(t)),
				.fields = field_list<typename [:t:]>(),
			});
		}
		template for (constexpr auto s : sketch_list) {
			using params_type = std::remove_cvref_t<typename [:std::meta::type_of(std::meta::parameters_of(s)[0]):]>;
			constexpr auto ann = first_annotation_of_type(s, ^^sketch);
			static constexpr sketch declared = [:std::meta::constant_of(ann):];
			built.push_back({
				.name = std::define_static_string(std::meta::identifier_of(s)),
				.fields = field_list<params_type>(),
				.summary = declared.summary,
				.sketch = true,
			});
		}
		return built;
	}();
	return table;
}
