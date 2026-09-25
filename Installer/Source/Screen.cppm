export module installer:screen;

import std;

import gse;

import :stages;

export namespace installer {
	constexpr float window_width = 560.f;

	class setup_screen : public gse::gui::screen {
	public:
		setup_screen(
			gse::sdk::pack_view pack,
			gse::channel_write<gse::window_launcher_mode_request> channels
		);

		auto build(
			gse::gui::builder& ui,
			gse::gui::nav& n
		) -> void override;

		auto title() const -> std::string_view override;

		auto dismissable() const -> bool override;

		auto occludes() const -> bool override;

		auto wants_chrome() const -> bool override;

		auto body_rect(
			const gse::gui::style& sty,
			gse::vec2f viewport_size
		) const -> gse::rectf override;

		auto draw_backdrop(
			gse::gui::draw_context& ctx,
			gse::vec2f viewport_size
		) const -> void override;

	private:
		enum class phase : std::uint8_t {
			ready,
			installing,
			installed,
			failed,
		};

		auto begin_install() -> void;

		auto build_body(
			gse::gui::builder& ui
		) -> void;

		auto fit_window(
			const gse::gui::builder& ui
		) -> void;

		auto draw_progress(
			const gse::gui::builder& ui
		) const -> void;

		gse::sdk::pack_view m_pack;
		std::string m_destination;
		gse::gui::text_input_state m_input;
		phase m_phase = phase::ready;
		std::shared_ptr<std::atomic<std::size_t>> m_done = std::make_shared<std::atomic<std::size_t>>(0);
		gse::task::pending<std::expected<std::filesystem::path, std::string>> m_job;
		std::string m_status;
		gse::channel_write<gse::window_launcher_mode_request> m_channels;
		int m_requested_height = 0;
	};

	auto set_payload(
		gse::sdk::pack_view pack
	) -> void;

	namespace boot {
		struct [[= gse::system_state<"Installer">{}]] data {
			bool pushed = false;
		};

		[[= gse::system_run<>{}]]
		auto run(
			gse::context& ctx,
			data& d,
			gse::channel_write<gse::gui::push_screen_request, gse::window_launcher_mode_request> ui_out
		) -> gse::async::task<>;
	}
}

namespace installer {
	gse::sdk::pack_view payload;

	auto megabytes(
		std::uint64_t bytes
	) -> std::string;
}

auto installer::megabytes(const std::uint64_t bytes) -> std::string {
	return std::format("{:.1f} MB", static_cast<double>(bytes) / 1048576.0);
}

auto installer::set_payload(gse::sdk::pack_view pack) -> void {
	payload = std::move(pack);
}

auto installer::boot::run(gse::context&, data& d, const gse::channel_write<gse::gui::push_screen_request, gse::window_launcher_mode_request> ui_out) -> gse::async::task<> {
	if (d.pushed) {
		return {};
	}
	d.pushed = true;
	ui_out.push<gse::gui::push_screen_request>({
		.factory = [channels = gse::channel_write<gse::window_launcher_mode_request>(ui_out)] {
			return std::make_unique<setup_screen>(payload, channels);
		},
	});
	return {};
}

installer::setup_screen::setup_screen(gse::sdk::pack_view pack, gse::channel_write<gse::window_launcher_mode_request> channels)
	: m_pack(std::move(pack)), m_destination(default_destination(m_pack.table).generic_native_encoded_string()), m_channels(std::move(channels)) {
}

auto installer::setup_screen::title() const -> std::string_view {
	return "GSEngine SDK Setup";
}

auto installer::setup_screen::dismissable() const -> bool {
	return false;
}

auto installer::setup_screen::occludes() const -> bool {
	return true;
}

auto installer::setup_screen::wants_chrome() const -> bool {
	return true;
}

auto installer::setup_screen::body_rect(const gse::gui::style&, const gse::vec2f viewport_size) const -> gse::rectf {
	return gse::rectf::from_position_size({ 0.f, viewport_size.y() }, viewport_size);
}

auto installer::setup_screen::draw_backdrop(gse::gui::draw_context& ctx, const gse::vec2f viewport_size) const -> void {
	ctx.sprites.push_back({
		.rect = body_rect(ctx.style, viewport_size),
		.color = { gse::vec3f(ctx.style.color_menu_body), 1.f },
		.texture = ctx.blank_texture,
		.layer = gse::render_layer::overlay,
	});
}

auto installer::setup_screen::fit_window(const gse::gui::builder& ui) -> void {
	const gse::gui::draw_context& ctx = ui.ctx;
	if (!ctx.current_menu) {
		return;
	}
	const gse::gui::style& sty = ctx.style;
	const float content = ctx.current_menu->rect.top() - ctx.layout_cursor.y() + sty.padding;
	const int height = static_cast<int>(sty.title_bar_height + content);
	if (height == m_requested_height) {
		return;
	}
	m_channels.push<gse::window_launcher_mode_request>({
		.active = true,
		.width = static_cast<int>(window_width * sty.scale_factor),
		.height = height,
	});
	m_requested_height = height;
}

