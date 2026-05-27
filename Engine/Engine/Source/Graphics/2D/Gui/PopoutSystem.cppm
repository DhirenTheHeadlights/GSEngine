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
		struct active_popout {
			std::string menu_name;
			id menu_id;
			const gse::settings::register_settings_type* entry = nullptr;
		};

		struct data {
			std::unordered_map<std::string, active_popout> active;
			gse::settings::panel_state panel_state;
			bool initialized = false;
		};

		static auto run(
			gse::run_context& ctx,
			data& d,
			const system::data& gui_d,
			const gse::save::registry& save_reg
		) -> gse::async::task<>;
	};

	constexpr std::string_view popout_menu_prefix = "live::";
}

namespace gse::gui {
	[[nodiscard]] auto make_popout_menu_name(
		std::string_view category
	) -> std::string;

	[[nodiscard]]
	auto extract_popout_category(
		std::string_view menu_name
	) -> std::optional<std::string_view>;

	[[nodiscard]]
	auto find_hot_entry(
		const gse::save::registry& save_reg,
		std::string_view category
	) -> const gse::settings::register_settings_type*;

	auto activate_popout(
		popout_system::data& d,
		const gse::save::registry& save_reg,
		std::string category
	) -> popout_system::active_popout*;
}

auto gse::gui::make_popout_menu_name(const std::string_view category) -> std::string {
	std::string out;
	out.reserve(popout_menu_prefix.size() + category.size());
	out.append(popout_menu_prefix);
	out.append(category);
	return out;
}

auto gse::gui::extract_popout_category(const std::string_view menu_name) -> std::optional<std::string_view> {
	if (!menu_name.starts_with(popout_menu_prefix)) {
		return std::nullopt;
	}
	return menu_name.substr(popout_menu_prefix.size());
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

auto gse::gui::activate_popout(popout_system::data& d, const gse::save::registry& save_reg, std::string category) -> popout_system::active_popout* {
	const auto cat_view = std::string_view{ category };
	popout_system::active_popout popout{
		.menu_name = make_popout_menu_name(cat_view),
		.menu_id = {},
		.entry = find_hot_entry(
			save_reg,
			cat_view
		),
	};
	popout.menu_id = find_or_generate_id(popout.menu_name);
	const auto [it, _] = d.active.emplace(std::move(category), std::move(popout));
	return &it->second;
}

auto gse::gui::popout_system::run(gse::run_context& ctx, data& d, const system::data& gui_d, const gse::save::registry& save_reg) -> gse::async::task<> {
	while (true) {
		if (!d.initialized) {
			for (const auto& m : gui_d.menus.items()) {
				if (const auto cat = extract_popout_category(m.id().tag())) {
					activate_popout(d, save_reg, std::string(*cat));
				}
			}
			d.initialized = true;
		}

		for (const auto& req : ctx.read_channel<popout_toggle>()) {
			if (const auto it = d.active.find(req.category); it != d.active.end()) {
				d.active.erase(it);
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

		for (auto& [cat, popout] : d.active) {
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

		co_await ctx.next_tick();
	}
}
