#pragma once

#include "Engine.h"

namespace editor {
	void initialize();

	void bind_fbo();
	void unbind_fbo();

	void update();
	void render();

	void exit();

	struct rendering_interface final : gse::window::rendering_interface {
		void on_pre_render() override {
			
		}

		void on_post_render() override {
			unbind_fbo();

			update();
			render();
		}
	};
}