export module launcher:screen;

import std;

import gse;

import :update;

export namespace launcher {
	constexpr float window_width = 520.f;

	class launcher_screen : public gse::gui::screen {
	public:
		launcher_screen(
			install local,
			arguments args,
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
			checking,
			updating,
			launching,
			failed,
		};

		auto begin_update(
			const gse::sdk::feed& entry
		) -> void;

		auto launch_game() -> void;

		auto resolve_check() -> void;

		auto resolve_update() -> void;

		auto build_body(
			gse::gui::builder& ui
		) -> void;

		auto fit_window(
			const gse::gui::builder& ui
		) -> void;

		auto draw_progress(
			const gse::gui::builder& ui
		) const -> void;

		[[nodiscard]] auto fraction() const -> float;

		[[nodiscard]] auto connect_address() const -> std::string;

		install m_local;
		arguments m_args;
		std::string m_title;
		phase m_phase = phase::checking;
		std::string m_status;
		std::optional<gse::sdk::feed> m_feed;
		std::shared_ptr<progress> m_progress = std::make_shared<progress>();
		gse::task::pending<std::expected<std::optional<gse::sdk::feed>, std::string>> m_check;
		gse::task::pending<std::expected<std::filesystem::path, std::string>> m_update;
		gse::channel_write<gse::window_launcher_mode_request> m_channels;
		int m_requested_height = 0;
	};

	auto set_install(
		install local,
		arguments args
	) -> void;

	namespace boot {
		struct [[= gse::system_state<"Launcher">{}]] data {
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

namespace launcher {
	install configured;
	arguments configured_args;

