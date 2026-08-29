module gse.ide.agent:blame_impl;

import std;
import gse;

import gse.ide.build;
import gse.ide.config;
import gse.ide.net;

import :blame;
import :model;
import :session;

auto gse::ide::agent::note_source_change(session& s, const std::filesystem::path& file, const std::int64_t mtime) -> id {
	id touched_id;
	for (const id key : build_runner::build_keys_for(file)) {
		const auto [stamp, unseen] = s.unbuilt.try_emplace(key, mtime);
		if (!unseen) {
			stamp->second = std::max(stamp->second, mtime);
		}

		touched_id = config::path_id(file);
		const auto [touch, first] = s.touched.try_emplace(touched_id, touched_source{ .build_key = key, .mtime = mtime });
		if (!first) {
			touch->second.build_key = key;
			touch->second.mtime = std::max(touch->second.mtime, mtime);
		}
	}
	return touched_id;
}

auto gse::ide::agent::note_written_file(session& s, const std::filesystem::path& file) -> void {
	const auto requested = static_cast<std::int64_t>(std::filesystem::file_time_type::clock::now().time_since_epoch().count());
	const auto touch = s.touched.find(note_source_change(s, file, requested));
	if (touch != s.touched.end()) {
		touch->second.wrote = true;
	}
}

auto gse::ide::agent::working_on_files(const session& s) -> bool {
	if (!s.running) {
		return false;
	}
	if (s.think_clock) {
		return true;
	}
	const time settled = seconds(10.f);
	return s.recent_turn && s.recent_turn->elapsed() < settled;
}

auto gse::ide::agent::attribute_change(data& d, const build_runner::source_changed& change) -> void {
	if (build_runner::is_build_touch(change.path, change.mtime)) {
		return;
	}

	const std::string& owner = config::worktree_for(change.path).name;
	for (session& s : d.sessions) {
		if (working_on_files(s) && config::worktree_for(s.cwd).name == owner) {
			note_source_change(s, change.path, change.mtime);
		}
	}
}

auto gse::ide::agent::refresh_stale(data& d) -> void {
	for (session& s : d.sessions) {
		for (auto entry = s.unbuilt.begin(); entry != s.unbuilt.end();) {
			const auto built = d.built.find(entry->first);
			entry = built != d.built.end() && entry->second <= built->second
				? s.unbuilt.erase(entry)
				: std::next(entry);
		}
		for (auto entry = s.touched.begin(); entry != s.touched.end();) {
			const auto built = d.built.find(entry->second.build_key);
			entry = built != d.built.end() && entry->second.mtime <= built->second
				? s.touched.erase(entry)
				: std::next(entry);
		}
		s.stale = !s.unbuilt.empty();
	}
}

auto gse::ide::agent::unbuilt_label(const session& s) -> std::string {
	if (s.unbuilt.empty()) {
		return "up to date";
	}

	std::vector<std::string> pending;
	for (const config::worktree& tree : config::worktrees()) {
		if (s.unbuilt.contains(build_runner::build_key(tree, tree.game_target))) {
			pending.push_back(&tree == &config::primary()
				? tree.game_target
				: std::format("{} ({})", tree.game_target, tree.name));
		}
	}
	if (s.unbuilt.contains(build_runner::build_key(config::primary(), config::editor_target))) {
		pending.emplace_back(config::editor_target);
	}
	if (pending.empty()) {
		return "unbuilt";
	}

	std::string out = pending.front();
	for (const std::string& target : pending | std::views::drop(1)) {
		out += ", " + target;
	}
	return out;
}

auto gse::ide::agent::touch_owner(data& d, const std::span<const std::filesystem::path> files) -> session* {
	session* owner = nullptr;
	touched_source best;
	for (const std::filesystem::path& file : files) {
		const id key = config::path_id(file);
		for (session& s : d.sessions) {
			const auto touch = s.touched.find(key);
			if (touch == s.touched.end()) {
				continue;
			}
			const touched_source& candidate = touch->second;
			if (!owner || std::pair(candidate.wrote, candidate.mtime) > std::pair(best.wrote, best.mtime)) {
				best = candidate;
				owner = &s;
			}
		}
	}
	return owner;
}

auto gse::ide::agent::blame_owner(data& d, const build_runner::build_error& error) -> session* {
	if (session* direct = touch_owner(d, std::span(&error.file, 1))) {
		return direct;
	}
	return touch_owner(d, error.related);
}

auto gse::ide::agent::attribute_build_errors(data& d, const build_runner::build_finished& finished, const channel_write<blame_offer> offers) -> void {
	constexpr std::size_t blame_limit = 8;

	for (session& s : d.sessions) {
		if (s.blame_build == finished.key) {
			s.blame.clear();
			s.blame_build = {};
		}
	}
	if (d.unclaimed_build == finished.key) {
		d.unclaimed.clear();
		d.unclaimed_build = {};
	}

	std::vector<blame_offer> grouped;

	for (const build_runner::build_error& error : finished.errors) {
		session* owner = blame_owner(d, error);
		std::vector<blamed_error>& claim = owner ? owner->blame : d.unclaimed;
		if (claim.size() >= blame_limit) {
			continue;
		}

		(owner ? owner->blame_build : d.unclaimed_build) = finished.key;
		claim.push_back({
			.file = error.file,
			.line = error.line,
			.message = error.message,
			.notes = error.notes,
		});

		if (owner) {
			append_row(*owner, {
				.kind = row_kind::failure,
				.text = std::format("broke the build in {}", error.file.filename().generic_display_string()),
				.detail = std::format("line {}: {}", error.line, error.message),
				.file = error.file,
				.start_line = std::max(error.line, 1u),
			});
		}

		const std::uint32_t claimant = owner ? owner->id : 0;
		const auto opened = std::ranges::find(grouped, claimant, &blame_offer::session);
		if (opened != grouped.end()) {
			++opened->extra;
			continue;
		}

		grouped.push_back({
			.session = claimant,
			.session_name = owner ? owner->name : std::string{},
			.file = error.file,
			.line = error.line,
			.kind = finished.kind,
		});
	}

	for (const blame_offer& offer : grouped) {
		offers.push<blame_offer>(offer);
	}
}

