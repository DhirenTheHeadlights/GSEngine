module gse.ide.agent:system_impl;

import std;
import gse;
import gse.win32;

import gse.ide.build;
import gse.ide.config;
import gse.ide.navigation;
import gse.ide.net;

import :blame;
import :chats;
import :model;
import :panel;
import :session;
import :system;

auto gse::ide::agent::run(context& ctx, data& d, const channel_read<start_request, dispatch_request, gui::context_menu_result, build_runner::build_finished, build_runner::source_changed> requests_in, const channel_write<gui::menu_content, jump_to_request, set_cursor_shape_request, blame_offer, build_runner::build_request> events_out, const shared_view<asset::data> assets_d, const shared_view<build_runner::data> build_d) -> async::task<> {
	if (!d.initialized) {
		load_sessions(d);
		adopt_inherited(d);
		d.built = build_runner::build_times();
		d.default_model.options.emplace_back();
		for (const model_option& option : available_models()) {
			d.default_model.options.push_back(option.value);
		}
		d.initialized = true;
	}

	if (d.sessions.empty()) {
		create_session(d, config::primary().project_root);
	}

	for (session& s : d.sessions) {
		if (s.draft.lines.empty()) {
			s.draft.lines.emplace_back();
		}
	}

	if (session* shown = active_session(d)) {
		hydrate_session(*shown);
	}

	if (std::optional<window::clipboard_image> pasted = window::take_clipboard_image()) {
		attach_image(d, assets_d, std::move(*pasted));
	}

	for (const start_request& request : requests_in.of<start_request>()) {
		session& started = create_session(d, request.cwd);
		if (!launch_session(started)) {
			log::println(log::level::error, log::category::task, "agent: failed to launch 'claude' - is it on PATH?");
			d.sessions.pop_back();
			continue;
		}
		send_to_session(started, request.prompt, {});
	}

	for (const gui::context_menu_result& result : requests_in.of<gui::context_menu_result>()) {
		if (result.tag != agent_context_tag()) {
			continue;
		}
		const std::uint64_t* packed = std::get_if<std::uint64_t>(&result.target);
		if (!packed) {
			continue;
		}
		const auto owner = std::ranges::find(d.sessions, static_cast<std::uint32_t>(*packed >> 32), &session::id);
		if (owner == d.sessions.end()) {
			continue;
		}

		const auto row = static_cast<std::uint32_t>(*packed & 0xffffffffull);
		std::string prompt = row < owner->rows.size() ? owner->rows[row].text : std::string{};
		if (rewind_session(*owner, row)) {
			fill_input(*owner, prompt);
		}
	}

	for (session& s : d.sessions) {
		if (s.running) {
			pump_session(s);
		}
		else if (win32::valid_handle(s.process)) {
			close_session(s);
		}
	}

	for (const build_runner::source_changed& change : requests_in.of<build_runner::source_changed>()) {
		attribute_change(d, change);
	}

	for (const build_runner::build_finished& finished : requests_in.of<build_runner::build_finished>()) {
		d.built = build_runner::build_times();
		attribute_build_errors(d, finished, events_out);
		if (!finished.inbox_id.empty()) {
			publish_inbox_result(d, finished);
		}
		wake_observers(d, finished);
	}

	accept_hibernations(d);
	poll_build_inbox(d, events_out, build_d.building);

	for (const dispatch_request& request : requests_in.of<dispatch_request>()) {
		if (request.session == 0) {
			fix_unclaimed(d);
		}
		else {
			dispatch_blame(d, request.session);
		}
	}

	refresh_stale(d);
	service_link(d);

	events_out.push<gui::menu_content>({
		.menu = std::string(panel_name),
		.layer = render_layer::content,
		.build = [d = &d, jump_out = events_out](gui::builder& b) {
			draw_panel(b, *d, jump_out);
		},
	});

	return {};
}

auto gse::ide::agent::shutdown(data& d) -> void {
	const bool relaunching = app::relaunch_pending();

	if (!relaunching) {
		for (session& s : d.sessions) {
			if (is_busy(s) && !s.retry.prompt.empty() && !s.retry.waiting) {
				s.retry.waiting = true;
				log::println(log::level::info, log::category::task, "agent: chat '{}' was mid-turn - holding it to resend on the next start", s.name);
			}
		}
	}

	save_sessions(d);
	net::cancel(d.link);

	if (relaunching) {
		app::drop_relaunch_arguments(std::wstring(handoff_option));
	}

	hand_off_builds(d, relaunching);

	for (session& s : d.sessions) {
		if (relaunching && hand_off_session(s)) {
			continue;
		}
		close_session(s);
	}
	d.sessions.clear();
}