auto installer::setup_screen::begin_install() -> void {
	m_phase = phase::installing;
	m_done->store(0, std::memory_order_release);
	m_job.start([pack = m_pack, destination = std::filesystem::path(m_destination), done = m_done] {
		return install(pack, destination, *done);
	}, gse::trace_id<"installer::install">(), gse::task::lane::background);
}

auto installer::setup_screen::draw_progress(const gse::gui::builder& ui) const -> void {
	const gse::gui::draw_context& ctx = ui.ctx;
	if (!ctx.current_menu) {
		return;
	}
	const std::size_t total = m_pack.table.entries.size();
	const float fraction = total == 0 ? 1.f : static_cast<float>(m_done->load(std::memory_order_acquire)) / static_cast<float>(total);
	const gse::rectf content = ctx.current_menu->rect.inset({ ctx.style.padding, ctx.style.padding });
	const float height = ctx.style.font_size * 0.5f;

	ctx.layout_cursor.y() -= ctx.style.padding;
	const gse::rectf track = gse::rectf::from_position_size(
		{ content.left(), ctx.layout_cursor.y() },
		{ content.width(), height }
	);
	ctx.queue_sprite({
		.rect = track,
		.color = ctx.style.color_widget_background,
		.texture = ctx.blank_texture,
	});
	ctx.queue_sprite({
		.rect = gse::rectf::from_position_size({ track.left(), track.top() }, { track.width() * std::clamp(fraction, 0.f, 1.f), height }),
		.color = m_phase == phase::failed ? ctx.style.color_error : ctx.style.color_accent,
		.texture = ctx.blank_texture,
	});
	ctx.layout_cursor.y() -= height + ctx.style.padding;
}

auto installer::setup_screen::build(gse::gui::builder& ui, gse::gui::nav&) -> void {
	build_body(ui);
	fit_window(ui);
}

auto installer::setup_screen::build_body(gse::gui::builder& ui) -> void {
	const gse::sdk::pack_table& table = m_pack.table;
	const std::uint64_t total_bytes = std::ranges::fold_left(table.entries, std::uint64_t{ 0 }, [](const std::uint64_t sum, const gse::sdk::pack_entry& entry) {
		return sum + entry.size;
	});

	ui.draw<gse::gui::text>({
		.content = std::format("GSEngine SDK {} ({})", table.version, table.preset),
		.style = { .strong = true },
	});
	ui.draw<gse::gui::text>({
		.content = std::format("{} files, {} on disk", table.entries.size(), megabytes(total_bytes)),
		.style = { .color = ui.ctx.style.color_text_secondary },
	});
	ui.draw<gse::gui::separator>();

	if (m_phase == phase::ready) {
		ui.draw<gse::gui::text>({ .content = "Install to" });
		ui.draw<gse::gui::text_input>({
			.name = "destination",
			.buffer = m_destination,
			.state = m_input,
		});
		ui.draw<gse::gui::text>({
			.content = "Image folder: " + image_path(m_destination, table).generic_display_string(),
			.style = { .color = ui.ctx.style.color_text_secondary },
		});
		if (ui.draw<gse::gui::button>({ .text = "Install", .role = gse::gui::button_role::accent })) {
			begin_install();
		}
		if (ui.draw<gse::gui::button>({ .text = "Cancel" })) {
			gse::shutdown();
		}
		return;
	}

	if (const auto finished = m_job.take()) {
		if (*finished) {
			m_phase = phase::installed;
			m_status = "Installed to " + (*finished)->generic_display_string();
		}
		else {
			m_phase = phase::failed;
			m_status = finished->error();
		}
	}

	draw_progress(ui);
	if (m_phase == phase::installing) {
		ui.draw<gse::gui::text>({
			.content = std::format("{} / {} files", m_done->load(std::memory_order_acquire), table.entries.size()),
			.style = { .color = ui.ctx.style.color_text_secondary },
		});
		return;
	}

	ui.draw<gse::gui::text>({
		.content = m_status,
		.style = { .color = m_phase == phase::failed ? ui.ctx.style.color_error : ui.ctx.style.color_text },
	});
	if (m_phase == phase::installed) {
		ui.draw<gse::gui::text>({
			.content = std::format("Projects bind it with [engine] version = {} in their .gseproj", table.version),
			.style = { .color = ui.ctx.style.color_text_secondary },
		});
	}
	if (ui.draw<gse::gui::button>({ .text = m_phase == phase::failed ? "Back" : "Close" })) {
		if (m_phase == phase::failed) {
			m_phase = phase::ready;
		}
		else {
			gse::shutdown();
		}
	}
}
