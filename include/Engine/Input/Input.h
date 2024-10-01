#pragma once

#include <string>
#include <unordered_map>
#include "Engine/Platform/PlatformFunctions.h"

namespace Engine::Input {
	struct Button {
		char pressed = 0;
		char held = 0;
		char released = 0;
		char newState = -1;
		char typed = 0;
		float typedTime = 0;
		bool toggled = false;

		void merge(const Button &b) {
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
			for (auto& [fst, snd] : buttons) {
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
			for (auto& [fst, snd] : keys) {
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
			for (auto& [fst, snd] : buttons) {
				snd.reset();
			}
		}
	};

	void update();
	void setUpKeyMaps();
	Keyboard& getKeyboard();
	Controller& getController();
	Mouse& getMouse();

	namespace Internal {
		inline void processEventButton(Button& b, const bool newState) {
			b.newState = newState;
		}

		inline void updateButton(Button& b, const float deltaTime) {
			if (b.newState == 1) {
				if (b.held) {
					b.pressed = false;
				}
				else {
					b.pressed = true;
					b.toggled = !b.toggled;
				}

				b.held = true;
				b.released = false;
			}
			else if (b.newState == 0) {
				b.held = false;
				b.pressed = false;
				b.released = true;
			}
			else {
				b.pressed = false;
				b.released = false;
			}

			// Processing typed
			if (b.pressed)
			{
				b.typed = true;
				b.typedTime = 0.48f;
			}
			else if(b.held) {
				b.typedTime -= deltaTime;
			
				if (b.typedTime < 0.f)
				{
					b.typedTime += 0.07f;
					b.typed = true;
				}
				else {
					b.typed = false;
				}

			}
			else {
				b.typedTime = 0;
				b.typed = false;
			}
			b.newState = -1;
		}

		void updateAllButtons(float deltaTime);
		void resetInputsToZero();

		void addToTypedInput(char c);
		void resetTypedInput();
	};
}
