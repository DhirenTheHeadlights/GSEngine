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

export namespace gse::gui::popout_system {
	struct popout_entry {
		std::string menu_name;
		id menu_id;
		const gse::settings::register_settings_type* entry = nullptr;
		bool active = true;
	};

	struct [[= gse::system_state<"Popouts">{}]] data {
		std::unordered_map<std::string, popout_entry> popouts;
		gse::settings::panel_state panel_state;
	};

	[[= gse::system_run<>{}]]
	auto run(
		gse::context& ctx,
		data& d,
		gse::channel_read<popout_toggle> popout_in,
		gse::channel_write<popout_closed, gse::settings::change_request, menu_content> popout_out,
		gse::shared_view<gse::gui::data> gui_d,
		const gse::save::registry& save_reg
	) -> gse::async::task<>;
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
		if (entry.category == category && entry.settings_ptr && std::ranges::any_of(entry.fields, &gse::settings::settings_field::hot_reloadable)) {
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

auto gse::gui::popout_system::run(gse::context& ctx, data& d, const gse::channel_read<popout_toggle> popout_in, const gse::channel_write<popout_closed, gse::settings::change_request, menu_content> popout_out, const gse::shared_view<gui::data> gui_d, const gse::save::registry& save_reg) -> gse::async::task<> {
		for (const auto& req : popout_in.of<popout_toggle>()) {
			auto it = d.popouts.find(req.category);
			const bool was_active = it != d.popouts.end() && it->second.active;
			if (was_active) {
				it->second.active = false;
				popout_out.push<popout_closed>({ .menu_name = it->second.menu_name });
			}
			else {
				activate_popout(d, save_reg, req.category);
				if (!gui_d.show_dev_overlays) {
					popout_out.push<gse::settings::change_request>({
						.state_type = gse::id_of<gui::data>(),
						.apply = [](void* p) {
							static_cast<gui::data*>(p)->show_dev_overlays = true;
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

			popout_out.push<menu_content>({
				.menu = popout.menu_name,
				.layer = render_layer::overlay,
				.build = [entry = popout.entry, ps_ptr = &d.panel_state, settings_out = gse::settings::change_request_writer(popout_out)](builder& b) {
					gse::settings::draw_fields_for_entry(b, *ps_ptr, settings_out, *entry, true);
				},
			});
		}

	return {};
}
