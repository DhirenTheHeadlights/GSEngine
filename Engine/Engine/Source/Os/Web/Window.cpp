module gse.os:window_impl;

import std;

import gse.core;
import gse.ecs;
import gse.log;
import gse.math;
import gse.meta;
import gse.time;

import :input_events;
import :keys;
import :window;

extern "C" auto gse_host_surface(std::int32_t* out) -> void;
extern "C" auto gse_host_input(void* out, std::int32_t max) -> std::int32_t;
extern "C" auto gse_host_wait(double milliseconds) -> void;
extern "C" auto gse_host_wake() -> void;
extern "C" auto gse_host_capture(std::int32_t want) -> void;

namespace gse::window {
	struct host_surface {
		vec2i size;
		bool visible = false;
		bool locked = false;
		std::int32_t resize_seq = 0;
		std::int32_t mark = 0;
	};

	enum struct host_input_kind : std::int32_t {
		key_down = 1,
		key_up = 2,
		button_down = 3,
		button_up = 4,
		move = 5,
		wheel = 6,
		text = 7,
		raw_move = 8,
	};

	struct host_input_record {
		host_input_kind kind;
		std::int32_t value;
		double x;
		double y;
		std::uint8_t code_length;
		std::array<char, 23> code;
	};

	static_assert(sizeof(host_input_record) == 48);

	struct dom_key_code {
		std::string_view code;
		key value;
	};

	constexpr std::size_t host_input_batch = 256;

	bool capture_requested = false;
	std::uint64_t tick_index = 0;
	std::int32_t seen_resize_seq = 0;
	std::int32_t seen_mark = 0;

	auto read_host_surface(
	) -> host_surface;

	consteval auto pascal_case(
		std::string_view snake
	) -> std::string;

	consteval auto unmappable_key(
		std::string_view name
	) -> bool;

	consteval auto dom_code_of(
		std::string_view name
	) -> std::string;

	consteval auto mapped_key_count(
	) -> std::size_t;

	auto key_from_dom_code(
		std::string_view code
	) -> std::optional<key>;

	auto log_key_codes(
	) -> void;

	auto push_event(
		window_surface& surface,
		const input::event& event
	) -> void;

	auto set_ui_focus(
		window_surface& surface,
		bool focus
	) -> void;

	auto handle_mouse_button(
		window_surface& surface,
		mouse_button button,
		double x,
		double y,
		bool pressed
	) -> void;

	auto handle_mouse_move(
		window_surface& surface,
		double x,
		double y
	) -> void;

	auto handle_text(
		window_surface& surface,
		std::uint32_t codepoint
	) -> void;

	auto handle_record(
		window_surface& surface,
		const host_input_record& record
	) -> void;

	auto drain_host_input(
		window_surface& surface
	) -> void;

	auto apply_cursor_mode(
		data& d,
		bool locked
	) -> void;
}

consteval auto gse::window::pascal_case(const std::string_view snake) -> std::string {
	std::string out;
	bool upper = true;
	for (const char c : snake) {
		if (c == '_') {
			upper = true;
			continue;
		}
		out.push_back(upper && c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c);
		upper = false;
	}
	return out;
}

consteval auto gse::window::unmappable_key(const std::string_view name) -> bool {
	return name == "unknown" || name == "world_1" || name == "f25";
}

consteval auto gse::window::dom_code_of(const std::string_view name) -> std::string {
	constexpr std::array<std::pair<std::string_view, std::string_view>, 7> renamed{ {
		{ "apostrophe", "Quote" },
		{ "grave_accent", "Backquote" },
		{ "del", "Delete" },
		{ "menu", "ContextMenu" },
		{ "left_super", "MetaLeft" },
		{ "right_super", "MetaRight" },
		{ "world_2", "IntlBackslash" },
	} };
	for (const auto& [from, to] : renamed) {
		if (name == from) {
			return std::string(to);
		}
	}

	const auto digit = [](const char c) {
		return c >= '0' && c <= '9';
	};

	if (name.size() == 1) {
		return "Key" + pascal_case(name);
	}
	if (name.starts_with("num_") && digit(name[4])) {
		return "Digit" + std::string(name.substr(4));
	}
	if (name.starts_with("kp_")) {
		return "Numpad" + pascal_case(name.substr(3));
	}
	if (name[0] == 'f' && digit(name[1])) {
		return "F" + std::string(name.substr(1));
	}
	if (name == "up" || name == "down" || name == "left" || name == "right") {
		return "Arrow" + pascal_case(name);
	}
	if (name.starts_with("left_")) {
		return pascal_case(name.substr(5)) + "Left";
	}
	if (name.starts_with("right_")) {
		return pascal_case(name.substr(6)) + "Right";
	}
	return pascal_case(name);
}

