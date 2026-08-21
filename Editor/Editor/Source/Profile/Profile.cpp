module gse.ide.profile;

import std;
import gse;

namespace gse::ide {
	constexpr std::size_t max_history_frames = 600;
	constexpr std::size_t max_history_nodes = 400000;
	constexpr double over_budget_share = 25.0;
	constexpr std::size_t profile_column_count = std::define_static_array(std::meta::enumerators_of(^^profile_column)).size();
	constexpr std::size_t min_strip_frames = 16;
	constexpr std::size_t number_buffer_size = 32;
	constexpr std::size_t label_buffer_size = 192;
	constexpr float min_bar_width = 1.f;
	constexpr float bar_label_min_width = 36.f;
	constexpr float max_name_column_ratio = 0.75f;

	constexpr vec4f color_main_lane{ 0.26f, 0.62f, 0.86f, 0.85f };
	constexpr vec4f color_worker_lane{ 0.62f, 0.46f, 0.86f, 0.85f };
	constexpr vec4f color_gpu_lane{ 0.30f, 0.78f, 0.56f, 0.85f };
	constexpr vec4f color_over_budget{ 0.92f, 0.64f, 0.28f, 1.f };

	auto capture_frame(
		profile_system::data& d
	) -> void;

	auto adopt_recorded_frames(
		const gse::profile::report_file& report,
		std::deque<captured_frame>& out
	) -> void;

	auto frame_for(
		const std::deque<captured_frame>& frames,
		std::uint64_t pinned
	) -> const captured_frame*;

	auto lane_color(
		std::uint32_t tid,
		std::uint32_t main_tid
	) -> vec4f;

	auto lane_label(
		std::uint32_t tid,
		std::uint32_t main_tid,
		std::span<char> buffer
	) -> std::string_view;

	auto lane_rank(
		std::span<const std::uint32_t> tids,
		std::uint32_t tid
	) -> std::size_t;

	auto compute_depths(
		const captured_frame& frame,
		std::vector<std::uint32_t>& depth,
		std::vector<std::uint32_t>& stack
	) -> void;

	auto build_lane_buckets(
		const captured_frame& frame,
		lane_buckets& out
	) -> void;

	auto row_height(
		const gse::gui::draw_context& ctx
	) -> float;

	auto draw_label(
		const gse::gui::draw_context& ctx,
		const rectf& clip,
		vec2f position,
		std::string_view text,
		vec4f color
	) -> void;

	auto display_tag(
		std::string_view tag
	) -> std::string_view;

	auto fitting_prefix(
		const gse::gui::draw_context& ctx,
		std::string_view text,
		float max_width
	) -> std::size_t;

	auto elide_label(
		const gse::gui::draw_context& ctx,
		std::string_view text,
		float max_width,
		std::span<char> buffer
	) -> std::string_view;

	auto draw_wrapped_label(
		const gse::gui::draw_context& ctx,
		const rectf& rect,
		float y,
		std::string_view text,
		vec4f color
	) -> float;

	auto draw_stat(
		const gse::gui::draw_context& ctx,
		const rectf& rect,
		float y,
		std::string_view label,
		std::string_view value
	) -> float;

	struct pill_hit {
		float next_left = 0.f;
		bool pressed = false;
	};

	auto draw_pill(
		const gse::gui::draw_context& ctx,
		const rectf& row,
		float left,
		std::string_view caption,
		bool active
	) -> pill_hit;

	template <typename Enum>
	auto draw_enum_toggle(
		const gse::gui::draw_context& ctx,
		const rectf& row,
		float left,
		Enum& value
	) -> float;

	auto draw_header(
		gse::gui::builder& ui,
		const rectf& outer,
		profile_view_state& state
	) -> void;

	auto draw_history_strip(
		const gse::gui::draw_context& ctx,
		const rectf& rect,
		const std::deque<captured_frame>& frames,
		profile_view_state& state
	) -> void;

	auto body_scroll_region(
		gse::gui::builder& ui,
		const rectf& body,
		std::string_view id,
		float row_h
	) -> gse::gui::scroll_handle;

	auto draw_flame(
		gse::gui::draw_context& ctx,
		const rectf& rect,
		const captured_frame& frame,
		profile_view_state& state
	) -> void;

	auto column_duration(
		const profile_row& row,
		profile_column column
	) -> gse::profile::sample_time;

	auto row_precedes(
		const profile_row& a,
		const profile_row& b,
		profile_column column
	) -> bool;

	auto collect_live_rows(
		gse::profile::domain domain,
		std::vector<gse::profile::report_entry>& scratch,
		std::vector<profile_row>& out
	) -> void;

	auto collect_report_rows(
		std::span<const gse::profile::report_record> records,
		std::vector<profile_row>& out
	) -> void;

	auto refresh_rows(
		const gse::profile::report_file& report,
		profile_view_state& state
	) -> void;

	auto find_row(
		const profile_view_state& state,
		gse::id id
	) -> const profile_row*;

	auto frame_share(
		const profile_row& row,
		gse::profile::sample_time frame_time
	) -> double;

	auto format_cell(
		profile_column column,
		const profile_row& row,
		gse::profile::sample_time frame_time,
		std::string_view unit,
		std::span<char> buffer
	) -> std::string_view;

	auto column_captions(
		std::string_view unit,
		std::span<std::array<char, number_buffer_size>> storage,
		std::span<std::string_view> out
	) -> void;

	auto measure_columns(
		const gse::gui::draw_context& ctx,
		std::span<const profile_row> rows,
		gse::profile::sample_time frame_time,
		std::string_view unit,
		std::span<float> widths
	) -> void;

	auto measure_name_column(
		const gse::gui::draw_context& ctx,
		std::span<const profile_row> rows
	) -> float;

	auto draw_row_section(
		const gse::gui::draw_context& ctx,
		const rectf& rect,
		const rectf& content,
		float y,
		std::string_view title,
		std::span<const profile_row> rows,
		gse::profile::sample_time frame_time,
		profile_view_state& state
	) -> float;

	auto draw_table(
		gse::gui::draw_context& ctx,
		const rectf& rect,
		profile_view_state& state,
		float scroll_x
	) -> float;

	auto draw_detail(
		const gse::gui::draw_context& ctx,
		const rectf& rect,
		const std::deque<captured_frame>& frames,
		const captured_frame* frame,
		profile_view_state& state
	) -> void;
}

