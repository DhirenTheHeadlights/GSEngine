#pragma once

#include <ranges>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

namespace gse::input {
	struct button {
		std::uint8_t pressed = 0;
		std::uint8_t held = 0;
		std::uint8_t released = 0;
		std::int8_t new_state = -1;
		std::uint8_t typed = 0;
		float typed_time = 0.0f;
		bool toggled = false;

		void merge(const button& b) {
			this->pressed |= b.pressed;
			this->released |= b.released;
			this->held |= b.held;
		}

		void reset() {
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

		void reset() {
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

		void reset() {
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

		void reset() {
			for (auto& snd : buttons | std::views::values) {
				snd.reset();
			}
		}
	};

	void update();
	void set_up_key_maps();

	keyboard& get_keyboard();
	controller& get_controller();
	mouse& get_mouse();

	void set_inputs_blocked(bool blocked);

	namespace internal {
		void process_event_button(button& button, bool new_state);
		void update_button(button& button);

		void update_all_buttons();
		void reset_inputs_to_zero();

		void add_to_typed_input(char input);
		void reset_typed_input();
	};
}