consteval auto gse::window::mapped_key_count() -> std::size_t {
	std::size_t count = 0;
	for (const auto e : std::meta::enumerators_of(^^key)) {
		if (!unmappable_key(std::meta::identifier_of(e))) {
			++count;
		}
	}
	return count;
}

namespace gse::window {
	constexpr auto dom_key_codes = [] consteval {
		std::array<dom_key_code, mapped_key_count()> out{};
		std::size_t at = 0;
		template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^key))) {
			if constexpr (!unmappable_key(std::meta::identifier_of(e))) {
				out[at++] = { std::define_static_string(dom_code_of(std::meta::identifier_of(e))), [:e:] };
			}
		}
		std::ranges::sort(out, {}, &dom_key_code::code);
		return out;
	}();

	static_assert(
		std::ranges::adjacent_find(dom_key_codes, {}, &dom_key_code::code) == dom_key_codes.end(),
		"two gse::key enumerators derive the same DOM code; add the odd one to dom_code_of's renamed table"
	);
}

auto gse::window::key_from_dom_code(const std::string_view code) -> std::optional<key> {
	const auto it = std::ranges::lower_bound(dom_key_codes, code, {}, &dom_key_code::code);
	if (it == dom_key_codes.end() || it->code != code) {
		return std::nullopt;
	}
	return it->value;
}

auto gse::window::log_key_codes() -> void {
	for (const auto& [code, value] : dom_key_codes) {
		log::println(log::level::debug, log::category::general, "keymap {} {}", code, enum_to_string(value));
	}
}

auto gse::window::push_event(window_surface& surface, const input::event& event) -> void {
	frame_demand::request_redraw();
	template for (constexpr auto alternative : std::define_static_array(std::views::iota(std::size_t{ 0 }, std::variant_size_v<input::event>))) {
		if (event.index() == alternative) {
			const auto& e = std::get<alternative>(event);
			using event_t = std::variant_alternative_t<alternative, input::event>;
			std::string line(std::meta::identifier_of(std::meta::dealias(^^event_t)));
			template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^event_t, std::meta::access_context::unchecked()))) {
				if constexpr (std::is_enum_v<typename [:std::meta::type_of(m):]>) {
					line += std::format(" {}", enum_to_string(e.[:m:]));
				}
				else {
					line += std::format(" {}", e.[:m:]);
				}
			}
			log::println(log::level::debug, log::category::general, "input {}", line);
		}
	}
	surface.input_events.push(event);
}

auto gse::window::set_ui_focus(window_surface& surface, const bool focus) -> void {
	const bool was = surface.ui_focus;
	surface.ui_focus = focus;

	if (was || !focus) {
		return;
	}

	const double center_x = surface.size.x() / 2.0;
	const double center_y = surface.size.y() / 2.0;
	push_event(surface, input::mouse_moved{ center_x, surface.size.y() - center_y });
}

auto gse::window::handle_mouse_button(window_surface& surface, const mouse_button button, const double x, double y, const bool pressed) -> void {
	const auto bit = static_cast<std::uint8_t>(1u << static_cast<int>(button));
	if (pressed) {
		surface.mouse_button_mask |= bit;
	}
	else {
		surface.mouse_button_mask &= static_cast<std::uint8_t>(~bit);
	}

	if (surface.ui_focus) {
		y = static_cast<double>(surface.size.y()) - y;
	}

	if (pressed) {
		push_event(surface, input::mouse_button_pressed{ button, x, y });
	}
	else {
		push_event(surface, input::mouse_button_released{ button, x, y });
	}
}

auto gse::window::handle_mouse_move(window_surface& surface, const double x, const double y) -> void {
	if (!surface.ui_focus) {
		push_event(surface, input::mouse_moved{ x, y });
		return;
	}

	const auto height = static_cast<double>(surface.size.y());

	if (surface.cursor_captured || (surface.mouse_button_mask & 1u) != 0) {
		push_event(surface, input::mouse_moved{ x, height - y });
		return;
	}

	const double clamped_x = std::clamp(x, 0.0, static_cast<double>(surface.size.x()));
	const double clamped_y = std::clamp(y, 0.0, height);
	push_event(surface, input::mouse_moved{ clamped_x, height - clamped_y });
}

auto gse::window::handle_text(window_surface& surface, const std::uint32_t codepoint) -> void {
	if (codepoint < 32 || (codepoint > 126 && codepoint < 160)) {
		return;
	}
	push_event(surface, input::text_entered{ codepoint });
}

