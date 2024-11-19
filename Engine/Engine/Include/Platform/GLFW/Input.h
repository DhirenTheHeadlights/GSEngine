#pragma once

#include <ranges>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

namespace Engine::Input {
	struct Button {
		std::uint8_t pressed = 0;
		std::uint8_t held = 0;
		std::uint8_t released = 0;
		std::int8_t newState = -1;
		std::uint8_t typed = 0;
		float typedTime = 0.0f;
		bool toggled = false;

		void merge(const Button& b) {
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

	struct Controller {
		std::unordered_map<int, Button> buttons;

		float lt = 0.f;
		float rt = 0.f;

		struct {
			float x = 0.f, y = 0.f;
		} lStick, rStick;

		void reset() {
			for (auto& snd : buttons | std::views::values) {
				snd.reset();
			}

			lt = 0.f;
			rt = 0.f;
			lStick.x = 0.f;
			lStick.y = 0.f;
		}
	};

	struct Keyboard {
		std::unordered_map<int, Button> keys;

		std::string typedInput;

		void reset() {
			for (auto& snd : keys | std::views::values) {
				snd.reset();
			}
		}
	};

	struct Mouse {
		std::unordered_map<int, Button> buttons;

		glm::vec2 position;
		glm::vec2 delta;
		glm::ivec2 lastPosition;

		void reset() {
			for (auto& snd : buttons | std::views::values) {
				snd.reset();
			}
		}
	};

	void update();
	void setUpKeyMaps();

	Keyboard& getKeyboard();
	Controller& getController();
	Mouse& getMouse();

	void setInputsBlocked(bool blocked);

	namespace Internal {
		void processEventButton(Button& button, bool newState);
		void updateButton(Button& button);

		void updateAllButtons();
		void resetInputsToZero();

		void addToTypedInput(char input);
		void resetTypedInput();
	};
}
