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
import :phase;
import :session;
import :system;

auto gse::ide::agent::run(context& ctx, data& d, const channel_read<start_request, dispatch_request, sync_request, gui::context_menu_result, build_runner::build_finished, build_runner::source_changed> requests_in, const channel_write<gui::menu_content, jump_to_request, set_cursor_shape_request, blame_offer, sync_finished, build_runner::build_request> events_out, const shared_view<build_runner::data> build_d) -> async::task<> {
	if (!d.initialized) {
		load_sessions(d);
		adopt_inherited(d);
		resume_interrupted(d);
		triage_crashes(d);
		d.default_model.options.emplace_back();
		for (const model_option& option : available_models()) {
			d.default_model.options.push_back(option.value);
		}
		d.initialized = true;
	}

	if (std::ranges::none_of(d.sessions, is_chat)) {
		create_session(d, config::primary().project_root, task_phase::scope);
	}

	for (session& s : d.sessions) {
		if (s.draft.lines.empty()) {
			s.draft.lines.emplace_back();
		}
	}

	if (session* shown = active_session(d)) {
		hydrate_session(*shown);
	}

	if (std::exchange(d.token_requests_cancelled, false)) {
		for (const sync_request& request : d.token_pending_syncs) {
			events_out.push<sync_finished>({ .root = request.root, .failure = "Claude token setup was cancelled" });
		}
		d.token_pending_starts.clear();
		d.token_pending_syncs.clear();
	}

	for (const start_request& request : requests_in.of<start_request>()) {
		d.token_pending_starts.push_back(request);
	}
	for (const sync_request& request : requests_in.of<sync_request>()) {
		d.token_pending_syncs.push_back(request);
	}
	const bool authenticated = agent_credentials().token;
	if (!authenticated && (!requests_in.of<start_request>().empty() || !requests_in.of<sync_request>().empty())) {
		d.token_prompt_open = true;
	}
	const auto starts = authenticated ? std::exchange(d.token_pending_starts, {}) : std::vector<start_request>{};
	const auto syncs = authenticated ? std::exchange(d.token_pending_syncs, {}) : std::vector<sync_request>{};
	for (const start_request& request : starts) {
		session& started = create_session(d, request.cwd, task_phase::apply);
		if (!launch_session(started)) {
			log::println(log::level::error, log::category::task, "agent: failed to launch 'claude' - is it on PATH?");
			d.sessions.pop_back();
			continue;
		}
		send_to_session(started, request.prompt, {}, true);
	}

	for (const sync_request& request : syncs) {
		if (session* running = sync_session(d)) {
			log::println(log::level::warning, log::category::task, "agent: a sync chat for {} is already open, so the one asked for {} was refused", running->sync->root, request.root);
			events_out.push<sync_finished>({
				.root = request.root,
				.failure = "a sync conversation is already open",
			});
			continue;
		}

		session& started = create_sync_session(d, request);
		if (!launch_session(started)) {
			const std::filesystem::path root = started.sync->root;
			erase_session(d, started.id);
			log::println(log::level::error, log::category::task, "agent: failed to launch 'claude' for the {} sync - is it on PATH?", root);
			events_out.push<sync_finished>({
				.root = root,
				.failure = "could not launch 'claude' - is it on PATH?",
			});
			continue;
		}
		send_to_session(started, sync_opening, {}, true);
	}

	if (session* reported = sync_session(d); reported && reported->sync_report && !is_busy(*reported)) {
		events_out.push<sync_finished>({
			.root = reported->sync->root,
			.summary = *reported->sync_report,
		});
		erase_session(d, reported->id);
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
		if (s.fork.process) {
			pump_inquiry(*s.fork.process);
		}
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
	service_inquiry(d, events_out);

	bool state_changed = poll_build_inbox(d, events_out, build_d.building);
	for (session& s : d.sessions) {
		state_changed = service_phase(s, static_cast<std::uint32_t>(d.goal_steer_budget)) || state_changed;
	}
	if (state_changed) {
		save_sessions(d);
	}

	refresh_presence(d);

	for (const dispatch_request& request : requests_in.of<dispatch_request>()) {
		if (request.session == 0) {
			fix_unclaimed(d);
		}
		else {
			dispatch_blame(d, request.session);
		}
	}

	refresh_build_times(d);
	wake_covered(d);
	refresh_stale(d);
	service_link(d);
	refresh_usage(d);

	events_out.push<gui::menu_content>({
		.menu = std::string(panel_name),
		.layer = render_layer::content,
		.build = [](gui::builder& b, data& panel_d, const channel_write<gui::menu_content, jump_to_request, set_cursor_shape_request> jump_out) {
			draw_panel(b, panel_d, jump_out);
		},
	});

	if (sync_session(d)) {
		events_out.push<gui::menu_content>({
			.menu = std::string(source_control_panel_name),
			.layer = render_layer::content,
			.build = [](gui::builder& b, data& panel_d, const channel_write<gui::menu_content, jump_to_request, set_cursor_shape_request> jump_out) {
				draw_sync_chat(b, panel_d, b.ctx.clip_stack.back(), jump_out);
			},
		});
	}

	return {};
}

auto gse::ide::agent::persist(const data& d) -> void {
	save_sessions(d);
}

auto gse::ide::agent::shutdown(data& d) -> void {
	const bool relaunching = app::relaunch_pending();

	save_sessions(d);
	net::cancel(d.link);
	d.usage_client.reset();

	if (relaunching) {
		app::drop_relaunch_arguments(std::wstring(handoff_option));
	}

	retire_presence(d);
	cancel_inquiry(d);
	hand_off_builds(d, relaunching);

	for (session& s : d.sessions) {
		if (relaunching && hand_off_session(s)) {
			continue;
		}
		close_session(s);
	}
	d.sessions.clear();
}