	auto megabytes(
		std::uint64_t bytes
	) -> std::string;
}

auto launcher::megabytes(const std::uint64_t bytes) -> std::string {
	return std::format("{:.1f} MB", static_cast<double>(bytes) / 1048576.0);
}

auto launcher::set_install(install local, arguments args) -> void {
	configured = std::move(local);
	configured_args = std::move(args);
}

auto launcher::boot::run(gse::context&, data& d, const gse::channel_write<gse::gui::push_screen_request, gse::window_launcher_mode_request> ui_out) -> gse::async::task<> {
	if (d.pushed) {
		return {};
	}
	d.pushed = true;
	ui_out.push<gse::gui::push_screen_request>({
		.factory = [channels = gse::channel_write<gse::window_launcher_mode_request>(ui_out)] {
			return std::make_unique<launcher_screen>(configured, configured_args, channels);
		},
	});
	return {};
}

launcher::launcher_screen::launcher_screen(install local, arguments args, gse::channel_write<gse::window_launcher_mode_request> channels)
	: m_local(std::move(local)), m_args(std::move(args)), m_title(m_local.stamp.product), m_channels(std::move(channels)) {
	if (m_args.updated) {
		launch_game();
		return;
	}
	m_status = "Checking for updates";
	m_check.start([local = m_local] {
		return check_feed(local);
	}, gse::trace_id<"launcher::check">(), gse::task::lane::background);
}

auto launcher::launcher_screen::title() const -> std::string_view {
	return m_title;
}

auto launcher::launcher_screen::dismissable() const -> bool {
	return false;
}

auto launcher::launcher_screen::occludes() const -> bool {
	return true;
}

auto launcher::launcher_screen::wants_chrome() const -> bool {
	return true;
}

auto launcher::launcher_screen::body_rect(const gse::gui::style&, const gse::vec2f viewport_size) const -> gse::rectf {
	return gse::rectf::from_position_size({ 0.f, viewport_size.y() }, viewport_size);
}

auto launcher::launcher_screen::draw_backdrop(gse::gui::draw_context& ctx, const gse::vec2f viewport_size) const -> void {
	ctx.sprites.push_back({
		.rect = body_rect(ctx.style, viewport_size),
		.color = { gse::vec3f(ctx.style.color_menu_body), 1.f },
		.texture = ctx.blank_texture,
		.layer = gse::render_layer::overlay,
	});
}

auto launcher::launcher_screen::connect_address() const -> std::string {
	if (!m_args.connect.empty()) {
		return m_args.connect;
	}
	return m_feed ? m_feed->server : std::string{};
}

auto launcher::launcher_screen::launch_game() -> void {
	m_phase = phase::launching;
	m_status = "Starting " + m_local.stamp.product + " " + m_local.stamp.version;
	start_game(m_local, connect_address());
	gse::shutdown();
}

auto launcher::launcher_screen::begin_update(const gse::sdk::feed& entry) -> void {
	m_phase = phase::updating;
	m_status = std::format("Updating to {}", entry.version);
	m_update.start([local = m_local, entry, reported = m_progress] {
		return apply_update(local, entry, *reported);
	}, gse::trace_id<"launcher::update">(), gse::task::lane::background);
}

auto launcher::launcher_screen::resolve_check() -> void {
	const auto finished = m_check.take();
	if (!finished) {
		return;
	}
	if (!*finished) {
		m_phase = phase::failed;
		m_status = finished->error();
		return;
	}
	m_feed = **finished;
	if (m_feed && gse::sdk::is_newer(m_feed->version, m_local.stamp.version)) {
		begin_update(*m_feed);
		return;
	}
	launch_game();
}

auto launcher::launcher_screen::resolve_update() -> void {
	const auto finished = m_update.take();
	if (!finished) {
		return;
	}
	if (!*finished) {
		m_phase = phase::failed;
		m_status = finished->error();
		return;
	}
	m_phase = phase::launching;
	m_status = "Restarting on " + m_feed->version;
	start_launcher(**finished, connect_address());
	gse::shutdown();
}

auto launcher::launcher_screen::fraction() const -> float {
	if (m_progress->extracting.load(std::memory_order_acquire)) {
		const std::size_t files = m_progress->files.load(std::memory_order_acquire);
		return files == 0 ? 1.f : static_cast<float>(m_progress->extracted.load(std::memory_order_acquire)) / static_cast<float>(files);
	}
	const std::uint64_t total = m_progress->total.load(std::memory_order_acquire);
	return total == 0 ? 0.f : static_cast<float>(m_progress->received.load(std::memory_order_acquire)) / static_cast<float>(total);
}

auto launcher::launcher_screen::draw_progress(const gse::gui::builder& ui) const -> void {
	const gse::gui::draw_context& ctx = ui.ctx;
	if (!ctx.current_menu) {
		return;
	}
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
		.rect = gse::rectf::from_position_size({ track.left(), track.top() }, { track.width() * std::clamp(fraction(), 0.f, 1.f), height }),
		.color = m_phase == phase::failed ? ctx.style.color_error : ctx.style.color_accent,
		.texture = ctx.blank_texture,
	});
	ctx.layout_cursor.y() -= height + ctx.style.padding;
}

auto launcher::launcher_screen::fit_window(const gse::gui::builder& ui) -> void {
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

auto launcher::launcher_screen::build(gse::gui::builder& ui, gse::gui::nav&) -> void {
	build_body(ui);
	fit_window(ui);
}

auto launcher::launcher_screen::build_body(gse::gui::builder& ui) -> void {
	if (m_phase == phase::checking) {
		resolve_check();
	}
	else if (m_phase == phase::updating) {
		resolve_update();
	}

	ui.draw<gse::gui::text>({
		.content = std::format("{} {} ({})", m_local.stamp.product, m_local.stamp.version, m_local.stamp.preset),
		.style = { .strong = true },
	});
	ui.draw<gse::gui::text>({
		.content = m_status,
		.style = { .color = m_phase == phase::failed ? ui.ctx.style.color_error : ui.ctx.style.color_text_secondary },
	});
	ui.draw<gse::gui::separator>();

	if (m_phase == phase::updating) {
		draw_progress(ui);
		if (m_progress->extracting.load(std::memory_order_acquire)) {
			ui.draw<gse::gui::text>({
				.content = std::format("Installing {} / {} files", m_progress->extracted.load(std::memory_order_acquire), m_progress->files.load(std::memory_order_acquire)),
				.style = { .color = ui.ctx.style.color_text_secondary },
			});
			return;
		}
		ui.draw<gse::gui::text>({
			.content = std::format("Downloading {} of {}", megabytes(m_progress->received.load(std::memory_order_acquire)), megabytes(m_progress->total.load(std::memory_order_acquire))),
			.style = { .color = ui.ctx.style.color_text_secondary },
		});
		return;
	}

	if (m_phase == phase::failed) {
		ui.draw<gse::gui::text>({
			.content = "The installed build still works.",
			.style = { .color = ui.ctx.style.color_text_secondary },
		});
		if (ui.draw<gse::gui::button>({ .text = "Play " + m_local.stamp.version, .role = gse::gui::button_role::accent })) {
			launch_game();
		}
		if (ui.draw<gse::gui::button>({ .text = "Quit" })) {
			gse::shutdown();
		}
	}
}
