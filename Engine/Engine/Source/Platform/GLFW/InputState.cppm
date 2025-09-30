export module gse.platform:input_state;

import std;

import :keys;

import gse.physics.math;

namespace gse::detail {
	struct input_state_token;
}

export namespace gse::input {
	class state {
	public:
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

		auto mouse_position(
		) const -> unitless::vec2;

		auto mouse_delta(
		) const -> unitless::vec2;

		auto text_entered(
		) const -> const std::string&;

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

		auto append_codepoint(
			std::uint32_t codepoint,
			const detail::input_state_token& token
		) -> void;

		auto end_frame(
			const detail::input_state_token& token
		) -> void;
	private:
		std::unordered_set<key> m_keys_held;
		std::unordered_set<key> m_keys_pressed_this_frame;
		std::unordered_set<key> m_keys_released_this_frame;

		std::unordered_set<mouse_button> m_mouse_buttons_held;
		std::unordered_set<mouse_button> m_mouse_buttons_pressed_this_frame;
		std::unordered_set<mouse_button> m_mouse_buttons_released_this_frame;

		unitless::vec2 m_mouse_position;
		unitless::vec2 m_mouse_prev_position;
		unitless::vec2 m_mouse_delta;
		std::string m_text_entered_this_frame;
	};
}

auto gse::input::state::key_pressed(const key key) const -> bool {
	return m_keys_pressed_this_frame.contains(key);
}

auto gse::input::state::key_held(const key key) const -> bool {
	return m_keys_held.contains(key);
}

auto gse::input::state::key_released(const key key) const -> bool {
	return m_keys_released_this_frame.contains(key);
}

auto gse::input::state::mouse_button_pressed(const mouse_button button) const -> bool {
	return m_mouse_buttons_pressed_this_frame.contains(button);
}

auto gse::input::state::mouse_button_held(const mouse_button button) const -> bool {
	return m_mouse_buttons_held.contains(button);
}

auto gse::input::state::mouse_button_released(const mouse_button button) const -> bool {
	return m_mouse_buttons_released_this_frame.contains(button);
}

auto gse::input::state::mouse_position() const -> unitless::vec2 {
	return m_mouse_position;
}

auto gse::input::state::mouse_delta() const -> unitless::vec2 {
	return m_mouse_delta;
}

auto gse::input::state::text_entered() const -> const std::string& {
	return m_text_entered_this_frame;
}

auto gse::input::state::begin_frame(const detail::input_state_token&) -> void {
	m_keys_pressed_this_frame.clear();
	m_keys_released_this_frame.clear();
	m_mouse_buttons_pressed_this_frame.clear();
	m_mouse_buttons_released_this_frame.clear();
	m_text_entered_this_frame.clear();
	m_mouse_prev_position = m_mouse_position;
}

auto gse::input::state::on_key_pressed(const key key, const detail::input_state_token&) -> void {
	m_keys_pressed_this_frame.insert(key);
	m_keys_held.insert(key);
}

auto gse::input::state::on_key_released(const key key, const detail::input_state_token&) -> void {
	m_keys_released_this_frame.insert(key);
	m_keys_held.erase(key);
}

auto gse::input::state::on_mouse_button_pressed(const mouse_button button, const detail::input_state_token&) -> void {
	m_mouse_buttons_pressed_this_frame.insert(button);
	m_mouse_buttons_held.insert(button);
}

auto gse::input::state::on_mouse_button_released(const mouse_button button, const detail::input_state_token&) -> void {
	m_mouse_buttons_released_this_frame.insert(button);
	m_mouse_buttons_held.erase(button);
}

auto gse::input::state::on_mouse_moved(const float x, const float y, const detail::input_state_token&) -> void {
	m_mouse_position = { x, y };
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
	m_mouse_delta = m_mouse_position - m_mouse_prev_position;
}