auto gse::ide::capture_frame(profile_system::data& d) -> void {
	const trace::frame_view view = trace::view();
	if (view.generation == d.last_generation || view.roots.empty() || view.nodes.empty()) {
		return;
	}
	d.last_generation = view.generation;

	captured_frame frame;
	frame.nodes.assign(view.nodes.begin(), view.nodes.end());
	frame.children.assign(view.children.begin(), view.children.end());
	frame.roots.assign(view.roots.begin(), view.roots.end());
	frame.generation = view.generation;

	time_t<std::uint64_t> first = frame.nodes.front().start;
	time_t<std::uint64_t> last = first;
	for (const trace::node& n : frame.nodes) {
		if (n.start < first) {
			first = n.start;
		}
		if (!n.open && n.stop > last) {
			last = n.stop;
		}
	}
	frame.origin = first;
	frame.span = time_t<double>(last - first);

	d.frames.push_back(std::move(frame));

	std::size_t total_nodes = 0;
	for (const captured_frame& held : d.frames) {
		total_nodes += held.nodes.size();
	}
	while (d.frames.size() > max_history_frames || (total_nodes > max_history_nodes && d.frames.size() > 1)) {
		total_nodes -= d.frames.front().nodes.size();
		d.frames.pop_front();
	}
}

auto gse::ide::profile_system::run(context& ctx, data& d, const channel_read<profile_capture_request, profile_report_request> requests_in) -> async::task<> {
	for (const profile_capture_request& request : requests_in.of<profile_capture_request>()) {
		if (request.source != d.source) {
			d.source = request.source;
			d.frames.clear();
			d.last_generation = 0;
		}
		d.capturing = request.enabled;
	}
	for (const profile_report_request& request : requests_in.of<profile_report_request>()) {
		d.report = {};
		d.report_loaded = false;
		if (request.path.empty()) {
			continue;
		}
		std::expected<gse::profile::report_file, archive_mismatch> loaded = gse::profile::load_report(request.path);
		if (!loaded) {
			log::println(log::level::warning, log::category::general, "profile: could not read report '{}'", request.path);
			continue;
		}
		d.report = std::move(*loaded);
		d.report_loaded = true;
		adopt_recorded_frames(d.report, d.frames);
	}
	if (d.capturing && d.source == profile_source::editor) {
		capture_frame(d);
	}
	return {};
}

auto gse::ide::adopt_recorded_frames(const gse::profile::report_file& report, std::deque<captured_frame>& out) -> void {
	out.clear();

	std::vector<gse::id> ids;
	ids.reserve(report.tags.size());
	for (const std::string& tag : report.tags) {
		ids.push_back(gse::generate_id(tag));
	}

	for (const gse::profile::report_frame& recorded : report.recorded) {
		captured_frame frame;
		frame.generation = recorded.generation;
		frame.origin = recorded.origin;
		frame.span = recorded.span;
		frame.children = recorded.children;
		frame.roots = recorded.roots;
		frame.nodes.reserve(recorded.nodes.size());
		for (const gse::profile::report_node& node : recorded.nodes) {
			frame.nodes.push_back({
				.id = node.tag < ids.size() ? ids[node.tag] : gse::id{},
				.trace_id = node.trace_id,
				.start = node.start,
				.stop = node.stop,
				.self = node.self,
				.children_first = node.children_first,
				.children_count = node.children_count,
				.open = node.open,
			});
		}
		out.push_back(std::move(frame));
	}
}

auto gse::ide::frame_for(const std::deque<captured_frame>& frames, const std::uint64_t pinned) -> const captured_frame* {
	if (frames.empty()) {
		return nullptr;
	}
	if (pinned != 0) {
		for (const captured_frame& frame : frames) {
			if (frame.generation == pinned) {
				return &frame;
			}
		}
	}
	return &frames.back();
}

auto gse::ide::lane_color(const std::uint32_t tid, const std::uint32_t main_tid) -> vec4f {
	if (tid >= trace::gpu_virtual_tid_min) {
		return color_gpu_lane;
	}
	if (tid == (main_tid != 0 ? main_tid : trace::main_tid())) {
		return color_main_lane;
	}
	return color_worker_lane;
}

auto gse::ide::lane_label(const std::uint32_t tid, const std::uint32_t main_tid, const std::span<char> buffer) -> std::string_view {
	if (tid == (main_tid != 0 ? main_tid : trace::main_tid())) {
		return "main";
	}
	if (tid >= trace::gpu_virtual_tid_min) {
		if (const std::optional<std::string> name = trace::virtual_thread_name(tid)) {
			return format_into(buffer, "{}", *name);
		}
		return "gpu";
	}
	return format_into(buffer, "worker {}", tid);
}

auto gse::ide::lane_rank(const std::span<const std::uint32_t> tids, const std::uint32_t tid) -> std::size_t {
	return static_cast<std::size_t>(std::ranges::distance(tids.begin(), std::ranges::find(tids, tid)));
}

auto gse::ide::compute_depths(const captured_frame& frame, std::vector<std::uint32_t>& depth, std::vector<std::uint32_t>& stack) -> void {
	depth.assign(frame.nodes.size(), 0);
	stack.clear();
	for (const std::uint32_t root : frame.roots) {
		if (root < frame.nodes.size()) {
			stack.push_back(root);
		}
	}
	while (!stack.empty()) {
		const std::uint32_t index = stack.back();
		stack.pop_back();
		const trace::node& n = frame.nodes[index];
		for (std::uint32_t i = 0; i < n.children_count; ++i) {
			const std::uint32_t slot = n.children_first + i;
			if (slot >= frame.children.size()) {
				continue;
			}
			const std::uint32_t child = frame.children[slot];
			if (child >= frame.nodes.size() || depth[child] != 0) {
				continue;
			}
			depth[child] = depth[index] + 1;
			stack.push_back(child);
		}
	}
}

