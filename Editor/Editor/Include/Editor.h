#pragma once

#include "Engine.h"

namespace Editor {
	void initialize();

	void bindFbo();
	void unbindFbo();

	void update();
	void render();

	void exit();

	struct RenderingInterface : gse::Window::RenderingInterface {
		void onPreRender() override {
			
		}

		void onPostRender() override {
			unbindFbo();

			update();
			render();
		}
	};
}