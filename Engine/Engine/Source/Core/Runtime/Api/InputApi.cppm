export module gse.runtime:input_api;

import std;

import gse.math;

import :core_api;

export namespace gse::keyboard {
	auto pressed(key k
	) -> bool;

	auto held(key k
	) -> bool;

	auto released(key k
	) -> bool;

	auto text_entered(
	) -> const std::string&;
}

export namespace gse::mouse {
	auto pressed(mouse_button button
	) -> bool;

	auto held(mouse_button button
	) -> bool;

	auto released(mouse_button button
	) -> bool;

	auto position(
	) -> vec2f;

	auto delta(
	) -> vec2f;
}

auto gse::keyboard::pressed(const key k) -> bool {
	return state_of<input::system_state>().current_state().key_pressed(k);
}

auto gse::keyboard::held(const key k) -> bool {
	return state_of<input::system_state>().current_state().key_held(k);
}

auto gse::keyboard::released(const key k) -> bool {
	return state_of<input::system_state>().current_state().key_released(k);
}

auto gse::keyboard::text_entered() -> const std::string& {
	return state_of<input::system_state>().current_state().text_entered();
}

auto gse::mouse::pressed(const mouse_button button) -> bool {
	return state_of<input::system_state>().current_state().mouse_button_pressed(button);
}

auto gse::mouse::held(const mouse_button button) -> bool {
	return state_of<input::system_state>().current_state().mouse_button_held(button);
}

auto gse::mouse::released(const mouse_button button) -> bool {
	return state_of<input::system_state>().current_state().mouse_button_released(button);
}

auto gse::mouse::position() -> vec2f {
	return state_of<input::system_state>().current_state().mouse_position();
}

auto gse::mouse::delta() -> vec2f {
	return state_of<input::system_state>().current_state().mouse_delta();
}
