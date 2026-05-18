export module gs:settings_screen;

import std;
import gse;

export namespace gs {
	class settings_screen : public gse::gui::screen {
	public:
		settings_screen(
			const gse::save::registry& save_reg,
			gse::channel_writer channels
		);

		auto build(
			gse::gui::builder& ui,
			gse::gui::nav& n
		) -> void override;

		auto title(
		) const -> std::string_view override;
	private:
		const gse::save::registry* m_save_reg;
		gse::channel_writer m_channels;
		gse::settings::panel_state m_panel_state;
	};
}

gs::settings_screen::settings_screen(const gse::save::registry& save_reg, gse::channel_writer channels)
	: m_save_reg(&save_reg), m_channels(std::move(channels)) {}

auto gs::settings_screen::build(gse::gui::builder& ui, gse::gui::nav&) -> void {
	ui.scroll_region(
		{ .id = "settings.body" },
		[this](gse::gui::builder& b) {
			gse::settings::panel(b, m_panel_state, m_channels, *m_save_reg);
		}
	);
}

auto gs::settings_screen::title() const -> std::string_view {
	return "Settings";
}