auto gse::ide::build_lane_buckets(const captured_frame& frame, lane_buckets& out) -> void {
	out.tids.clear();
	for (const trace::node& n : frame.nodes) {
		if (!std::ranges::contains(out.tids, n.trace_id)) {
			out.tids.push_back(n.trace_id);
		}
	}
	const std::uint32_t main = trace::main_tid();
	std::ranges::sort(out.tids, [main](const std::uint32_t a, const std::uint32_t b) {
		const bool a_gpu = a >= trace::gpu_virtual_tid_min;
		const bool b_gpu = b >= trace::gpu_virtual_tid_min;
		if (a_gpu != b_gpu) {
			return b_gpu;
		}
		if ((a == main) != (b == main)) {
			return a == main;
		}
		return a < b;
	});

	const std::size_t lane_count = out.tids.size();
	out.offsets.assign(lane_count + 1, 0);
	out.nodes.resize(frame.nodes.size());

	for (const trace::node& n : frame.nodes) {
		++out.offsets[lane_rank(out.tids, n.trace_id) + 1];
	}
	for (std::size_t rank = 0; rank < lane_count; ++rank) {
		out.offsets[rank + 1] += out.offsets[rank];
	}
	for (std::size_t i = 0; i < frame.nodes.size(); ++i) {
		const std::size_t rank = lane_rank(out.tids, frame.nodes[i].trace_id);
		out.nodes[out.offsets[rank]++] = static_cast<std::uint32_t>(i);
	}
	for (std::size_t rank = lane_count; rank > 0; --rank) {
		out.offsets[rank] = out.offsets[rank - 1];
	}
	out.offsets[0] = 0;
}

auto gse::ide::row_height(const gse::gui::draw_context& ctx) -> float {
	return ctx.fonts.code.resolve()->line_height(ctx.style.font_size) + ctx.style.padding * 0.5f;
}

auto gse::ide::draw_label(const gse::gui::draw_context& ctx, const rectf& clip, const vec2f position, const std::string_view text, const vec4f color) -> void {
	const auto code_view = ctx.fonts.code.resolve();
	ctx.queue_text({
		.font = ctx.fonts.code,
		.text = text,
		.position = { position.x(), position.y() + code_view->vertical_center_offset(ctx.style.font_size) },
		.scale = ctx.style.font_size,
		.color = color,
		.clip_rect = clip,
	});
}

auto gse::ide::display_tag(const std::string_view tag) -> std::string_view {
	const std::size_t marker = tag.rfind("#sz");
	return marker == std::string_view::npos ? tag : tag.substr(0, marker);
}

auto gse::ide::fitting_prefix(const gse::gui::draw_context& ctx, const std::string_view text, const float max_width) -> std::size_t {
	const auto code_view = ctx.fonts.code.resolve();
	const float font_sz = ctx.style.font_size;
	if (text.empty() || max_width <= 0.f) {
		return 0;
	}
	const float full = code_view->width(text, font_sz);
	if (full <= max_width) {
		return text.size();
	}
	std::size_t fit = std::min(text.size(), static_cast<std::size_t>(static_cast<float>(text.size()) * max_width / full));
	while (fit > 0 && code_view->width(text.substr(0, fit), font_sz) > max_width) {
		--fit;
	}
	return fit;
}

auto gse::ide::elide_label(const gse::gui::draw_context& ctx, const std::string_view text, const float max_width, const std::span<char> buffer) -> std::string_view {
	const auto code_view = ctx.fonts.code.resolve();
	constexpr std::string_view ellipsis = "...";
	if (code_view->width(text, ctx.style.font_size) <= max_width) {
		return text;
	}
	if (buffer.size() <= ellipsis.size()) {
		return {};
	}
	const std::size_t room = fitting_prefix(ctx, text, max_width - code_view->width(ellipsis, ctx.style.font_size));
	const std::size_t fit = std::min(room, buffer.size() - ellipsis.size());
	if (fit == 0) {
		return {};
	}
	std::ranges::copy_n(text.begin(), static_cast<std::ptrdiff_t>(fit), buffer.begin());
	std::ranges::copy(ellipsis, buffer.begin() + static_cast<std::ptrdiff_t>(fit));
	return { buffer.data(), fit + ellipsis.size() };
}

auto gse::ide::draw_wrapped_label(const gse::gui::draw_context& ctx, const rectf& rect, float y, const std::string_view text, const vec4f color) -> float {
	const auto code_view = ctx.fonts.code.resolve();
	const float pad = ctx.style.padding;
	const float row_h = code_view->line_height(ctx.style.font_size);
	const float max_width = rect.width() - pad;

	std::string_view rest = text;
	while (!rest.empty() && y - row_h > rect.bottom()) {
		const std::size_t fit = fitting_prefix(ctx, rest, max_width);
		if (fit == 0) {
			break;
		}
		draw_label(ctx, rect, { rect.left() + pad * 0.5f, y - row_h * 0.5f }, rest.substr(0, fit), color);
		rest.remove_prefix(fit);
		y -= row_h;
	}
	return y;
}

auto gse::ide::draw_stat(const gse::gui::draw_context& ctx, const rectf& rect, const float y, const std::string_view label, const std::string_view value) -> float {
	const auto code_view = ctx.fonts.code.resolve();
	const float font_sz = ctx.style.font_size;
	const float pad = ctx.style.padding;
	const float row_h = row_height(ctx);
	const float center_y = y - row_h * 0.5f;

	draw_label(ctx, rect, { rect.left() + pad * 0.5f, center_y }, label, ctx.style.color_text_secondary);
	draw_label(ctx, rect, { rect.right() - code_view->width(value, font_sz) - pad * 0.5f, center_y }, value, ctx.style.color_text);
	return y - row_h;
}

auto gse::ide::draw_pill(const gse::gui::draw_context& ctx, const rectf& row, const float left, const std::string_view caption, const bool active) -> pill_hit {
	const auto code_view = ctx.fonts.code.resolve();
	const float font_sz = ctx.style.font_size;
	const float pad = ctx.style.padding;

	const float w = code_view->width(caption, font_sz) + pad;
	const rectf cell = rectf::from_position_size({ left, row.top() }, { w, row.height() });
	const bool hovered = ctx.hovers(cell);
	ctx.queue_sprite({
		.rect = cell,
		.color = active ? ctx.style.color_tab_active : (hovered ? ctx.style.color_tab_hovered : ctx.style.color_tab_background),
		.texture = ctx.blank_texture,
		.clip_rect = row,
		.corner_radius = ctx.style.corner_radius,
	});
	draw_label(ctx, cell, { cell.left() + pad * 0.5f, cell.center().y() }, caption, active ? ctx.style.color_text : ctx.style.color_text_secondary);
	return {
		.next_left = left + w + pad * 0.25f,
		.pressed = ctx.mouse_pressed_for(cell),
	};
}

