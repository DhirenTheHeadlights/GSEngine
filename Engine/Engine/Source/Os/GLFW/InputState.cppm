export module gse.os:input_state;

import std;

import :keys;

import gse.math;

namespace gse::detail {
	struct input_state_token;
}

export namespace gse::input {
	class state {
	public:
		state() noexcept = default;
		~state() noexcept = default;
		state(
			const state&
		) = default;
		state(
			state&&
		) noexcept = default;
		auto operator=(
			const state&
		) -> state& = default;
		auto operator=(
			state&&
		) noexcept -> state& = default;

		auto key_pressed(
			key key
		) const -> bool;

		auto key_held(
			key key
		) const -> bool;

		auto key_released(
			key key
		) const -> bool;

		auto mouse_button_pressed(
			mouse_button button
		) const -> bool;

		auto mouse_button_held(
			mouse_button button
		) const -> bool;

		auto mouse_button_released(
			mouse_button button
		) const -> bool;

		auto mouse_position() const -> vec2f;

		auto mouse_delta() const -> vec2f;

		auto scroll_delta() const -> vec2f;

		auto text_entered() const -> const std::string&;

		auto keys_held() const -> std::span<const key>;

		auto keys_pressed() const -> std::span<const key>;

		auto keys_released() const -> std::span<const key>;

		auto mouse_buttons_held() const -> std::span<const mouse_button>;

		auto mouse_buttons_pressed() const -> std::span<const mouse_button>;

		auto mouse_buttons_released() const -> std::span<const mouse_button>;

		auto begin_frame(
			const detail::input_state_token& token
		) -> void;

		auto on_key_pressed(
			key key,
			const detail::input_state_token& token
		) -> void;

		auto on_key_released(
			key key,
			const detail::input_state_token& token
		) -> void;

		auto on_mouse_button_pressed(
			mouse_button button,
			const detail::input_state_token& token
		) -> void;

		auto on_mouse_button_released(
			mouse_button button,
			const detail::input_state_token& token
		) -> void;

		auto on_mouse_moved(
			float x,
			float y,
			const detail::input_state_token& token
		) -> void;

		auto on_mouse_raw_moved(
			float x_delta,
			float y_delta,
			const detail::input_state_token& token
		) -> void;

		auto on_scroll(
			float x,
			float y,
			const detail::input_state_token& token
		) -> void;

		auto append_codepoint(
			std::uint32_t codepoint,
			const detail::input_state_token& token
		) -> void;

		auto end_frame(
			const detail::input_state_token& token
		) -> void;

		auto copy_persistent_from(
			const state& other
		) -> void;

	private:
		std::vector<key> m_keys_held;
		std::vector<key> m_keys_pressed_this_frame;
		std::vector<key> m_keys_released_this_frame;

		std::vector<mouse_button> m_mouse_buttons_held;
		std::vector<mouse_button> m_mouse_buttons_pressed_this_frame;
		std::vector<mouse_button> m_mouse_buttons_released_this_frame;

		vec2f m_mouse_position;
		vec2f m_mouse_prev_position;
		vec2f m_mouse_delta;
		vec2f m_mouse_raw_delta;
		bool m_mouse_raw_received = false;
		vec2f m_scroll_delta;
		std::string m_text_entered_this_frame;
	};
}

auto gse::input::state::key_pressed(const key key) const -> bool {
	return std::ranges::contains(m_keys_pressed_this_frame, key);
}

auto gse::input::state::key_held(const key key) const -> bool {
	return std::ranges::contains(m_keys_held, key);
}

auto gse::input::state::key_released(const key key) const -> bool {
	return std::ranges::contains(m_keys_released_this_frame, key);
}

auto gse::input::state::mouse_button_pressed(const mouse_button button) const -> bool {
	return std::ranges::contains(m_mouse_buttons_pressed_this_frame, button);
}

auto gse::input::state::mouse_button_held(const mouse_button button) const -> bool {
	return std::ranges::contains(m_mouse_buttons_held, button);
}

auto gse::input::state::mouse_button_released(const mouse_button button) const -> bool {
	return std::ranges::contains(m_mouse_buttons_released_this_frame, button);
}

auto gse::input::state::mouse_position() const -> vec2f {
	return m_mouse_position;
}

auto gse::input::state::mouse_delta() const -> vec2f {
	return m_mouse_delta;
}

auto gse::input::state::scroll_delta() const -> vec2f {
	return m_scroll_delta;
}

auto gse::input::state::text_entered() const -> const std::string& {
	return m_text_entered_this_frame;
}

auto gse::input::state::keys_held() const -> std::span<const key> {
	return m_keys_held;
}

