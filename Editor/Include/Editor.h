#pragma once

#include "Engine.h"

namespace Editor {
	void bindFbo();
	void unbindFbo();
	void renderImGuiViewport();

	void update();
	void render();

	struct RenderingInterface : Engine::RenderingInterface {
		void onPreRender() override {
			bindFbo();
		}
		void onPostRender() override {
			unbindFbo();
			renderImGuiViewport();
		}
	};
}