template <typename Enum>
auto gse::ide::draw_enum_toggle(const gse::gui::draw_context& ctx, const rectf& row, const float left, Enum& value) -> float {
	float x = left;
	template for (constexpr auto enumerator : std::define_static_array(std::meta::enumerators_of(^^Enum))) {
		constexpr Enum option = [:enumerator:];
		const pill_hit hit = draw_pill(ctx, row, x, gse::enum_to_string(option), value == option);
		if (hit.pressed) {
			value = option;
		}
		x = hit.next_left;
	}
	return x;
}

auto gse::ide::draw_header(gse::gui::builder& ui, const rectf& outer, profile_view_state& state) -> void {
	const gse::gui::draw_context& ctx = ui.ctx;
	const auto code_view = ctx.fonts.code.resolve();
	const float font_sz = ctx.style.font_size;
	const float pad = ctx.style.padding;
	const rectf rect = rectf::from_position_size(
		{ outer.left(), outer.top() - pad * 0.5f },
		{ outer.width(), std::max(1.f, outer.height() - pad) }
	);

	constexpr std::size_t source_count = std::define_static_array(std::meta::enumerators_of(^^profile_source)).size();
	std::array<std::string_view, source_count> options{};
	float widest = 0.f;
	std::size_t index = 0;
	template for (constexpr auto enumerator : std::define_static_array(std::meta::enumerators_of(^^profile_source))) {
		options[index] = gse::enum_to_string([:enumerator:]);
		widest = std::max(widest, code_view->width(options[index], font_sz));
		++index;
	}

	const float dropdown_w = widest + ctx.style.icon_extent + pad * 2.f;
	const rectf dropdown_rect = rectf::from_position_size(
		{ rect.right() - dropdown_w - pad * 0.5f, rect.top() },
		{ dropdown_w, rect.height() }
	);
	const gse::gui::dropdown_result picked = gse::gui::draw::dropdown_in_rect(
		ctx,
		"profile.source",
		static_cast<std::size_t>(state.source),
		options,
		state.source_dropdown,
		dropdown_rect,
		ui.hot_widget_id,
		ui.active_widget_id
	);
	if (picked.changed) {
		state.source = static_cast<profile_source>(picked.new_index);
		state.pinned_generation = 0;
		state.live_generation = 0;
		state.selected = {};
		state.cpu_display.clear();
		state.gpu_display.clear();
		state.worker_offset = 0;
	}

	const std::span<const std::string_view> units = gse::internal::unit_names<gse::time_tag>();
	float widest_unit = 0.f;
	for (const std::string_view unit : units) {
		widest_unit = std::max(widest_unit, code_view->width(unit, font_sz));
	}
	const auto current_unit = std::ranges::find(units, state.time_unit);
	const float unit_w = widest_unit + ctx.style.icon_extent + pad * 2.f;
	const rectf unit_rect = rectf::from_position_size(
		{ dropdown_rect.left() - unit_w - pad * 0.5f, rect.top() },
		{ unit_w, rect.height() }
	);
	const gse::gui::dropdown_result unit_picked = gse::gui::draw::dropdown_in_rect(
		ctx,
		"profile.unit",
		current_unit == units.end() ? 0 : static_cast<std::size_t>(std::ranges::distance(units.begin(), current_unit)),
		units,
		state.unit_dropdown,
		unit_rect,
		ui.hot_widget_id,
		ui.active_widget_id
	);
	if (unit_picked.changed && unit_picked.new_index < units.size()) {
		state.time_unit = units[unit_picked.new_index];
		state.columns = {};
	}

	const float x = draw_enum_toggle(ctx, rect, rect.left() + pad * 0.5f, state.mode);
	if (state.source != profile_source::editor) {
		return;
	}
	if (draw_pill(ctx, rect, x + pad * 0.5f, state.enabled ? "stop" : "start", state.enabled).pressed) {
		state.enabled = !state.enabled;
	}
}

auto gse::ide::draw_history_strip(const gse::gui::draw_context& ctx, const rectf& rect, const std::deque<captured_frame>& frames, profile_view_state& state) -> void {
	ctx.queue_sprite({
		.rect = rect,
		.color = ctx.style.color_input_background,
		.texture = ctx.blank_texture,
	});
	if (frames.empty() || rect.width() <= 0.f) {
		return;
	}

	const time_t<double> budget = milliseconds(1000.0 / 60.0);

	if (ctx.hovers(rect) && !ctx.is_scroll_consumed()) {
		const vec2f wheel = ctx.scroll_delta();
		if (std::abs(wheel.y()) > 0.001f) {
			const std::size_t shown = state.strip_visible == 0 ? frames.size() : state.strip_visible;
			const double scaled = static_cast<double>(shown) * (wheel.y() > 0.f ? 0.8 : 1.25);
			state.strip_visible = std::clamp(static_cast<std::size_t>(scaled + 0.5), min_strip_frames, frames.size());
			ctx.consume_scroll();
		}
	}

	const std::size_t visible = state.strip_visible == 0
		? frames.size()
		: std::clamp(state.strip_visible, min_strip_frames, frames.size());
	const std::size_t first = frames.size() - visible;

	time_t<double> peak = budget;
	for (std::size_t i = first; i < frames.size(); ++i) {
		peak = std::max(peak, frames[i].span);
	}

	const auto count = static_cast<float>(visible);
	const float column_w = std::max(1.f, rect.width() / count);
	const bool dragging = ctx.mouse_held();

	if (ctx.mouse_pressed_for(rect, mouse_button::button_2)) {
		state.pinned_generation = 0;
	}

	for (std::size_t i = first; i < frames.size(); ++i) {
		const captured_frame& frame = frames[i];
		const float x = rect.left() + rect.width() * static_cast<float>(i - first) / count;
		const rectf column = rectf::from_position_size({ x, rect.top() }, { column_w, rect.height() });
		const auto ratio = static_cast<float>(frame.span / peak);
		const float h = std::max(1.f, rect.height() * std::clamp(ratio, 0.f, 1.f));
		const rectf bar = rectf::from_position_size({ x, rect.bottom() + h }, { column_w, h });
		const bool pinned = state.pinned_generation == frame.generation;
		ctx.queue_sprite({
			.rect = bar,
			.color = pinned ? ctx.style.color_text : (frame.span > budget ? color_over_budget : ctx.style.color_accent),
			.texture = ctx.blank_texture,
			.clip_rect = rect,
		});
		if (dragging && ctx.hovers(column)) {
			state.pinned_generation = frame.generation;
		}
	}
}

