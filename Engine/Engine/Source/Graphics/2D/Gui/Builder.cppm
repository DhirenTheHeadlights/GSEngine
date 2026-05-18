export module gse.graphics:builder;

import std;

import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;

import :types;
import :render_layer;

export namespace gse::gui {
	template <typename W>
	concept widget = requires {
		typename W::result;
		requires std::is_same_v<
			decltype(W::draw(
				std::declval<draw_context&>(),
				std::declval<id&>(),
				std::declval<id&>(),
				std::declval<id&>()
			)),
			typename W::result>;
	};

	template <typename W>
	concept parameterized_widget = requires {
		typename W::params;
		typename W::result;
		requires std::is_same_v<
			decltype(W::draw(
				std::declval<draw_context&>(),
				std::declval<typename W::params>(),
				std::declval<id&>(),
				std::declval<id&>(),
				std::declval<id&>()
			)),
			typename W::result>;
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

		[[nodiscard]] auto scroll_region(
			const scroll_region_info& info
		) -> scroll_handle;

		auto scroll_region(
			const scroll_region_info& info,
			const std::function<void(builder&)>& content
		) -> void;
	};

	struct menu_content {
		std::string menu;
		int priority = 0;
		render_layer layer = render_layer::content;
		std::function<void(builder&)> build;
	};
}

auto gse::gui::builder::scroll_region(const scroll_region_info& info) -> scroll_handle {
	return gse::gui::scroll_region(ctx, info);
}

auto gse::gui::builder::scroll_region(const scroll_region_info& info, const std::function<void(builder&)>& content) -> void {
	auto guard = scroll_region(info);
	if (!guard.valid()) {
		return;
	}
	content(*this);
}
