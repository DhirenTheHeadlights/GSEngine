module;

#include <glad/glad.h>
#include <GLFW/glfw3.h>

export module gse.platform.glfw.input;

import std;
import glm;

export namespace gse::input {
	struct button {
		std::uint8_t pressed = 0;
		std::uint8_t held = 0;
		std::uint8_t released = 0;
		std::int8_t new_state = -1;
		std::uint8_t typed = 0;
		float typed_time = 0.0f;
		bool toggled = false;

		auto merge(const button& b) -> void {
			this->pressed |= b.pressed;
			this->released |= b.released;
			this->held |= b.held;
		}

		auto reset() -> void {
			pressed = 0;
			held = 0;
			released = 0;
		}
	};

	struct controller {
		std::unordered_map<int, button> buttons;

		float lt = 0.f;
		float rt = 0.f;

		struct {
			float x = 0.f, y = 0.f;
		} l_stick, r_stick;

		auto reset() -> void {
			for (auto& snd : buttons | std::views::values) {
				snd.reset();
			}

			lt = 0.f;
			rt = 0.f;
			l_stick.x = 0.f;
			l_stick.y = 0.f;
		}
	};

	struct keyboard {
		std::unordered_map<int, button> keys;

		std::string typed_input;

		auto reset() -> void {
			for (auto& snd : keys | std::views::values) {
				snd.reset();
			}
		}
	};

	struct mouse {
		std::unordered_map<int, button> buttons;

		glm::vec2 position;
		glm::vec2 delta;
		glm::ivec2 last_position;

		auto reset() -> void {
			for (auto& snd : buttons | std::views::values) {
				snd.reset();
			}
		}
	};

	auto update() -> void;
	auto set_up_key_maps() -> void;

	auto get_keyboard() -> keyboard&;
	auto get_controller() -> controller&;
	auto get_mouse() -> mouse&;

	auto set_inputs_blocked(bool blocked) -> void;

	namespace internal {
		auto process_event_button(button& button, bool new_state) -> void;
		auto update_button(button& button) -> void;

		auto update_all_buttons() -> void;
		auto reset_inputs_to_zero() -> void;

		auto add_to_typed_input(char input) -> void;
		auto reset_typed_input() -> void;
	};
}