auto gse::ide::body_scroll_region(gse::gui::builder& ui, const rectf& body, const std::string_view id, const float row_h) -> gse::gui::scroll_handle {
	ui.ctx.layout_cursor = { body.left(), body.top() };
	return gse::gui::scroll_region(ui.ctx, {
		.id = id,
		.size = { body.width(), body.height() },
		.config = {
			.scrollbar_width = 6.f,
			.scrollbar_min_height = 20.f,
			.scroll_speed = row_h * 3.f,
			.smooth_factor = 0.2f,
			.auto_hide_scrollbar = true,
			.smooth_scrolling = true,
		},
	});
}

auto gse::ide::draw_flame(gse::gui::draw_context& ctx, const rectf& rect, const captured_frame& frame, profile_view_state& state) -> void {
	const auto code_view = ctx.fonts.code.resolve();
	const float font_sz = ctx.style.font_size;
	const float pad = ctx.style.padding;
	const float row_h = row_height(ctx);
	const time_t<double> total = frame.span;
	if (total <= time_t<double>{} || frame.nodes.empty() || rect.width() <= 0.f) {
		return;
	}

	compute_depths(frame, state.depth_scratch, state.stack_scratch);
	build_lane_buckets(frame, state.lanes);

	std::array<char, number_buffer_size> buffer{};
	float lane_top = ctx.layout_cursor.y();
	for (std::size_t rank = 0; rank < state.lanes.tids.size(); ++rank) {
		const std::uint32_t tid = state.lanes.tids[rank];
		const float rows_top = lane_top - row_h;
		if (lane_top > rect.bottom() && rows_top < rect.top()) {
			draw_label(ctx, rect, { rect.left() + pad * 0.5f, lane_top - row_h * 0.5f }, lane_label(tid, state.live_main_tid, buffer), ctx.style.color_text_secondary);
		}

		std::uint32_t drawable_depth = 0;
		for (std::uint32_t slot = state.lanes.offsets[rank]; slot < state.lanes.offsets[rank + 1]; ++slot) {
			const std::uint32_t index = state.lanes.nodes[slot];
			const trace::node& n = frame.nodes[index];
			const time_t<double> start(n.start - frame.origin);
			const time_t<double> stop = n.open ? total : std::max(start, time_t<double>(n.stop - frame.origin));
			const float x0 = rect.left() + static_cast<float>(start / total) * rect.width();
			const float x1 = rect.left() + static_cast<float>(stop / total) * rect.width();
			const float w = x1 - x0;
			if (w < min_bar_width) {
				continue;
			}
			drawable_depth = std::max(drawable_depth, state.depth_scratch[index]);
			const float bar_top = rows_top - static_cast<float>(state.depth_scratch[index]) * row_h;
			const rectf bar = rectf::from_position_size({ x0, bar_top }, { w, std::max(1.f, row_h - 1.f) });
			if (bar.bottom() > rect.top() || bar.top() < rect.bottom()) {
				continue;
			}

			vec4f color = lane_color(tid, state.live_main_tid);
			if (n.open) {
				color.w() *= 0.45f;
			}
			const bool hovered = ctx.hovers(bar);
			if (hovered) {
				state.hovered = n.id;
				color = ctx.style.color_widget_hovered;
			}
			if (n.id == state.selected) {
				color = ctx.style.color_accent;
			}
			ctx.queue_sprite({
				.rect = bar,
				.color = color,
				.texture = ctx.blank_texture,
				.clip_rect = rect,
			});
			if (ctx.mouse_pressed_for(bar)) {
				state.selected = n.id;
			}
			if (w > bar_label_min_width) {
				std::array<char, label_buffer_size> label_buffer{};
				const std::string_view label = elide_label(ctx, display_tag(n.id.tag()), w - pad * 0.75f, label_buffer);
				if (!label.empty()) {
					draw_label(ctx, bar, { bar.left() + pad * 0.25f, bar.center().y() }, label, ctx.style.color_text);
				}
			}
		}

		lane_top = rows_top - static_cast<float>(drawable_depth + 1) * row_h - pad * 0.5f;
	}
	ctx.layout_cursor.y() = lane_top;
}

auto gse::ide::column_duration(const profile_row& row, const profile_column column) -> gse::profile::sample_time {
	switch (column) {
		case profile_column::ema:
			return row.ema;
		case profile_column::peak:
			return row.peak;
		default:
			return row.per_frame;
	}
}

auto gse::ide::row_precedes(const profile_row& a, const profile_row& b, const profile_column column) -> bool {
	if (column == profile_column::calls) {
		return a.calls_per_frame < b.calls_per_frame;
	}
	return column_duration(a, column) < column_duration(b, column);
}

auto gse::ide::frame_share(const profile_row& row, const gse::profile::sample_time frame_time) -> double {
	return frame_time > gse::profile::sample_time{} ? row.per_frame / frame_time * 100.0 : 0.0;
}

auto gse::ide::format_cell(const profile_column column, const profile_row& row, const gse::profile::sample_time frame_time, const std::string_view unit, const std::span<char> buffer) -> std::string_view {
	if (gse::annotation_from_enum<view_label>(column, { .text = "" }).dimensioned) {
		return format_into(buffer, "{:.2f:{}!}", column_duration(row, column), unit);
	}
	if (column == profile_column::share) {
		return format_into(buffer, "{:.1f}", frame_share(row, frame_time));
	}
	return format_into(buffer, "{:.2f}", row.calls_per_frame);
}

auto gse::ide::column_captions(const std::string_view unit, const std::span<std::array<char, number_buffer_size>> storage, const std::span<std::string_view> out) -> void {
	std::size_t index = 0;
	template for (constexpr auto enumerator : std::define_static_array(std::meta::enumerators_of(^^profile_column))) {
		static constexpr view_label label = gse::annotation_from_enum<view_label>([:enumerator:], { .text = "" });
		out[index] = label.dimensioned
			? format_into(storage[index], "{} {}", std::string_view(label.text), unit)
			: std::string_view(label.text);
		++index;
	}
}

