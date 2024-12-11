#pragma once

#include "Engine.h"

namespace Editor {
	void initialize();

	void bindFbo();
	void unbindFbo();

	void update();
	void render();

	void exit();

	struct RenderingInterface : gse::window::rendering_interface {
		void on_pre_render() override {
			
		}

		void on_post_render() override {
			unbindFbo();

			update();
			render();
		}
	};
}