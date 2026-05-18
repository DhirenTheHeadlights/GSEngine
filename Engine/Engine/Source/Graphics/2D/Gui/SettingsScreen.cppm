export module gse.graphics:settings_screen;

import std;

import gse.core;
import gse.ecs;
import gse.save;

import :menu_stack;
import :builder;
import :settings;
import :scroll_widget;

export namespace gse::gui {
	class settings_screen : public screen {
	public:
		settings_screen(
			const save::registry& save_reg,
			channel_writer channels
		);

		auto build(
			builder& ui,
			nav& n
		) -> void override;

		auto title(
		) const -> std::string_view override;

		auto section(
		) const -> menu_bar::section override;
	private:
		const save::registry* m_save_reg;
		channel_writer m_channels;
		settings::panel_state m_panel_state;
	};
}

gse::gui::settings_screen::settings_screen(const save::registry& save_reg, channel_writer channels)
	: m_save_reg(&save_reg), m_channels(std::move(channels)) {}

auto gse::gui::settings_screen::section() const -> menu_bar::section {
	return menu_bar::section::settings;
}

auto gse::gui::settings_screen::build(builder& ui, nav&) -> void {
	ui.scroll_region(
		{ .id = "settings.body" },
		[this](builder& b) {
			settings::panel(b, m_panel_state, m_channels, *m_save_reg);
		}
	);
}

auto gse::gui::settings_screen::title() const -> std::string_view {
	return "Settings";
}