auto gse::ide::measure_columns(const gse::gui::draw_context& ctx, const std::span<const profile_row> rows, const gse::profile::sample_time frame_time, const std::string_view unit, const std::span<float> widths) -> void {
	const auto code_view = ctx.fonts.code.resolve();
	const float font_sz = ctx.style.font_size;
	std::array<char, number_buffer_size> buffer{};

	for (const profile_row& row : rows) {
		std::size_t index = 0;
		template for (constexpr auto enumerator : std::define_static_array(std::meta::enumerators_of(^^profile_column))) {
			const std::string_view text = format_cell([:enumerator:], row, frame_time, unit, buffer);
			widths[index] = std::max(widths[index], code_view->width(text, font_sz));
			++index;
		}
	}
}

auto gse::ide::measure_name_column(const gse::gui::draw_context& ctx, const std::span<const profile_row> rows) -> float {
	const auto code_view = ctx.fonts.code.resolve();
	const float font_sz = ctx.style.font_size;
	float widest = 0.f;

	for (const profile_row& row : rows) {
		widest = std::max(widest, code_view->width(display_tag(row.tag), font_sz));
	}
	return widest;
}

auto gse::ide::draw_row_section(const gse::gui::draw_context& ctx, const rectf& rect, const rectf& content, float y, const std::string_view title, const std::span<const profile_row> rows, const gse::profile::sample_time frame_time, profile_view_state& state) -> float {
	const auto code_view = ctx.fonts.code.resolve();
	const float font_sz = ctx.style.font_size;
	const float pad = ctx.style.padding;
	const float row_h = row_height(ctx);
	const float name_left = content.left() + pad * 0.5f;

	auto value_at = [&](const rectf& cell, const std::string_view text) {
		return cell.right() - code_view->width(text, font_sz) - pad * 0.5f;
	};

	const auto visible = [&](const float top) {
		return top <= rect.top() && top - row_h >= rect.bottom();
	};

	if (visible(y)) {
		draw_label(ctx, rect, { name_left, y - row_h * 0.5f }, title, ctx.style.color_section_header);
	}
	y -= row_h;

	if (rows.empty()) {
		if (visible(y)) {
			draw_label(ctx, rect, { name_left, y - row_h * 0.5f }, "no samples", ctx.style.color_text_secondary);
		}
		return y - row_h - pad * 0.5f;
	}

	if (visible(y)) {
		std::array<std::array<char, number_buffer_size>, profile_column_count> caption_storage{};
		std::array<std::string_view, profile_column_count> captions{};
		column_captions(state.time_unit, caption_storage, captions);
		const gse::gui::column_header_result picked = gse::gui::column_header(ctx, {
			.area = rectf::from_position_size({ content.left(), y }, { content.width(), row_h }),
			.captions = captions,
			.active = static_cast<std::size_t>(state.sort_column),
		}, state.columns);
		if (picked.activated) {
			const auto column = static_cast<profile_column>(picked.index);
			state.sort_descending = state.sort_column == column ? !state.sort_descending : true;
			state.sort_column = column;
		}
		state.wants_resize_cursor = state.wants_resize_cursor || picked.resize_cursor;
	}
	y -= row_h;

	std::array<char, number_buffer_size> buffer{};
	std::array<char, label_buffer_size> label_buffer{};
	for (const profile_row& row : rows) {
		if (!visible(y)) {
			y -= row_h;
			continue;
		}
		const rectf line = rectf::from_position_size({ content.left(), y }, { content.width(), row_h });
		if (row.id == state.selected) {
			ctx.queue_sprite({
				.rect = line,
				.color = ctx.style.color_widget_selected,
				.texture = ctx.blank_texture,
				.clip_rect = rect,
			});
		}
		const float center_y = line.center().y();
		const bool over_budget = frame_share(row, frame_time) >= over_budget_share;

		const float name_limit = gse::gui::column_cell(state.columns, line, 0).left() - name_left - pad * 0.5f;
		draw_label(ctx, rect, { name_left, center_y }, elide_label(ctx, display_tag(row.tag), name_limit, label_buffer), ctx.style.color_text);

		std::size_t column_index = 0;
		template for (constexpr auto enumerator : std::define_static_array(std::meta::enumerators_of(^^profile_column))) {
			constexpr profile_column column = [:enumerator:];
			const std::string_view text = format_cell(column, row, frame_time, state.time_unit, buffer);
			const vec4f color = column == profile_column::share && over_budget
				? color_over_budget
				: (column == profile_column::per_frame ? ctx.style.color_text : ctx.style.color_text_secondary);
			draw_label(ctx, rect, { value_at(gse::gui::column_cell(state.columns, line, column_index), text), center_y }, text, color);
			++column_index;
		}
		if (ctx.mouse_pressed_for(line)) {
			state.selected = row.id;
		}
		y -= row_h;
	}
	return y - pad * 0.5f;
}

auto gse::ide::collect_live_rows(const gse::profile::domain domain, std::vector<gse::profile::report_entry>& scratch, std::vector<profile_row>& out) -> void {
	gse::profile::build_report(domain, scratch);
	out.clear();
	out.reserve(scratch.size());
	for (const gse::profile::report_entry& row : scratch) {
		out.push_back({
			.id = row.id,
			.tag = row.id.tag(),
			.per_frame = row.per_frame,
			.ema = row.ema,
			.last = row.last,
			.peak = row.peak,
			.calls_per_frame = row.calls_per_frame,
			.sample_count = row.sample_count,
			.dominant_tid = row.dominant_tid,
		});
	}
}

auto gse::ide::collect_report_rows(const std::span<const gse::profile::report_record> records, std::vector<profile_row>& out) -> void {
	out.clear();
	out.reserve(records.size());
	for (const gse::profile::report_record& record : records) {
		out.push_back({
			.id = gse::generate_id(record.tag),
			.tag = record.tag,
			.per_frame = record.per_frame,
			.ema = record.ema,
			.last = record.last,
			.peak = record.peak,
			.calls_per_frame = record.calls_per_frame,
			.sample_count = record.sample_count,
			.dominant_tid = record.dominant_tid,
		});
	}
}