auto gse::window::handle_record(window_surface& surface, const host_input_record& record) -> void {
	switch (record.kind) {
		case host_input_kind::key_down:
		case host_input_kind::key_up: {
			const auto mapped = key_from_dom_code({ record.code.data(), record.code_length });
			if (!mapped) {
				return;
			}
			if (record.kind == host_input_kind::key_down) {
				push_event(surface, input::key_pressed{ .key_code = *mapped });
			}
			else {
				push_event(surface, input::key_released{ .key_code = *mapped });
			}
			return;
		}
		case host_input_kind::button_down:
		case host_input_kind::button_up:
			handle_mouse_button(surface, static_cast<mouse_button>(record.value), record.x, record.y, record.kind == host_input_kind::button_down);
			return;
		case host_input_kind::move:
			handle_mouse_move(surface, record.x, record.y);
			return;
		case host_input_kind::wheel:
			push_event(surface, input::mouse_scrolled{ record.x, record.y });
			return;
		case host_input_kind::text:
			handle_text(surface, static_cast<std::uint32_t>(record.value));
			return;
		case host_input_kind::raw_move:
			if (surface.cursor_capture_active) {
				push_event(surface, input::mouse_raw_moved{ .x_delta = record.x, .y_delta = record.y });
			}
			return;
	}
}

auto gse::window::drain_host_input(window_surface& surface) -> void {
	std::array<host_input_record, host_input_batch> batch;
	while (true) {
		const auto count = gse_host_input(batch.data(), static_cast<std::int32_t>(batch.size()));
		for (const auto& record : std::span(batch).first(static_cast<std::size_t>(count))) {
			handle_record(surface, record);
		}
		if (static_cast<std::size_t>(count) < batch.size()) {
			return;
		}
	}
}

auto gse::window::read_host_surface() -> host_surface {
	std::array<std::int32_t, 6> slots{};
	gse_host_surface(slots.data());
	return {
		.size = { slots[0], slots[1] },
		.visible = slots[2] != 0,
		.locked = slots[3] != 0,
		.resize_seq = slots[4],
		.mark = slots[5],
	};
}

auto gse::window::apply_cursor_mode(data& d, const bool locked) -> void {
	const bool want = d.primary.focused
		&& !d.cursor_suppressed
		&& (d.primary.cursor_captured || !d.mouse_visible);

	if (want != capture_requested || locked != d.primary.cursor_capture_active) {
		log::println(log::category::general, "window: cursor capture want {} locked {}", want, locked);
	}
	if (want != capture_requested) {
		capture_requested = want;
		gse_host_capture(want ? 1 : 0);
	}

	d.primary.cursor_capture_active = locked;
	d.current_cursor_captured = locked;
}

auto gse::window::tick(scheduler& sched, data& d) -> void {
	[[maybe_unused]] static const bool key_codes_logged = (log_key_codes(), true);

	const auto [size, visible, locked, resize_seq, mark] = read_host_surface();
	++tick_index;
	d.primary.shown = visible;
	d.primary.focused = visible;
	if (mark != seen_mark) {
		log::println(log::category::general, "window: tick {} mark {}", tick_index, mark);
		seen_mark = mark;
	}
	if (size != d.primary.size) {
		log::println(log::category::general, "window: tick {} resize seq {} {}x{} -> {}x{}", tick_index, resize_seq, d.primary.size.x(), d.primary.size.y(), size.x(), size.y());
		d.primary.size = size;
		d.primary.framebuffer_resized = true;
	}
	else if (resize_seq != seen_resize_seq) {
		log::println(log::category::general, "window: tick {} resize seq {}", tick_index, resize_seq);
	}
	seen_resize_seq = resize_seq;

	for (const auto& [focus] : sched.read_channel<ui_focus_request>()) {
		set_ui_focus(d.primary, focus);
	}

	for (const auto& [capture] : sched.read_channel<cursor_capture_request>()) {
		d.primary.cursor_captured = capture;
	}

	apply_cursor_mode(d, locked);
	drain_host_input(d.primary);
}

auto gse::window::shutdown(data& d) -> void {
	d.secondaries.clear();
}

auto gse::window::wait_events(const time timeout) -> void {
	gse_host_wait(std::max(0.f, timeout.as<milliseconds>()));
}

auto gse::window::install_resize_pump(std::function<void()>) -> void {}

auto gse::window::take_resize_pump_stats() -> resize_pump_stats {
	return {};
}

auto gse::window::post_wake() -> void {
	gse_host_wake();
}

auto gse::window::show(data&) -> void {}

auto gse::window::minimized(native_window_handle) -> bool {
	const auto surface = read_host_surface();
	return !surface.visible || surface.size.x() == 0 || surface.size.y() == 0;
}

auto gse::window::viewport(native_window_handle) -> vec2i {
	return read_host_surface().size;
}
