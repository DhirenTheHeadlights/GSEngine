export module gse.runtime:input_api;

import std;

import gse.physics.math;

import :core_api;

export namespace gse::keyboard {
	auto pressed(
		const key k
	) -> bool {
		return system_of<input::system>().current_state().key_pressed(k);
	}

	auto held(
		const key k
	) -> bool {
		return system_of<input::system>().current_state().key_held(k);
	}

	auto released(
		const key k
	) -> bool {
		return system_of<input::system>().current_state().key_released(k);
	}

	auto text_entered(
	) -> const std::string& {
		return system_of<input::system>().current_state().text_entered();
	}
}

export namespace gse::mouse {
	auto pressed(
		const mouse_button button
	) -> bool {
		return system_of<input::system>().current_state().mouse_button_pressed(button);
	}

	auto held(
		const mouse_button button
	) -> bool {
		return system_of<input::system>().current_state().mouse_button_held(button);
	}

	auto released(
		const mouse_button button
	) -> bool {
		return system_of<input::system>().current_state().mouse_button_released(button);
	}

	auto position(
	) -> unitless::vec2 {
		return system_of<input::system>().current_state().mouse_position();
	}

	auto delta(
	) -> unitless::vec2 {
		return system_of<input::system>().current_state().mouse_delta();
	}
}