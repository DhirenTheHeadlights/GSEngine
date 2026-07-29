export module gse.graphics:context_actions;

import std;

import gse.meta;

import :types;

export namespace gse::gui {
	template <fixed_string Label, fixed_string Group = "", symbol_glyph Icon = nullptr>
	struct context_action {
		static constexpr std::string_view value = Label;
		static constexpr std::string_view group = Group;
		static constexpr symbol_glyph icon = Icon;
	};

	struct destructive_tag {};
	constexpr destructive_tag destructive{};

	template <typename Sig>
	struct context_action_entry {
		std::string label;
		std::string group;
		symbol_glyph icon = nullptr;
		bool destructive = false;
		bool separator_before = false;
		Sig* run = nullptr;
	};
}

namespace gse::gui {
	consteval auto find_context_action_anno(std::meta::info fn) -> std::meta::info {
		for (const auto ann : std::meta::annotations_of(fn)) {
			const auto t = std::meta::dealias(std::meta::type_of(ann));
			if (std::meta::has_template_arguments(t) && std::meta::template_of(t) == ^^context_action) {
				return ann;
			}
		}
		return std::meta::info{};
	}

	template <typename Sig, std::meta::info Fn>
	auto append_context_action(std::vector<context_action_entry<Sig>>& out) -> void {
		constexpr auto ann = find_context_action_anno(Fn);
		if constexpr (ann != std::meta::info{}) {
			using anno_t = [:std::meta::type_of(ann):];
			constexpr bool is_destructive = find_annotation<destructive_tag>(Fn) != std::meta::info{};
			out.push_back({
				.label = std::string(anno_t::value),
				.group = std::string(anno_t::group),
				.icon = anno_t::icon,
				.destructive = is_destructive,
				.run = &[:Fn:],
			});
		}
	}
}

export namespace gse::gui {
	template <typename Sig, std::meta::info... Fns>
	auto build_context_actions() -> std::vector<context_action_entry<Sig>> {
		std::vector<context_action_entry<Sig>> out;
		(append_context_action<Sig, Fns>(out), ...);
		for (std::size_t i = 1; i < out.size(); ++i) {
			out[i].separator_before = out[i].group != out[i - 1].group;
		}
		return out;
	}

	template <typename Sig>
	auto to_menu_items(const std::vector<context_action_entry<Sig>>& actions) -> std::vector<menu_item> {
		std::vector<menu_item> items;
		items.reserve(actions.size());
		for (std::uint32_t i = 0; i < actions.size(); ++i) {
			items.push_back({
				.label = actions[i].label,
				.icon = actions[i].icon,
				.action_id = i,
				.destructive = actions[i].destructive,
				.separator_before = actions[i].separator_before,
			});
		}
		return items;
	}
}