auto gse::ide::refresh_rows(const gse::profile::report_file& report, profile_view_state& state) -> void {
	if (state.source == profile_source::game) {
		collect_report_rows(report.cpu, state.cpu_display);
		collect_report_rows(report.gpu, state.gpu_display);
		state.live_frame_time = report.frame_time;
		state.live_main_tid = report.main_tid;
	}
	else if (state.enabled) {
		collect_live_rows(gse::profile::domain::cpu, state.cpu_rows, state.cpu_display);
		collect_live_rows(gse::profile::domain::gpu, state.gpu_rows, state.gpu_display);
		const auto fps = system_clock::fps();
		state.live_frame_time = fps > 0 ? milliseconds(1000.0 / static_cast<double>(fps)) : gse::profile::sample_time{};
		state.live_main_tid = trace::main_tid();
	}
	else {
		return;
	}

	const profile_column column = state.sort_column;
	const bool descending = state.sort_descending;
	const auto by_column = [column, descending](const profile_row& a, const profile_row& b) {
		return descending ? row_precedes(b, a, column) : row_precedes(a, b, column);
	};
	std::ranges::sort(state.cpu_display, by_column);
	std::ranges::sort(state.gpu_display, by_column);

	const std::uint32_t main_tid = state.live_main_tid;
	const auto workers = std::ranges::stable_partition(state.cpu_display, [main_tid](const profile_row& row) {
		return gse::profile::on_main_thread(row.dominant_tid, main_tid);
	});
	state.worker_offset = static_cast<std::size_t>(std::ranges::distance(state.cpu_display.begin(), workers.begin()));
}

auto gse::ide::find_row(const profile_view_state& state, const gse::id id) -> const profile_row* {
	for (const std::span<const profile_row> rows : { std::span<const profile_row>(state.cpu_display), std::span<const profile_row>(state.gpu_display) }) {
		const auto found = std::ranges::find(rows, id, &profile_row::id);
		if (found != rows.end()) {
			return &*found;
		}
	}
	return nullptr;
}

auto gse::ide::draw_table(gse::gui::draw_context& ctx, const rectf& rect, profile_view_state& state, const float scroll_x) -> float {
	const std::size_t offset = std::min(state.worker_offset, state.cpu_display.size());
	const std::span<const profile_row> all_cpu(state.cpu_display);
	const std::span<const profile_row> main_rows = all_cpu.first(offset);
	const std::span<const profile_row> worker_rows = all_cpu.subspan(offset);
	const gse::profile::sample_time frame_time = state.live_frame_time;

	const float pad = ctx.style.padding;

	if (gse::gui::needs_seed(state.columns, profile_column_count)) {
		const auto code_view = ctx.fonts.code.resolve();
		const float font_sz = ctx.style.font_size;

		std::array<std::array<char, number_buffer_size>, profile_column_count> caption_storage{};
		std::array<std::string_view, profile_column_count> captions{};
		column_captions(state.time_unit, caption_storage, captions);

		std::array<float, profile_column_count> widths{};
		for (std::size_t i = 0; i < profile_column_count; ++i) {
			widths[i] = code_view->width(captions[i], font_sz);
		}
		measure_columns(ctx, all_cpu, frame_time, state.time_unit, widths);
		measure_columns(ctx, state.gpu_display, frame_time, state.time_unit, widths);
		gse::gui::seed_columns(ctx, widths, state.columns);
	}

	const float name_width = std::min(
		rect.width() * max_name_column_ratio,
		std::max(measure_name_column(ctx, all_cpu), measure_name_column(ctx, state.gpu_display)) + pad
	);
	float content_width = name_width;
	for (const float width : state.columns.widths) {
		content_width += width;
	}

	const rectf content = rectf::from_position_size(
		{ rect.left() - scroll_x, rect.top() },
		{ std::max(rect.width(), content_width), rect.height() }
	);

	float y = ctx.layout_cursor.y();
	y = draw_row_section(ctx, rect, content, y, "cpu - main thread (blocks the frame)", main_rows, frame_time, state);
	y = draw_row_section(ctx, rect, content, y, "cpu - workers (parallel, can exceed 100%)", worker_rows, frame_time, state);
	y = draw_row_section(ctx, rect, content, y, "gpu (per pass)", state.gpu_display, frame_time, state);
	ctx.layout_cursor.y() = y;
	return content_width;
}

auto gse::ide::draw_detail(const gse::gui::draw_context& ctx, const rectf& rect, const std::deque<captured_frame>& frames, const captured_frame* frame, profile_view_state& state) -> void {
	const auto code_view = ctx.fonts.code.resolve();
	const float font_sz = ctx.style.font_size;
	const float pad = ctx.style.padding;
	const float row_h = row_height(ctx);

	ctx.queue_sprite({
		.rect = rect,
		.color = ctx.style.color_panel_alt,
		.texture = ctx.blank_texture,
	});

	std::array<char, number_buffer_size> buffer{};
	float y = rect.top() - pad * 0.5f;
	const gse::id focus = state.hovered.exists() ? state.hovered : state.selected;
	y = draw_wrapped_label(ctx, rect, y, focus.exists() ? focus.tag() : "no selection", ctx.style.color_text);

	if (focus.exists()) {
		if (const profile_row* row = find_row(state, focus)) {
			y = draw_stat(ctx, rect, y, "per/f", format_into(buffer, "{:.2f:{}}", row->per_frame, state.time_unit));
			y = draw_stat(ctx, rect, y, "calls/f", format_into(buffer, "{:.2f}", row->calls_per_frame));
			y = draw_stat(ctx, rect, y, "avg", format_into(buffer, "{:.2f:{}}", row->ema, state.time_unit));
			y = draw_stat(ctx, rect, y, "peak", format_into(buffer, "{:.2f:{}}", row->peak, state.time_unit));
			y = draw_stat(ctx, rect, y, "last", format_into(buffer, "{:.2f:{}}", row->last, state.time_unit));
			y = draw_stat(ctx, rect, y, "hits", format_into(buffer, "{}", row->sample_count));
		}
	}

	y -= pad * 0.5f;
	if (frame) {
		y = draw_stat(ctx, rect, y, "frame", format_into(buffer, "{:.2f:{}}", frame->span, state.time_unit));
		y = draw_stat(ctx, rect, y, "spans", format_into(buffer, "{}", frame->nodes.size()));
	}
	y = draw_stat(ctx, rect, y, "history", format_into(buffer, "{}", frames.size()));
	if (state.source == profile_source::editor) {
		y = draw_stat(ctx, rect, y, "dropped", format_into(buffer, "{}", trace::dropped_events()));
		y = draw_stat(ctx, rect, y, "abandoned", format_into(buffer, "{}", trace::abandoned_spans()));
	}
	if (state.pinned_generation != 0) {
		draw_label(ctx, rect, { rect.left() + pad * 0.5f, y - row_h * 0.5f }, "pinned (right click to resume)", ctx.style.color_accent);
	}
	else if (state.enabled) {
		draw_label(ctx, rect, { rect.left() + pad * 0.5f, y - row_h * 0.5f }, "live", ctx.style.color_accent);
	}

	const auto worst = std::ranges::max_element(frames, {}, &captured_frame::span);
	if (worst == frames.end()) {
		return;
	}
	const rectf button = rectf::from_position_size(
		{ rect.left() + pad * 0.5f, rect.bottom() + row_h + pad },
		{ std::max(0.f, rect.width() - pad), row_h }
	);
	const bool active = state.pinned_generation == worst->generation;
	const bool hovered = ctx.hovers(button);
	ctx.queue_sprite({
		.rect = button,
		.color = active ? ctx.style.color_tab_active : (hovered ? ctx.style.color_button_hovered : ctx.style.color_button_background),
		.texture = ctx.blank_texture,
		.clip_rect = rect,
		.corner_radius = ctx.style.corner_radius,
	});
	const std::string_view worst_value = format_into(buffer, "{:.2f:{}}", worst->span, state.time_unit);
	draw_label(ctx, button, { button.left() + pad * 0.5f, button.center().y() }, "worst frame", ctx.style.color_text);
	draw_label(ctx, button, { button.right() - code_view->width(worst_value, font_sz) - pad * 0.5f, button.center().y() }, worst_value, ctx.style.color_text_secondary);
	if (ctx.mouse_pressed_for(button)) {
		state.pinned_generation = worst->generation;
	}
}

