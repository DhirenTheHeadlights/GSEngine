export module gse.graphics:builder;

import std;

import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.assert;
import gse.diag;
import gse.ecs;
import gse.math;

import :types;
import :render_layer;

export namespace gse::gui {
	template <typename W>
	concept widget = requires {
		typename W::result;
		requires std::is_same_v<decltype(W::draw(
			std::declval<draw_context&>(),
			std::declval<id&>(),
			std::declval<id&>(),
			std::declval<id&>()
		)),
								typename W::result>; };

	template <typename W>
	concept parameterized_widget = requires {
		typename W::params;
		typename W::result;
		requires std::is_same_v<decltype(W::draw(
			std::declval<draw_context&>(),
			std::declval<typename W::params>(),
			std::declval<id&>(),
			std::declval<id&>(),
			std::declval<id&>()
		)),
								typename W::result>; };

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

		template <typename F>
		requires std::invocable<F, builder&>
		auto scroll_region(
			const scroll_region_info& info,
			F&& content
		) -> void;

		template <typename F>
		requires std::invocable<F, builder&, const row&>
		auto row_list(
			const row_list_info& info,
			F&& draw_row
		) -> void;
	};

	template <typename Arg>
	constexpr bool is_draw_builder_arg_v = std::is_same_v<std::remove_cvref_t<Arg>, builder>;

	template <typename Arg>
	constexpr bool is_draw_injected_arg_v =
		is_draw_builder_arg_v<Arg> ||
		is_shared_view_v<Arg> ||
		is_optional_shared_view_v<Arg> ||
		is_channel_write_v<std::remove_cvref_t<Arg>>;

	template <typename... Args>
	consteval auto draw_owner_arity() -> std::size_t {
		return (static_cast<std::size_t>(!is_draw_injected_arg_v<Args>) + ... + 0u);
	}

	template <typename... Args>
	struct draw_owner {
		using type = void;
	};

	template <typename First, typename... Rest>
	struct draw_owner<First, Rest...> {
		using type = std::conditional_t<
			is_draw_injected_arg_v<First>,
			typename draw_owner<Rest...>::type,
			std::remove_cvref_t<First>
		>;
	};

	struct draw_no_owner {};

	template <typename Owner>
	consteval auto draw_owner_id() -> id {
		if constexpr (std::is_same_v<Owner, void>) {
			return {};
		}
		else {
			return id_of<Owner>();
		}
	}

	template <typename Arg, typename Data>
	auto resolve_draw_arg(
		builder& b,
		context& ctx,
		Data& state
	) -> decltype(auto);

	template <typename F, typename Fn>
	struct draw_thunk;

	template <typename F, typename... Args>
	struct draw_thunk<F, void (*)(Args...)> {
		using owner = typename draw_owner<Args...>::type;

		static_assert(
			draw_owner_arity<Args...>() <= 1,
			"a draw_fn borrows at most one system's data; every other parameter must be builder&, shared_view<X> or channel_write<...>"
		);

		static auto invoke(
			builder& b,
			context& ctx
		) -> void;
	};

	class draw_fn : public identifiable_owned {
	public:
		static constexpr bool lends_system_state = true;

		draw_fn() = default;

		template <typename F>
		requires requires(F f) { +f; }
		draw_fn(
			F f
		);

		auto operator()(
			builder& b,
			context& ctx
		) const -> void;

	private:
		void (*m_invoke)(builder&, context&) = nullptr;
	};

	struct menu_content {
		std::string menu;
		int priority = 0;
		render_layer layer = render_layer::content;
		bool translucent = false;
		draw_fn build;
	};

	struct hud_content {
		std::string name;
		render_layer layer = render_layer::overlay;
		draw_fn build;
	};
}

template <typename Arg, typename Data>
auto gse::gui::resolve_draw_arg(builder& b, context& ctx, Data& state) -> decltype(auto) {
	if constexpr (is_draw_builder_arg_v<Arg>) {
		return (b);
	}
	else {
		static_assert(
			is_draw_injected_arg_v<Arg> || std::is_same_v<std::remove_cvref_t<Arg>, Data>,
			"a draw_fn runs inside the gui's own run(), so it can only be handed builder&, its owning system's data, shared_view<X> or channel_write<...>; a channel_read, an access token or a resource pointer would take a permission the scheduler attributed to the gui"
		);
		return resolve_run_arg<Arg, Data>(ctx, state);
	}
}

template <typename F, typename... Args>
auto gse::gui::draw_thunk<F, void (*)(Args...)>::invoke(builder& b, context& ctx) -> void {
	if constexpr (std::is_same_v<owner, void>) {
		draw_no_owner none;
		F{}(resolve_draw_arg<Args, draw_no_owner>(b, ctx, none)...);
	}
	else {
		owner* state = live_system_state<owner>(ctx);
		assert(state != nullptr, "a draw_fn outlived {}, the system that lent it", type_tag<owner>());
		F{}(resolve_draw_arg<Args, owner>(b, ctx, *state)...);
	}
}

template <typename F>
requires requires(F f) { +f; }
gse::gui::draw_fn::draw_fn(F f)
	: identifiable_owned(draw_owner_id<typename draw_thunk<F, decltype(+f)>::owner>()), m_invoke(&draw_thunk<F, decltype(+f)>::invoke) {
}

auto gse::gui::draw_fn::operator()(builder& b, context& ctx) const -> void {
	assert(m_invoke != nullptr, "a menu_content was pushed without a build callback");
	m_invoke(b, ctx);
}

auto gse::gui::builder::scroll_region(const scroll_region_info& info) -> scroll_handle {
	return gui::scroll_region(ctx, info);
}

template <typename F>
requires std::invocable<F, gse::gui::builder&>
auto gse::gui::builder::scroll_region(const scroll_region_info& info, F&& content) -> void {
	auto guard = scroll_region(info);
	if (!guard.valid()) {
		return;
	}
	content(*this);
}

template <typename F>
requires std::invocable<F, gse::gui::builder&, const gse::gui::row&>
auto gse::gui::builder::row_list(const row_list_info& info, F&& draw_row) -> void {
	gui::row_list(ctx, info, [this, &draw_row](const row& r) {
		draw_row(*this, r);
	});
}
