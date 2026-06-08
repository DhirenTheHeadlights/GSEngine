export module gse.graphics:popout_system;

import std;

import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.save;

import :builder;
import :menu_stack;
import :settings;
import :types;
import :gui;

export namespace gse::gui {
	struct popout_system {
		struct popout_entry {
			std::string menu_name;
			id menu_id;
			const gse::settings::register_settings_type* entry = nullptr;
			bool active = true;
		};

		struct data {
			std::unordered_map<std::string, popout_entry> popouts;
			gse::settings::panel_state panel_state;
		};

		static auto run(
			gse::run_context& ctx,
			data& d,
			const system::data& gui_d,
			const gse::save::registry& save_reg
		) -> gse::async::task<>;
	};
}

namespace gse::gui {
	[[nodiscard]] auto make_popout_menu_name(
		std::string_view category
	) -> std::string;

	[[nodiscard]]
	auto find_hot_entry(
		const gse::save::registry& save_reg,
		std::string_view category
	) -> const gse::settings::register_settings_type*;

	auto activate_popout(
		popout_system::data& d,
		const gse::save::registry& save_reg,
		std::string category
	) -> popout_system::popout_entry*;
}

auto gse::gui::make_popout_menu_name(const std::string_view category) -> std::string {
	std::string out;
	out.reserve(popout_menu_prefix.size() + category.size());
	out.append(popout_menu_prefix);
	out.append(category);
	return out;
}

auto gse::gui::find_hot_entry(const gse::save::registry& save_reg, const std::string_view category) -> const gse::settings::register_settings_type* {
	const gse::settings::register_settings_type* found = nullptr;
	save_reg.for_each_entry([&](const gse::settings::register_settings_type& entry) {
		if (entry.category == category && entry.draw_hot_fields && entry.settings_ptr) {
			found = &entry;
		}
	});
	return found;
}

auto gse::gui::activate_popout(popout_system::data& d, const gse::save::registry& save_reg, std::string category) -> popout_system::popout_entry* {
	if (const auto it = d.popouts.find(category); it != d.popouts.end()) {
		it->second.active = true;
		if (!it->second.entry) {
			it->second.entry = find_hot_entry(save_reg, category);
		}
		return &it->second;
	}
	const auto cat_view = std::string_view{ category };
	popout_system::popout_entry entry{
		.menu_name = make_popout_menu_name(cat_view),
		.menu_id = {},
		.entry = find_hot_entry(save_reg, cat_view),
		.active = true,
	};
	entry.menu_id = find_or_generate_id(entry.menu_name);
	const auto [it, _] = d.popouts.emplace(std::move(category), std::move(entry));
	return &it->second;
}

auto gse::gui::popout_system::run(gse::run_context& ctx, data& d, const system::data& gui_d, const gse::save::registry& save_reg) -> gse::async::task<> {
		for (const auto& req : ctx.read_channel<popout_toggle>()) {
			auto it = d.popouts.find(req.category);
			const bool was_active = it != d.popouts.end() && it->second.active;
			if (was_active) {
				it->second.active = false;
				ctx.channels.push<popout_closed>({ .menu_name = it->second.menu_name });
			}
			else {
				activate_popout(d, save_reg, req.category);
				if (!gui_d.show_dev_overlays) {
					ctx.channels.push<gse::settings::change_request<system>>({
						.apply = [](system::data& s) {
							s.show_dev_overlays = true;
						},
					});
				}
			}
		}

		auto try_activate_candidate = [&](const std::string_view candidate) {
			if (!is_popout_menu_tag(candidate)) {
				return;
			}
			std::string category(popout_category_from_tag(candidate));
			if (d.popouts.contains(category)) {
				return;
			}
			activate_popout(d, save_reg, std::move(category));
		};

		for (const auto& m : gui_d.menus.items()) {
			try_activate_candidate(m.id().tag());
			for (const std::string& tab : m.tab_contents) {
				try_activate_candidate(tab);
			}
		}

		for (auto& [cat, popout] : d.popouts) {
			if (!popout.active) {
				continue;
			}
			if (!popout.entry) {
				popout.entry = find_hot_entry(save_reg, cat);
				if (!popout.entry) {
					continue;
				}
			}

			ctx.channels.push<menu_content>({
				.menu = popout.menu_name,
				.layer = render_layer::overlay,
				.build = [thunk = popout.entry->draw_hot_fields, settings_ptr = popout.entry->settings_ptr, ps_ptr = &d.panel_state, &channels_ref = ctx.channels](builder& b) {
					thunk(&b, ps_ptr, settings_ptr, &channels_ref);
				},
			});
		}

	return {};
}
