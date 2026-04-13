export module gse.graphics:builder;

import std;

import gse.utility;

import :types;

export namespace gse::gui {
	template <typename W>
	concept widget = requires {
		typename W::result;
		{ W::draw(
			std::declval<draw_context&>(),
			std::declval<id&>(),
			std::declval<id&>(),
			std::declval<id&>()
		) } -> std::same_as<typename W::result>;
	};

	template <typename W>
	concept parameterized_widget = requires {
		typename W::params;
		typename W::result;
		{ W::draw(
			std::declval<draw_context&>(),
			std::declval<typename W::params>(),
			std::declval<id&>(),
			std::declval<id&>(),
			std::declval<id&>()
		) } -> std::same_as<typename W::result>;
	};

	struct builder {
		draw_context& ctx;
		id& hot_widget_id;
		id& active_widget_id;
		id& focus_widget_id;

		template <parameterized_widget W>
		auto draw(typename W::params p) -> W::result {
			return W::draw(ctx, std::move(p), hot_widget_id, active_widget_id, focus_widget_id);
		}

		template <widget W>
		auto draw() -> W::result {
			return W::draw(ctx, hot_widget_id, active_widget_id, focus_widget_id);
		}
	};

	struct menu_content {
		std::string menu;
		int priority = 0;
		std::function<void(builder&)> build;
	};
}
