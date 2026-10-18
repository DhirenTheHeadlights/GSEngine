export module sandbox:network_screen;

import std;
import gse;

import :card_backdrop;

export namespace sandbox {
	class network_screen : public gse::gui::screen {
	public:
		network_screen(
			gse::shared_view<gse::network::data> net,
			gse::channel_write<gse::network::connect_request, gse::network::refresh_servers_request, gse::network::refresh_server_info_request, gse::network::ping_request, gse::network::remember_server_request, gse::network::forget_server_request> channels
		);

		auto build(
			gse::gui::builder& ui,
			gse::gui::nav& n
		) -> void override;

		auto title() const -> std::string_view override;

		auto body_rect(
			const gse::gui::style& sty,
			gse::vec2f viewport_size
		) const -> gse::rect_t<gse::vec2f> override;

		auto draw_backdrop(
			gse::gui::draw_context& ctx,
			gse::vec2f viewport_size
		) const -> void override;

	private:
		auto draw_masthead(
			gse::gui::draw_context& ctx,
			const gse::rect_t<gse::vec2f>& rect
		) const -> void;

		auto draw_directory(
			gse::gui::builder& ui,
			const gse::rect_t<gse::vec2f>& rect
		) -> void;

		auto draw_inspector(
			gse::gui::builder& ui,
			const gse::rect_t<gse::vec2f>& rect
		) -> void;

		auto draw_footer(
			gse::gui::builder& ui,
			gse::gui::nav& n,
			const gse::rect_t<gse::vec2f>& rect
		) -> void;

		[[nodiscard]] auto picked() const -> const gse::network::discovery_result*;

		gse::shared_view<gse::network::data> m_net;
		gse::channel_write<gse::network::connect_request, gse::network::refresh_servers_request, gse::network::refresh_server_info_request, gse::network::ping_request, gse::network::remember_server_request, gse::network::forget_server_request> m_channels;
		gse::network::address m_selected;
		std::string m_entry;
		gse::gui::text_input_state m_entry_state;
		std::uint32_t m_ping_seq = 0;
		gse::clock m_refresh_clock;

		static constexpr gse::time refresh_interval = gse::seconds(1.f);
		static constexpr gse::time discovery_timeout = gse::seconds(1.f);
		static constexpr gse::time connect_timeout = gse::seconds(5.f);
		static constexpr gse::time connect_retry = gse::seconds(1.f);
		static constexpr float directory_width_ratio = 0.58f;
	};
}

sandbox::network_screen::network_screen(const gse::shared_view<gse::network::data> net, gse::channel_write<gse::network::connect_request, gse::network::refresh_servers_request, gse::network::refresh_server_info_request, gse::network::ping_request, gse::network::remember_server_request, gse::network::forget_server_request> channels)
	: m_net(net), m_channels(std::move(channels)) {
}

auto sandbox::network_screen::title() const -> std::string_view {
	return "Uplink";
}

auto sandbox::network_screen::body_rect(const gse::gui::style& sty, const gse::vec2f viewport_size) const -> gse::rect_t<gse::vec2f> {
	return ui::card_rect(sty, viewport_size);
}

auto sandbox::network_screen::draw_backdrop(gse::gui::draw_context& ctx, const gse::vec2f viewport_size) const -> void {
	ui::draw_card_backdrop(ctx, viewport_size);
}

auto sandbox::network_screen::draw_masthead(gse::gui::draw_context& ctx, const gse::rect_t<gse::vec2f>& rect) const -> void {
	namespace gui = gse::gui;
	namespace lo = gse::gui::layout;
	const auto& sty = ctx.style;

	gui::draw::label(ctx, rect, title(), sty.screen.title, {
		.color = sty.color_text,
		.clip_rect = rect,
	});

	const bool connected = m_net.connection_state == gse::network::client::state::connected;

	gui::label_spec status_spec = sty.screen.note;
	status_spec.h = lo::halign::end;

	gui::draw::label(
		ctx,
		rect,
		m_net.connection_status.empty() ? std::string_view("disconnected") : std::string_view(m_net.connection_status),
		status_spec,
		{
			.color = connected ? sty.color_added : sty.color_text_secondary,
			.clip_rect = rect,
		}
	);
}

auto sandbox::network_screen::picked() const -> const gse::network::discovery_result* {
	const auto& list = m_net.available_servers;
	const auto it = std::ranges::find(list, m_selected, &gse::network::discovery_result::addr);
	return it == list.end() ? nullptr : &*it;
}

auto sandbox::network_screen::draw_directory(gse::gui::builder& ui, const gse::rect_t<gse::vec2f>& rect) -> void {
	namespace gui = gse::gui;
	namespace lo = gse::gui::layout;

	auto& ctx = ui.ctx;
	auto _ = lo::within(ctx, rect);

	ui.draw<gui::section>({
		.title = "Directory",
		.subtitle = "Discovered sessions on this network",
	});

	ui.draw<gui::text_input>({
		.name = "Address",
		.buffer = m_entry,
		.state = m_entry_state,
	});

	if (ui.draw<gui::button>({
			.text = "Remember",
			.key = "##uplink_remember",
			.glyph = gui::symbol::plus(),
			.enabled = !m_entry.empty(),
		})) {
		m_channels.push<gse::network::remember_server_request>({
			.entry = m_entry,
		});
		m_entry.clear();
		m_entry_state = {};
	}

	for (const auto& sv : m_net.available_servers) {
		if (ui.draw<gui::selectable>({
				.text = ctx.intern(std::format("{}  {}:{}", sv.name, sv.addr.ip, sv.addr.port)),
				.detail = ctx.intern(std::format("{}/{}  v{}", sv.players, sv.max_players, sv.build)),
				.key = ctx.intern(std::format("{}:{}", sv.addr.ip, sv.addr.port)),
				.selected = sv.addr == m_selected,
				.align = gui::selectable_align::left,
			})) {
			m_selected = sv.addr;
		}
	}
}