auto gse::input::state::keys_pressed() const -> std::span<const key> {
	return m_keys_pressed_this_frame;
}

auto gse::input::state::keys_released() const -> std::span<const key> {
	return m_keys_released_this_frame;
}

auto gse::input::state::mouse_buttons_held() const -> std::span<const mouse_button> {
	return m_mouse_buttons_held;
}

auto gse::input::state::mouse_buttons_pressed() const -> std::span<const mouse_button> {
	return m_mouse_buttons_pressed_this_frame;
}

auto gse::input::state::mouse_buttons_released() const -> std::span<const mouse_button> {
	return m_mouse_buttons_released_this_frame;
}

auto gse::input::state::begin_frame(const detail::input_state_token&) -> void {
	m_keys_pressed_this_frame.clear();
	m_keys_released_this_frame.clear();
	m_mouse_buttons_pressed_this_frame.clear();
	m_mouse_buttons_released_this_frame.clear();
	m_text_entered_this_frame.clear();
	m_mouse_prev_position = m_mouse_position;
	m_mouse_raw_delta = {};
	m_mouse_raw_received = false;
	m_scroll_delta = {};
}

auto gse::input::state::on_key_pressed(const key key, const detail::input_state_token&) -> void {
	if (!std::ranges::contains(m_keys_pressed_this_frame, key)) {
		m_keys_pressed_this_frame.push_back(key);
	}
	if (!std::ranges::contains(m_keys_held, key)) {
		m_keys_held.push_back(key);
	}
}

auto gse::input::state::on_key_released(const key key, const detail::input_state_token&) -> void {
	if (!std::ranges::contains(m_keys_released_this_frame, key)) {
		m_keys_released_this_frame.push_back(key);
	}
	std::erase(m_keys_held, key);
}

auto gse::input::state::on_mouse_button_pressed(const mouse_button button, const detail::input_state_token&) -> void {
	if (!std::ranges::contains(m_mouse_buttons_pressed_this_frame, button)) {
		m_mouse_buttons_pressed_this_frame.push_back(button);
	}
	if (!std::ranges::contains(m_mouse_buttons_held, button)) {
		m_mouse_buttons_held.push_back(button);
	}
}

auto gse::input::state::on_mouse_button_released(const mouse_button button, const detail::input_state_token&) -> void {
	if (!std::ranges::contains(m_mouse_buttons_released_this_frame, button)) {
		m_mouse_buttons_released_this_frame.push_back(button);
	}
	std::erase(m_mouse_buttons_held, button);
}

auto gse::input::state::on_mouse_moved(const float x, const float y, const detail::input_state_token&) -> void {
	m_mouse_position = { x, y };
}

auto gse::input::state::on_mouse_raw_moved(const float x_delta, const float y_delta, const detail::input_state_token&) -> void {
	m_mouse_raw_delta += vec2f{ x_delta, y_delta };
	m_mouse_raw_received = true;
}

auto gse::input::state::on_scroll(const float x, const float y, const detail::input_state_token&) -> void {
	m_scroll_delta += vec2f{ x, y };
}

auto gse::input::state::append_codepoint(const std::uint32_t codepoint, const detail::input_state_token&) -> void {
	if (codepoint < 0x80) {
		m_text_entered_this_frame += static_cast<char>(codepoint);
	}
	else if (codepoint < 0x800) {
		m_text_entered_this_frame += static_cast<char>(0xC0 | (codepoint >> 6));
		m_text_entered_this_frame += static_cast<char>(0x80 | (codepoint & 0x3F));
	}
	else if (codepoint < 0x10000) {
		m_text_entered_this_frame += static_cast<char>(0xE0 | (codepoint >> 12));
		m_text_entered_this_frame += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
		m_text_entered_this_frame += static_cast<char>(0x80 | (codepoint & 0x3F));
	}
	else {
		m_text_entered_this_frame += static_cast<char>(0xF0 | (codepoint >> 18));
		m_text_entered_this_frame += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
		m_text_entered_this_frame += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
		m_text_entered_this_frame += static_cast<char>(0x80 | (codepoint & 0x3F));
	}
}

auto gse::input::state::end_frame(const detail::input_state_token&) -> void {
	m_mouse_delta = m_mouse_raw_received ? m_mouse_raw_delta : m_mouse_position - m_mouse_prev_position;
}

auto gse::input::state::copy_persistent_from(const state& other) -> void {
	m_keys_held = other.m_keys_held;
	m_mouse_buttons_held = other.m_mouse_buttons_held;
	m_mouse_position = other.m_mouse_position;
	m_mouse_prev_position = other.m_mouse_prev_position;
	m_mouse_delta = other.m_mouse_delta;
}
