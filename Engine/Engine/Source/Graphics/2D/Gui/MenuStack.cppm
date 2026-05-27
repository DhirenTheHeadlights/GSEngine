export module gse.graphics:menu_stack;

import std;

import gse.math;

import :builder;
import :types;

export namespace gse::gui {
	struct screen;

	struct nav {
		template <typename T, typename... Args>
		auto push(
			Args&&... args
		) -> void;

		auto pop() -> void;

		auto clear() -> void;

		template <typename T, typename... Args>
		auto replace_top(
			Args&&... args
		) -> void;

		[[nodiscard]] auto depth() const -> std::size_t;

	private:
		friend struct menu_stack_state;
		struct pop_tag {};
		struct clear_tag {};
		using factory = std::function<std::unique_ptr<screen>()>;
		using action = std::variant<factory, pop_tag, clear_tag>;
		std::vector<action> m_actions;
		std::size_t m_depth = 0;
	};

	struct screen {
		virtual ~screen() = default;

		virtual auto build(
			builder& ui,
			nav& n
		) -> void = 0;

		virtual auto on_push() -> void {
		}

		virtual auto on_pop() -> void {
		}

		virtual auto captures_input() const -> bool {
			return true;
		}

		virtual auto wants_chrome() const -> bool {
			return false;
		}

		virtual auto dismissable() const -> bool {
			return true;
		}

		virtual auto title() const -> std::string_view {
			return {};
		}

		virtual auto body_rect(
			const style& sty,
			vec2f viewport_size
		) const -> ui_rect;

		virtual auto draw_backdrop(
			draw_context& ctx,
			vec2f viewport_size
		) const -> void;
	};

	struct menu_stack_state {
		template <typename T, typename... Args>
		auto push(
			Args&&... args
		) -> void;

		auto push_factory(
			std::function<std::unique_ptr<screen>()> factory
		) -> void;

		auto pop() -> void;

		auto clear() -> void;

		template <typename Self>
		[[nodiscard]] auto top(
			this Self& self
		);

		[[nodiscard]] auto empty() const -> bool;

		[[nodiscard]] auto size() const -> std::size_t;

		[[nodiscard]] auto captures_input() const -> bool;

		auto tick(
			builder& ui
		) -> void;

	private:
		auto apply(
			nav& n
		) -> void;

		std::vector<std::unique_ptr<screen>> m_stack;
	};

	struct push_screen_request {
		std::function<std::unique_ptr<screen>()> factory;
	};

	struct pop_screen_request {};

	struct clear_screens_request {};

	struct set_manual_cursor_request {
		bool show = false;
	};

	struct popout_toggle {
		std::string category;
	};
}

auto gse::gui::screen::body_rect(const style& sty, const vec2f viewport_size) const -> ui_rect {
	return ui_rect::from_position_size(
		{ 0.f, viewport_size.y() },
		{ viewport_size.x(), viewport_size.y() }
	);
}

auto gse::gui::screen::draw_backdrop(draw_context& ctx, const vec2f viewport_size) const -> void {
	const ui_rect rect = body_rect(ctx.style, viewport_size);
	const vec4f color = {
		ctx.style.color_menu_body.x(),
		ctx.style.color_menu_body.y(),
		ctx.style.color_menu_body.z(),
		1.0f,
	};
	ctx.queue_sprite({
		.rect = rect,
		.color = color,
		.texture = ctx.blank_texture,
	});
}

template <typename T, typename... Args>
auto gse::gui::nav::push(Args&&... args) -> void {
	m_actions.emplace_back(factory{ [... captured = std::forward<Args>(args)]() mutable -> std::unique_ptr<screen> {
		return std::make_unique<T>(std::move(captured)...);
	} });
}

auto gse::gui::nav::pop() -> void {
	m_actions.emplace_back(pop_tag{});
}

auto gse::gui::nav::clear() -> void {
	m_actions.emplace_back(clear_tag{});
}

template <typename T, typename... Args>
auto gse::gui::nav::replace_top(Args&&... args) -> void {
	pop();
	push<T>(std::forward<Args>(args)...);
}

auto gse::gui::nav::depth() const -> std::size_t {
	return m_depth;
}

template <typename T, typename... Args>
auto gse::gui::menu_stack_state::push(Args&&... args) -> void {
	auto s = std::make_unique<T>(std::forward<Args>(args)...);
	s->on_push();
	m_stack.push_back(std::move(s));
}

auto gse::gui::menu_stack_state::push_factory(std::function<std::unique_ptr<screen>()> factory) -> void {
	if (!factory) {
		return;
	}
	auto s = factory();
	if (!s) {
		return;
	}
	s->on_push();
	m_stack.push_back(std::move(s));
}

auto gse::gui::menu_stack_state::pop() -> void {
	if (m_stack.empty()) {
		return;
	}
	m_stack.back()->on_pop();
	m_stack.pop_back();
}

auto gse::gui::menu_stack_state::clear() -> void {
	while (!m_stack.empty()) {
		pop();
	}
}

template <typename Self>
auto gse::gui::menu_stack_state::top(this Self& self) {
	using ptr_t = std::conditional_t<std::is_const_v<Self>, const screen*, screen*>;
	if (self.m_stack.empty()) {
		return ptr_t{};
	}
	return static_cast<ptr_t>(self.m_stack.back().get());
}

auto gse::gui::menu_stack_state::empty() const -> bool {
	return m_stack.empty();
}

auto gse::gui::menu_stack_state::size() const -> std::size_t {
	return m_stack.size();
}

auto gse::gui::menu_stack_state::captures_input() const -> bool {
	return !m_stack.empty() && m_stack.back()->captures_input();
}

auto gse::gui::menu_stack_state::tick(builder& ui) -> void {
	if (m_stack.empty()) {
		return;
	}
	nav n;
	n.m_depth = m_stack.size();
	m_stack.back()->build(ui, n);
	apply(n);
}

auto gse::gui::menu_stack_state::apply(nav& n) -> void {
	for (auto& a : n.m_actions) {
		std::visit(
			[this](auto&& x) {
				using A = std::decay_t<decltype(x)>;
				if constexpr (std::is_same_v<A, nav::pop_tag>) {
					pop();
				}
				else if constexpr (std::is_same_v<A, nav::clear_tag>) {
					clear();
				}
				else {
					auto s = x();
					s->on_push();
					m_stack.push_back(std::move(s));
				}
			},
			a
		);
	}
	n.m_actions.clear();
	n.m_depth = m_stack.size();
}