auto sandbox::network_screen::draw_inspector(gse::gui::builder& ui, const gse::rect_t<gse::vec2f>& rect) -> void {
	namespace gui = gse::gui;
	namespace lo = gse::gui::layout;

	auto& ctx = ui.ctx;
	auto _ = lo::within(ctx, rect);

	const gse::network::discovery_result* target = picked();

	ui.draw<gui::section>({
		.title = "Session",
		.subtitle = target == nullptr ? "Select a session on the left" : std::string_view(target->name),
	});

	if (target != nullptr) {
		ui.draw<gui::value<int>>({
			.name = "Players",
			.val = static_cast<int>(target->players),
		});
		ui.draw<gui::value<int>>({
			.name = "Capacity",
			.val = static_cast<int>(target->max_players),
		});
		ui.draw<gui::value<int>>({
			.name = "Build",
			.val = static_cast<int>(target->build),
		});
	}

	const bool connected = m_net.connection_state == gse::network::client::state::connected;

	if (connected) {
		ui.draw<gui::value<int>>({
			.name = "Pings sent",
			.val = static_cast<int>(m_ping_seq),
		});
		if (ui.draw<gui::button>({
				.text = "Send Ping",
				.key = "##uplink_ping",
			})) {
			m_channels.push<gse::network::ping_request>({
				.sequence = ++m_ping_seq,
			});
		}
	}
}

auto sandbox::network_screen::draw_footer(gse::gui::builder& ui, gse::gui::nav& n, const gse::rect_t<gse::vec2f>& rect) -> void {
	namespace gui = gse::gui;
	namespace lo = gse::gui::layout;

	const auto& sty = ui.ctx.style;
	const gse::network::discovery_result* target = picked();

	const float vertical_inset = (rect.height() - sty.button_height) * 0.5f;
	const gse::rect_t<gse::vec2f> band = lo::inset_per_side(rect, vertical_inset, 0.f, vertical_inset, 0.f);

	const auto [back_cell, spacer, forget_cell, connect_cell] = lo::split_horizontal<4>(
		band,
		{
			lo::size_spec::px(sty.button_min_width),
			lo::size_spec::flex(),
			lo::size_spec::px(sty.button_min_width),
			lo::size_spec::px(sty.accent_button_min_width),
		},
		sty.button_spacing
	);

	if (ui.draw<gui::button>({
			.text = "Back",
			.rect = back_cell,
			.key = "##uplink_back",
			.glyph = gui::symbol::chevron_left(),
			.role = gui::button_role::ghost,
		})) {
		n.pop();
	}

	if (ui.draw<gui::button>({
			.text = "Forget",
			.rect = forget_cell,
			.key = "##uplink_forget",
			.glyph = gui::symbol::trash(),
			.enabled = target != nullptr,
			.role = gui::button_role::danger,
		})) {
		m_channels.push<gse::network::forget_server_request>({
			.entry = target->name,
		});
		m_selected = {};
	}

	if (ui.draw<gui::button>({
			.text = "Connect",
			.rect = connect_cell,
			.key = "##uplink_connect",
			.enabled = target != nullptr,
			.role = gui::button_role::accent,
		})) {
		m_channels.push<gse::network::connect_request>({
			.options = {
				.addr = target->addr,
				.local_bind = gse::network::address{
					.ip = "0.0.0.0",
					.port = 0,
				},
				.timeout = connect_timeout,
				.retry = connect_retry,
			},
		});
	}
}

auto sandbox::network_screen::build(gse::gui::builder& ui, gse::gui::nav& n) -> void {
	namespace lo = gse::gui::layout;
	using spec = lo::size_spec;

	auto& ctx = ui.ctx;
	const auto& sty = ctx.style;

	if (m_refresh_clock.elapsed() > refresh_interval) {
		m_channels.push<gse::network::refresh_servers_request>({
			.timeout = discovery_timeout,
		});
		m_refresh_clock.reset();
	}

	const gse::rect_t<gse::vec2f> content = lo::inset_per_side(
		ctx.current_menu->rect,
		sty.padding,
		sty.padding,
		sty.padding,
		sty.padding + ui::rail_width(sty)
	);

	const auto [masthead, body, footer] = lo::split_vertical<3>(
		content,
		{
			spec::px(sty.header_height),
			spec::flex(),
			spec::px(sty.footer_height),
		}
	);

	draw_masthead(ctx, masthead);
	gse::gui::draw::fill_surface(
		ctx,
		gse::rect_t<gse::vec2f>::from_position_size(
			{ masthead.left(), masthead.bottom() },
			{ masthead.width(), sty.separator_thickness }
		),
		sty.separator_line,
		{}
	);

	const auto [directory, inspector] = lo::split_horizontal<2>(
		body,
		{
			spec::ratio(directory_width_ratio),
			spec::flex(),
		},
		sty.padding
	);

	draw_directory(ui, directory);

	gse::gui::draw::fill_surface(
		ctx,
		gse::rect_t<gse::vec2f>::from_position_size(
			{ inspector.left() - sty.padding * 0.5f, body.top() },
			{ sty.separator_thickness, body.height() }
		),
		sty.separator_line,
		{}
	);

	draw_inspector(ui, inspector);

	gse::gui::draw::fill_surface(
		ctx,
		gse::rect_t<gse::vec2f>::from_position_size(
			{ footer.left(), footer.top() },
			{ footer.width(), sty.separator_thickness }
		),
		sty.separator_line,
		{}
	);

	draw_footer(ui, n, footer);
}
