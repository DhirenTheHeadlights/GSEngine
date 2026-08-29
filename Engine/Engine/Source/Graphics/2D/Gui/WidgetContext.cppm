module gse.graphics:widget_context;

import std;
import gse.os;

import :types;

namespace gse::gui {
	struct widget_context : draw_context {
		~widget_context() = default;

		widget_context(
			draw_context_init init,
			const input::state& input
		);
	};
}

gse::gui::widget_context::widget_context(draw_context_init init, const input::state& input) : draw_context(std::move(init), input) {}