auto gse::ide::draw_profile_panel(gse::gui::builder& ui, const rectf& rect, profile_view_state& state, const std::deque<captured_frame>& frames, const gse::profile::report_file& report, const bool report_loaded, gse::channel_write<gse::set_cursor_shape_request> channels) -> void {
	const gse::gui::draw_context& ctx = ui.ctx;
	const auto code_view = ctx.fonts.code.resolve();
	const float font_sz = ctx.style.font_size;
	const float pad = ctx.style.padding;
	const float row_h = row_height(ctx);

	state.hovered = {};

	const rectf header_rect = rectf::from_position_size(
		{ rect.left(), rect.top() },
		{ rect.width(), std::min(row_h + pad * 2.f, rect.height()) }
	);
	draw_header(ui, header_rect, state);

	const float strip_h = std::min(row_h * 3.f, std::max(0.f, (rect.height() - header_rect.height()) * 0.3f));
	const rectf strip_rect = rectf::from_position_size(
		{ rect.left(), header_rect.bottom() - pad * 0.25f },
		{ rect.width(), strip_h }
	);
	draw_history_strip(ctx, strip_rect, frames, state);

	const float body_h = std::max(0.f, rect.height() - header_rect.height() - strip_h - pad * 0.75f);
	const rectf split_area = rectf::from_position_size(
		{ rect.left(), strip_rect.bottom() - pad * 0.5f },
		{ rect.width(), body_h }
	);
	if (state.detail_width <= 0.f) {
		state.detail_width = std::min(code_view->width("pinned (right click to resume)", font_sz) + pad * 1.5f, rect.width() * 0.35f);
	}

	const float divider_thickness = std::max(6.f, ctx.style.resize_border_thickness) * 2.f;
	const gse::gui::layout::split_params split{
		.container = split_area,
		.axis = gse::gui::layout::split_axis::columns,
		.ratio = split_area.width() > 0.f ? std::clamp((split_area.width() - state.detail_width) / split_area.width(), 0.f, 1.f) : 0.7f,
		.min_first = 200.f,
		.min_second = 120.f,
		.divider_thickness = divider_thickness,
	};
	const vec2f mouse = ctx.mouse_position();
	const bool resize_blocked = ctx.hit_regions && ctx.hit_regions->is_resize_blocked(mouse);
	const gse::gui::layout::split_result panels = gse::gui::layout::update_split(
		split,
		{
			.mouse = mouse,
			.pressed = ctx.mouse_pressed(),
			.held = ctx.mouse_held(),
			.blocked = resize_blocked,
		},
		state.resizing_detail
	);
	const rectf body_rect = panels.first;
	const rectf detail_rect = panels.second;
	state.detail_width = detail_rect.width();

	state.wants_resize_cursor = (panels.divider.contains(mouse) && !resize_blocked) || state.resizing_detail.dragging;

	if (state.pinned_generation == 0 && !frames.empty() && (state.live_generation == 0 || state.live_timer.tick())) {
		state.live_generation = frames.back().generation;
	}

	refresh_rows(report, state);

	const captured_frame* frame = frame_for(frames, state.pinned_generation != 0 ? state.pinned_generation : state.live_generation);
	const bool have_data = state.source == profile_source::editor || report_loaded;
	if (have_data && state.mode == profile_mode::table) {
		gse::gui::scroll_handle region = body_scroll_region(ui, body_rect, "profile.table", row_h);
		const rectf view = region.valid() ? region.visible_rect() : body_rect;
		const float content_width = draw_table(ui.ctx, view, state, region.offset_x());
		region.set_content_width(content_width);
	}
	else if (have_data && frame) {
		const gse::gui::scroll_handle region = body_scroll_region(ui, body_rect, "profile.flame", row_h);
		draw_flame(ui.ctx, region.valid() ? region.visible_rect() : body_rect, *frame, state);
	}
	else {
		const auto text_view = ctx.fonts.text.resolve();
		const std::string_view message = state.source == profile_source::editor
			? "Press start to capture editor frames"
			: "The game report loads here when the game exits";
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = message,
			.position = {
				body_rect.center().x() - text_view->width(message, font_sz) * 0.5f,
				body_rect.center().y() + text_view->vertical_center_offset(font_sz),
			},
			.scale = font_sz,
			.color = ctx.style.color_text_secondary,
			.clip_rect = body_rect,
		});
	}

	draw_detail(ctx, detail_rect, frames, frame, state);

	if (state.wants_resize_cursor) {
		channels.push<set_cursor_shape_request>({
			.shape = cursor_shape::resize_ew,
		});
	}
}
