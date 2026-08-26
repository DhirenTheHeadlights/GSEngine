export module gse.graphics:menu_stack;

import std;

import gse.math;
import gse.meta;

import :builder;
import :types;
import :font;

export namespace gse::gui {
	struct screen;

	struct caption_exclusion {
		int y0 = 0;
		int y1 = 0;
	};

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

		virtual auto occludes() const -> bool {
			return false;
		}

		virtual auto wants_chrome() const -> bool {
			return false;
		}

		virtual auto draw_caption(
			builder& b,
			const rectf& area
		) -> float;

		virtual auto caption_exclusion_range(
			const draw_context& ctx,
			const rectf& full_rect
		) const -> caption_exclusion {
			return {};
		}

		virtual auto dismissable() const -> bool {
			return true;
		}

		virtual auto should_dismiss() const -> bool {
			return false;
		}

		virtual auto title() const -> std::string_view {
			return {};
		}

		virtual auto body_rect(
			const style& sty,
			vec2f viewport_size
		) const -> rectf;

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

		[[nodiscard]] auto top(
			this menu_stack_state& self
		) -> screen*;

		[[nodiscard]] auto top(
			this const menu_stack_state& self
		) -> const screen*;

		[[nodiscard]] auto empty() const -> bool;

		[[nodiscard]] auto size() const -> std::size_t;

		[[nodiscard]] auto captures_input() const -> bool;

		[[nodiscard]] auto occludes() const -> bool;

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
		id window;
	};

	struct pop_screen_request {};

	struct clear_screens_request {};

	struct set_manual_cursor_request {
		bool show = false;
	};

	struct popout_toggle {
		std::string category;
	};

	struct popout_closed {
		std::string menu_name;
	};

	struct menu_migrate_request {
		std::string menu_name;
		id target_window;
	};

	constexpr std::string_view popout_menu_prefix = "live::";

	[[nodiscard]] auto is_popout_menu_tag(
		std::string_view tag
	) -> bool;

	[[nodiscard]] auto popout_category_from_tag(
		std::string_view tag
	) -> std::string_view;
}

auto gse::gui::is_popout_menu_tag(const std::string_view tag) -> bool {
	return tag.starts_with(popout_menu_prefix);
}

auto gse::gui::popout_category_from_tag(const std::string_view tag) -> std::string_view {
	if (!tag.starts_with(popout_menu_prefix)) {
		return {};
	}
	return tag.substr(popout_menu_prefix.size());
}

auto gse::gui::screen::draw_caption(builder& b, const rectf& area) -> float {
	const draw_context& ctx = b.ctx;
	const std::string_view label = title();
	if (label.empty()) {
		return 0.f;
	}

	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = label,
		.position = {
			area.left() + ctx.style.padding,
			area.center().y() + ctx.fonts.text.resolve()->vertical_center_offset(ctx.style.font_size)
		},
		.scale = ctx.style.font_size,
		.color = ctx.style.color_text_secondary,
		.clip_rect = area,
	});

	return 0.f;
}

auto gse::gui::screen::body_rect(const style& sty, const vec2f viewport_size) const -> rectf {
	return rectf::from_position_size(
		{ 0.f, viewport_size.y() },
		{ viewport_size.x(), viewport_size.y() }
	);
}

auto gse::gui::screen::draw_backdrop(draw_context& ctx, const vec2f viewport_size) const -> void {
	const rectf rect = body_rect(ctx.style, viewport_size);
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

auto gse::gui::menu_stack_state::top(this menu_stack_state& self) -> screen* {
	if (self.m_stack.empty()) {
		return nullptr;
	}
	return self.m_stack.back().get();
}

auto gse::gui::menu_stack_state::top(this const menu_stack_state& self) -> const screen* {
	if (self.m_stack.empty()) {
		return nullptr;
	}
	return self.m_stack.back().get();
}

auto gse::gui::menu_stack_state::empty() const -> bool {
	return m_stack.empty();
}

auto gse::gui::menu_stack_state::size() const -> std::size_t {
	return m_stack.size();
}

auto gse::gui::menu_stack_state::occludes() const -> bool {
	return !m_stack.empty() && m_stack.back()->occludes();
}

auto gse::gui::menu_stack_state::captures_input() const -> bool {
	return !m_stack.empty() && m_stack.back()->captures_input();
}

auto gse::gui::menu_stack_state::tick(builder& ui) -> void {
	for (auto it = m_stack.begin(); it != m_stack.end();) {
		if ((*it)->should_dismiss()) {
			(*it)->on_pop();
			it = m_stack.erase(it);
		}
		else {
			++it;
		}
	}
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
		match(a)
			.if_is([this](const nav::pop_tag&) {
				pop();
			})
			.else_if_is([this](const nav::clear_tag&) {
				clear();
			})
			.else_if_is([this](const nav::factory& f) {
				auto s = f();
				s->on_push();
				m_stack.push_back(std::move(s));
			});
	}
	n.m_actions.clear();
	n.m_depth = m_stack.size();
}