auto gse::ide::agent::dispatch_blame(data& d, const std::uint32_t session_id) -> void {
	const auto found = std::ranges::find(d.sessions, session_id, &session::id);
	if (found == d.sessions.end() || found->blame.empty()) {
		return;
	}

	if (!found->running && !launch_session(*found)) {
		append_row(*found, {
			.kind = row_kind::failure,
			.text = "failed to launch 'claude' - is it on PATH?",
		});
		return;
	}

	d.active = session_id;
	send_to_session(*found, blame_prompt("The build failed on files this session edited:", found->blame), {});
	found->blame.clear();
	found->blame_build = {};
}

auto gse::ide::agent::fix_unclaimed(data& d) -> void {
	if (d.unclaimed.empty()) {
		return;
	}

	const blamed_error& first = d.unclaimed.front();
	session& started = create_session(d, config::worktree_for(first.file).project_root);
	started.name = std::format("fix {}", first.file.filename().generic_display_string());

	if (!launch_session(started)) {
		append_row(started, {
			.kind = row_kind::failure,
			.text = "failed to launch 'claude' - is it on PATH?",
		});
		return;
	}

	send_to_session(started, blame_prompt("The build failed and no chat owns these edits:", d.unclaimed), {});
	d.unclaimed.clear();
	d.unclaimed_build = {};
}

auto gse::ide::agent::blame_label(const session& s) -> std::string {
	if (s.blame.empty()) {
		return "-";
	}

	const blamed_error& first = s.blame.front();
	const std::string head = std::format("{}:{}", first.file.filename().generic_display_string(), first.line);
	return s.blame.size() > 1
		? std::format("{} +{}", head, s.blame.size() - 1)
		: head;
}

auto gse::ide::agent::blame_prompt(const std::string_view lead, const std::span<const blamed_error> errors) -> std::string {
	std::string prompt(lead);
	prompt += '\n';
	for (const blamed_error& error : errors) {
		prompt += std::format("{}:{}: error: {}\n", error.file.generic_display_string(), error.line, error.message);
		for (const std::string& note : error.notes) {
			prompt += "  " + note + "\n";
		}
	}
	prompt += "Fix them.";
	return prompt;
}

auto gse::ide::agent::arm_retry(session& s) -> void {
	if (s.retry.prompt.empty()) {
		return;
	}

	s.retry.waiting = true;
	++s.retry.attempts;
	append_row(s, {
		.kind = row_kind::note,
		.text = "holding this turn - it will be resent once the connection is back",
	});
}

auto gse::ide::agent::link_delay(const data& d) -> time {
	const time first = seconds(15.f);
	const time longest = seconds(300.f);
	return std::min<time>(first * static_cast<float>(1u << std::min(d.link_misses, 5u)), longest);
}

auto gse::ide::agent::resume_waiting(data& d) -> void {
	for (session& s : d.sessions) {
		if (!s.retry.waiting) {
			continue;
		}
		if (!s.running && !launch_session(s)) {
			continue;
		}

		const std::string prompt = s.retry.prompt;
		std::vector<attachment> images;
		images.reserve(s.retry.images.size());
		for (const std::filesystem::path& file : s.retry.images) {
			images.push_back({
				.path = file,
			});
		}

		append_row(s, {
			.kind = row_kind::note,
			.text = std::format("retrying the interrupted turn (attempt {})", s.retry.attempts + 1),
		});
		send_to_session(s, prompt, images);
	}
}

auto gse::ide::agent::service_link(data& d) -> void {
	constexpr std::uint32_t blind_retry_after = 6;

	const bool waiting = std::ranges::any_of(d.sessions, [](const session& s) {
		return s.retry.waiting;
	});

	if (!waiting) {
		net::cancel(d.link);
		d.link_clock.reset();
		d.link_misses = 0;
		return;
	}

	if (!d.link_clock) {
		d.link_clock.emplace();
	}

	if (net::active(d.link)) {
		const net::reach state = net::poll(d.link);
		if (state == net::reach::pending) {
			return;
		}

		if (state == net::reach::up) {
			d.link_misses = 0;
			resume_waiting(d);
		}
		else if (++d.link_misses >= blind_retry_after) {
			resume_waiting(d);
		}

		d.link_clock.emplace();
		return;
	}

	if (d.link_clock->elapsed() >= link_delay(d)) {
		net::begin(d.link);
	}
}

auto gse::ide::agent::link_label(const data& d, const session& s) -> std::string {
	if (!s.retry.waiting) {
		return {};
	}
	if (net::active(d.link)) {
		return std::format("offline - checking the connection (attempt {})", s.retry.attempts);
	}

	const time waited = d.link_clock ? d.link_clock->elapsed() : time{};
	const time remaining = std::max<time>(link_delay(d) - waited, time{});
	return std::format("offline - retrying in {:.0f}s (attempt {})", remaining.as<seconds>(), s.retry.attempts);
}