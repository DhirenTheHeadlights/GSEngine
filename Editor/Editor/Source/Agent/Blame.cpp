module gse.ide.agent:blame_impl;

import gse;
import gse.ide.build;
import gse.ide.config;
import gse.ide.net;
import std;

import :blame;
import :model;
import :phase;
import :session;
import :stream;

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
		touch->second.file = file;
	}
	return touched_id;
}

auto gse::ide::agent::note_written_file(session& s, const std::filesystem::path& file, const std::span<const std::string> added) -> void {
	const auto requested = static_cast<std::int64_t>(std::filesystem::file_time_type::clock::now().time_since_epoch().count());
	const auto touch = s.touched.find(note_source_change(s, file, requested));
	if (touch != s.touched.end()) {
		touch->second.wrote = true;
		if (!added.empty()) {
			touch->second.hunks.emplace_back(added.begin(), added.end());
		}
	}
	s.wrote_this_turn = true;
	s.last_write.emplace();
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

auto gse::ide::agent::is_busy(const session& s) -> bool {
	return s.running && s.think_clock.has_value();
}

auto gse::ide::agent::edit_settle() -> time {
	return seconds(8.f);
}

auto gse::ide::agent::editing_tree(const session& s) -> bool {
	if (!s.running || s.hibernating) {
		return false;
	}
	if (is_busy(s) && (s.wrote_this_turn || !s.unbuilt.empty())) {
		return true;
	}
	return s.last_write && s.last_write->elapsed() < edit_settle();
}

auto gse::ide::agent::blocks_builds(const data& d, const queued_build& queued, const session& s) -> bool {
	if (!editing_tree(s)) {
		return false;
	}
	if (!queued.agent.empty() && queued.agent == s.info.agent_id) {
		return false;
	}

	const queued_build* theirs = queued_for(d, s);
	if (!theirs) {
		return true;
	}
	if (!s.last_write) {
		return false;
	}

	const time wrote_at = system_clock::now<time>() - s.last_write->elapsed();
	return wrote_at > theirs->requested && theirs->requested < queued.requested;
}

auto gse::ide::agent::build_wait_for(const data& d, const session& s) -> build_wait {
	if (s.info.agent_id.empty()) {
		return build_wait::none;
	}

	const auto claimed_by_session = [&s](const queued_build& queued) {
		return queued.agent == s.info.agent_id;
	};
	if (std::ranges::any_of(d.inbox_active, claimed_by_session)) {
		return build_wait::building;
	}
	if (queued_for(d, s)) {
		return build_wait::queued;
	}
	return build_wait::none;
}

auto gse::ide::agent::queued_for(const data& d, const session& s) -> const queued_build* {
	if (s.info.agent_id.empty()) {
		return nullptr;
	}

	const auto found = std::ranges::find(d.inbox_queue, s.info.agent_id, &queued_build::agent);
	return found == d.inbox_queue.end() ? nullptr : &*found;
}

auto gse::ide::agent::state_of(const data& d, const session& s) -> agent_state {
	if (s.hibernating) {
		return agent_state::hibernating;
	}
	if (s.retry.waiting && s.limited_until > unix_now()) {
		return agent_state::rate_limited;
	}

	switch (build_wait_for(d, s)) {
		case build_wait::building:
			return agent_state::compiling;
		case build_wait::queued:
			return agent_state::build_queued;
		case build_wait::none:
			break;
	}

	if (!s.running) {
		if (s.goal && s.goal->waiting) {
			return agent_state::awaiting_goal;
		}
		return s.info.failure.empty() ? agent_state::resting : agent_state::failed;
	}
	if (s.think_clock) {
		return s.wrote_this_turn ? agent_state::editing : agent_state::working;
	}
	if (awaiting_answer(s)) {
		return agent_state::awaiting_answer;
	}
	return s.gate ? agent_state::awaiting_approval : agent_state::idle;
}

auto gse::ide::agent::style_of(const agent_state state) -> state_style {
	return annotation_from_enum<state_style>(state, {});
}

auto gse::ide::agent::kind_of(const session& s) -> chat_kind {
	if (s.sync) {
		return chat_kind::sync;
	}
	return s.goal ? chat_kind::goal : s.kind;
}

auto gse::ide::agent::style_of(const chat_kind kind) -> kind_style {
	return annotation_from_enum<kind_style>(kind, {});
}

auto gse::ide::agent::status_label(const data& d, const session& s) -> std::string {
	const agent_state state = state_of(d, s);
	std::string out = style_of(state).label;
	if (state == agent_state::rate_limited) {
		out += std::format(" - resuming in {}m", std::max<std::int64_t>((s.limited_until - unix_now()) / 60, 1));
	}
	return out;
}

auto gse::ide::agent::holding_builds(const data& d, const session& s) -> std::size_t {
	if (s.info.agent_id.empty()) {
		return 0;
	}

	return static_cast<std::size_t>(std::ranges::count_if(d.inbox_queue, [&s](const queued_build& queued) {
		return queued.reported == build_hold::tree_busy && queued.blocker == s.info.agent_id;
	}));
}

auto gse::ide::agent::exe_label(const session& s) -> std::string {
	if (s.touched.empty() && s.unbuilt.empty()) {
		return "no edits";
	}
	return s.unbuilt.empty() ? "in the current exe" : "NOT in the exe: " + unbuilt_label(s);
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

auto gse::ide::agent::refresh_build_times(data& d) -> void {
	const time now = system_clock::now<time>();
	if (now < d.next_built_poll) {
		return;
	}
	d.next_built_poll = now + seconds(1.f);
	d.built = build_runner::build_times();
}

auto gse::ide::agent::built_through(const data& d, const id key) -> std::int64_t {
	const auto built = d.built.find(key);
	return built != d.built.end() ? built->second : 0;
}

auto gse::ide::agent::edits_built(const data& d, const session& s) -> bool {
	return std::ranges::all_of(s.unbuilt, [&d](const auto& entry) {
		return entry.second <= built_through(d, entry.first);
	});
}

auto gse::ide::agent::refresh_stale(data& d) -> void {
	for (session& s : d.sessions) {
		for (auto entry = s.unbuilt.begin(); entry != s.unbuilt.end();) {
			entry = entry->second <= built_through(d, entry->first)
				? s.unbuilt.erase(entry)
				: std::next(entry);
		}
		for (auto entry = s.touched.begin(); entry != s.touched.end();) {
			entry = entry->second.mtime <= built_through(d, entry->second.build_key)
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

auto gse::ide::agent::hunk_owner(data& d, const std::filesystem::path& file, const std::uint32_t line) -> session* {
	const id key = config::path_id(file);
	std::vector<std::string> lines;
	session* owner = nullptr;
	std::int64_t latest = 0;
	for (session& s : d.sessions) {
		const auto touch = s.touched.find(key);
		if (touch == s.touched.end() || touch->second.hunks.empty()) {
			continue;
		}
		if (lines.empty()) {
			lines = file_lines(file);
		}
		for (const std::vector<std::string>& hunk : touch->second.hunks) {
			const std::optional<std::uint32_t> start = locate_lines(lines, hunk);
			const bool covers = start && line >= *start && line < *start + hunk.size();
			if (covers && (!owner || touch->second.mtime > latest)) {
				owner = &s;
				latest = touch->second.mtime;
			}
		}
	}
	return owner;
}

auto gse::ide::agent::blame_owner(data& d, const build_runner::build_error& error) -> blame_match {
	if (session* exact = hunk_owner(d, error.file, error.line)) {
		return { .owner = exact, .at_line = true };
	}
	if (session* direct = touch_owner(d, std::span(&error.file, 1))) {
		return { .owner = direct };
	}
	for (const std::filesystem::path& related : error.related) {
		if (session* via_related = touch_owner(d, std::span(&related, 1))) {
			return { .owner = via_related, .via = related };
		}
	}
	return dependency_owner(d, error.file);
}

auto gse::ide::agent::trim_blank(const std::string_view text) -> std::string_view {
	const std::size_t first = text.find_first_not_of(" \t\r");
	if (first == std::string_view::npos) {
		return {};
	}
	return text.substr(first, text.find_last_not_of(" \t\r") + 1 - first);
}

auto gse::ide::agent::declared_module(const std::span<const std::string> lines) -> std::string {
	constexpr std::string_view exported = "export module ";
	for (const std::string& line : lines) {
		const std::string_view text = trim_blank(line);
		if (text.starts_with(exported) && text.ends_with(';')) {
			return std::string(trim_blank(text.substr(exported.size(), text.size() - exported.size() - 1)));
		}
	}
	return {};
}

auto gse::ide::agent::imported_modules(const std::span<const std::string> lines) -> std::vector<std::string> {
	std::string own;
	std::vector<std::string> out;
	for (const std::string& line : lines) {
		std::string_view text = trim_blank(line);
		if (!text.ends_with(';')) {
			continue;
		}
		text.remove_suffix(1);
		const bool exported = text.starts_with("export ");
		if (exported) {
			text = trim_blank(text.substr(7));
		}

		if (text.starts_with("module ")) {
			own = std::string(trim_blank(text.substr(7)));
			const std::size_t partition = own.find(':');
			if (!exported && partition == std::string::npos) {
				out.push_back(own);
			}
			own = own.substr(0, partition);
			continue;
		}
		if (!text.starts_with("import ")) {
			continue;
		}

		const std::string_view name = trim_blank(text.substr(7));
		if (name.starts_with(':')) {
			out.push_back(own + std::string(name));
		}
		else if (!name.starts_with('<') && !name.starts_with('"')) {
			out.emplace_back(name);
		}
	}
	return out;
}

auto gse::ide::agent::dependency_owner(data& d, const std::filesystem::path& file) -> blame_match {
	const std::vector<std::string> imports = imported_modules(file_lines(file));
	if (imports.empty()) {
		return {};
	}

	std::vector<std::pair<std::filesystem::path, std::string>> declared;
	blame_match best;
	std::int64_t latest = 0;
	for (session& s : d.sessions) {
		for (const touched_source& touch : s.touched | std::views::values) {
			if (touch.file.empty() || touch.file == file) {
				continue;
			}

			auto known = std::ranges::find(declared, touch.file, &decltype(declared)::value_type::first);
			if (known == declared.end()) {
				declared.emplace_back(touch.file, declared_module(file_lines(touch.file)));
				known = std::prev(declared.end());
			}
			if (known->second.empty() || !std::ranges::contains(imports, known->second)) {
				continue;
			}

			if (!best.owner || touch.mtime > latest) {
				best = { .owner = &s, .via = touch.file };
				latest = touch.mtime;
			}
		}
	}
	return best;
}

auto gse::ide::agent::attribute_build_errors(data& d, const build_runner::build_finished& finished, const channel_write<blame_offer> offers) -> void {
	constexpr std::size_t blame_limit = 8;

	const std::vector<blamed_error> asked = d.inquiry && d.inquiry->build == finished.key
		? d.unclaimed
		: std::vector<blamed_error>{};

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

	if (!finished.succeeded && finished.errors.empty() && !finished.output.empty()) {
		d.unclaimed = { blamed_error{ .message = "the build failed without a diagnostic the editor could place on a line", .output = finished.output } };
		d.unclaimed_build = finished.key;
		grouped.push_back({ .kind = finished.kind });
	}

	for (const build_runner::build_error& error : finished.errors) {
		const blame_match match = blame_owner(d, error);
		session* owner = match.owner;
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
			.via = match.via,
		});

		if (owner) {
			append_row(*owner, {
				.kind = row_kind::failure,
				.text = match.via.empty()
					? std::format("broke the build in {}", error.file.filename().generic_display_string())
					: std::format("your edit to {} likely broke {}", match.via.filename().generic_display_string(), error.file.filename().generic_display_string()),
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

	if (!d.unclaimed.empty()) {
		d.unclaimed_tree = finished.tree;
	}

	const auto same_errors = [](const blamed_error& a, const blamed_error& b) {
		return a.file == b.file && a.line == b.line && a.message == b.message;
	};
	const bool still_asking = !d.unclaimed.empty() && std::ranges::equal(asked, d.unclaimed, same_errors);
	if (d.inquiry && d.inquiry->build == finished.key && !still_asking) {
		cancel_inquiry(d);
	}
	if (still_asking) {
		d.inquiry->run = finished.run;
	}

	for (blame_offer& offer : grouped) {
		offer.run = finished.run;
		if (offer.session == 0) {
			offer.asking = still_asking
				? static_cast<std::uint32_t>(d.inquiry->forks.size())
				: start_inquiry(d, finished.kind, finished.run);
		}
		offers.push<blame_offer>(offer);
	}
}

auto gse::ide::agent::inquiry_patience() -> time {
	return seconds(90.f);
}

auto gse::ide::agent::inquiry_candidates(data& d) -> std::vector<session*> {
	std::vector<session*> out;
	for (session& s : d.sessions) {
		if (is_chat(s) && !s.info.agent_id.empty() && !s.touched.empty() && config::worktree_for(s.cwd).name == d.unclaimed_tree) {
			out.push_back(&s);
		}
	}
	return out;
}

auto gse::ide::agent::inquiry_prompt(const std::span<const blamed_error> errors) -> std::string {
	std::string prompt =
		"The editor is asking you a question from a throwaway copy of this conversation. Nothing you do here reaches the real chat, and you can only read files.\n\n"
		"The latest build failed, and no chat in the editor can be tied to it by the lines it edited:\n";
	prompt += describe_errors(errors);
	prompt +=
		"\nDid the edits you made in this conversation cause any of them - for example by changing a module, signature or declaration these files depend on? "
		"Read the code if you need to, then explain in a sentence or two. "
		"End your reply with a final line that is exactly CLAIM if your edits caused them, or exactly NOT_MINE if they did not.";
	return prompt;
}

auto gse::ide::agent::start_inquiry(data& d, const build_runner::stream_kind kind, const std::uint32_t run) -> std::uint32_t {
	cancel_inquiry(d);

	unclaimed_inquiry inquiry{
		.build = d.unclaimed_build,
		.run = run,
		.kind = kind,
	};
	const std::string prompt = inquiry_prompt(d.unclaimed);
	for (const session* candidate : inquiry_candidates(d)) {
		inquiry_fork fork = launch_inquiry(*candidate, prompt);
		if (fork.verdict == inquiry_verdict::pending) {
			inquiry.forks.push_back(std::move(fork));
		}
	}

	const auto asking = static_cast<std::uint32_t>(inquiry.forks.size());
	log::println(log::level::info, log::category::task, "agent: no chat owns {} build error(s), asking {} chat(s) whether they caused them", d.unclaimed.size(), asking);
	d.inquiry = std::move(inquiry);
	return asking;
}

auto gse::ide::agent::cancel_inquiry(data& d) -> void {
	if (!d.inquiry) {
		return;
	}
	for (inquiry_fork& fork : d.inquiry->forks) {
		end_inquiry(fork);
	}
	d.inquiry.reset();
}

auto gse::ide::agent::service_inquiry(data& d, const channel_write<blame_offer> offers) -> void {
	if (!d.inquiry) {
		return;
	}
	if (d.unclaimed.empty() || d.unclaimed_build != d.inquiry->build) {
		cancel_inquiry(d);
		return;
	}

	const bool expired = d.inquiry->asked.elapsed() > inquiry_patience();
	for (inquiry_fork& fork : d.inquiry->forks) {
		if (expired) {
			end_inquiry(fork);
		}
		else {
			pump_inquiry(fork);
		}
	}
	const bool answered = std::ranges::none_of(d.inquiry->forks, [](const inquiry_fork& fork) {
		return fork.verdict == inquiry_verdict::pending;
	});
	if (!answered) {
		return;
	}

	unclaimed_inquiry finished = std::move(*d.inquiry);
	d.inquiry.reset();

	const auto session_of = [&d](const inquiry_fork& fork) -> session* {
		const auto found = std::ranges::find(d.sessions, fork.session, &session::id);
		return found == d.sessions.end() ? nullptr : &*found;
	};
	const auto latest_write = [](const session& s) {
		return std::ranges::max(s.touched | std::views::values | std::views::transform(&touched_source::mtime));
	};

	session* owner = nullptr;
	const inquiry_fork* confession = nullptr;
	std::vector<std::string> also;
	std::vector<std::string> denied;
	for (const inquiry_fork& fork : finished.forks) {
		session* s = session_of(fork);
		if (!s) {
			continue;
		}
		const std::string name = s->name.empty() ? "unnamed" : s->name;
		if (fork.verdict == inquiry_verdict::denied) {
			denied.push_back(name);
			continue;
		}
		if (fork.verdict != inquiry_verdict::claimed || s->touched.empty()) {
			continue;
		}
		if (!owner || latest_write(*s) > latest_write(*owner)) {
			if (owner) {
				also.push_back(owner->name.empty() ? "unnamed" : owner->name);
			}
			owner = s;
			confession = &fork;
		}
		else {
			also.push_back(name);
		}
	}

	if (!owner) {
		log::println(log::level::info, log::category::task, "agent: no asked chat claimed the unowned build error(s) ({} denied)", denied.size());
		const bool fixing = std::ranges::any_of(d.sessions, [](const session& s) {
			return s.kind == chat_kind::build_error && current_phase(s) != task_phase::settled;
		});
		if (d.auto_fix_unclaimed && !fixing) {
			fix_unclaimed(d, denied);
		}
		return;
	}

	const blamed_error& first = d.unclaimed.front();
	append_row(*owner, {
		.kind = row_kind::failure,
		.text = std::format("asked whether your edits broke {}, a copy of this chat said they did", first.file.empty() ? std::string("the build") : first.file.filename().generic_display_string()),
		.detail = also.empty()
			? confession->answer
			: std::format("{}\nalso claimed by: {}", confession->answer, also | std::views::join_with(std::string_view(", ")) | std::ranges::to<std::string>()),
		.file = first.file,
		.start_line = std::max(first.line, 1u),
	});
	offers.push<blame_offer>({
		.session = owner->id,
		.session_name = owner->name,
		.file = first.file,
		.line = first.line,
		.extra = static_cast<std::uint32_t>(d.unclaimed.size() - 1),
		.run = finished.run,
		.kind = finished.kind,
	});
	owner->blame.insert(owner->blame.end(), d.unclaimed.begin(), d.unclaimed.end());
	owner->blame_build = d.unclaimed_build;
	d.unclaimed.clear();
	d.unclaimed_build = {};
}

auto gse::ide::agent::build_patience() -> time {
	return seconds(120.f);
}

auto gse::ide::agent::blocker_for(const data& d, const queued_build& queued) -> std::string {
	const config::worktree& tree = queued.tree ? *queued.tree : config::primary();

	for (const session& s : d.sessions) {
		if (!blocks_builds(d, queued, s)) {
			continue;
		}
		if (config::worktree_for(s.cwd).name != tree.name) {
			continue;
		}
		return s.info.agent_id.empty() ? std::format("chat {}", s.id) : s.info.agent_id;
	}

	const auto known_here = [&d](const build_inbox::presence& active) {
		return std::ranges::any_of(d.sessions, [&active](const session& s) {
			return s.info.agent_id == active.agent;
		});
	};
	for (const build_inbox::presence& active : d.presence) {
		if (active.agent == queued.agent || active.tree != tree.name || known_here(active)) {
			continue;
		}
		return active.agent;
	}
	return {};
}

auto gse::ide::agent::active_elsewhere(const data& d, const queued_build& queued) -> std::string {
	const config::worktree& tree = queued.tree ? *queued.tree : config::primary();

	std::vector<std::string_view> names;
	for (const build_inbox::presence& active : d.presence) {
		const bool here = std::ranges::any_of(d.sessions, [&active](const session& s) {
			return s.info.agent_id == active.agent;
		});
		if (here || active.tree != tree.name) {
			continue;
		}
		names.push_back(active.name.empty() ? std::string_view("unnamed") : std::string_view(active.name));
	}
	if (names.empty()) {
		return {};
	}

	std::string out = "another editor is mid-edit in this tree: ";
	for (const auto [position, name] : std::views::enumerate(names)) {
		out += position == 0 ? "" : ", ";
		out += name;
	}
	return out;
}

auto gse::ide::agent::refresh_presence(data& d) -> void {
	const time now = system_clock::now<time>();
	const bool heartbeat = now >= d.next_presence_write;
	if (heartbeat) {
		d.next_presence_write = now + seconds(5.f);
	}

	for (session& s : d.sessions) {
		const bool announce = editing_tree(s) && !s.info.agent_id.empty();
		if (!announce) {
			if (s.published_presence) {
				s.published_presence = false;
				build_inbox::clear_presence(s.info.agent_id);
			}
			continue;
		}
		if (!s.published_presence || heartbeat) {
			s.published_presence = true;
			build_inbox::publish_presence({
				.agent = s.info.agent_id,
				.name = s.name,
				.tree = config::worktree_for(s.cwd).name,
			});
		}
	}
}

auto gse::ide::agent::retire_presence(data& d) -> void {
	for (session& s : d.sessions) {
		if (s.published_presence) {
			s.published_presence = false;
			build_inbox::clear_presence(s.info.agent_id);
		}
	}
}

auto gse::ide::agent::hold_for(const data& d, const queued_build& queued, const bool building) -> build_hold_state {
	const auto rebuilding_editor = [](const queued_build& active) {
		return active.target == build_runner::build_target::editor;
	};
	if (std::ranges::any_of(d.inbox_active, rebuilding_editor)) {
		return { .reason = build_hold::editor_restart };
	}
	if (building || !d.inbox_active.empty()) {
		return { .reason = build_hold::building };
	}
	if (queued.forced || queued.run_only) {
		return {};
	}
	if (std::string blocker = blocker_for(d, queued); !blocker.empty()) {
		return {
			.reason = build_hold::tree_busy,
			.blocker = std::move(blocker),
		};
	}
	return {};
}

auto gse::ide::agent::blocker_name(const data& d, const std::string_view blocker) -> std::string {
	if (blocker.empty()) {
		return {};
	}

	for (const session& s : d.sessions) {
		if (s.info.agent_id == blocker) {
			return s.name.empty() ? "unnamed" : s.name;
		}
	}
	for (const build_inbox::presence& active : d.presence) {
		if (active.agent == blocker) {
			return std::format("{} (another editor)", active.name.empty() ? "unnamed" : active.name);
		}
	}
	return std::string(blocker);
}

auto gse::ide::agent::hold_message(const build_hold_state& hold, const std::string_view blocker) -> std::string {
	const hold_style style = annotation_from_enum<hold_style>(hold.reason, {});

	switch (hold.reason) {
		case build_hold::tree_busy:
			return std::format(
				"{} - chat '{}' is still editing, so the tree is mid-change and a build now would fail on their half-written files. Your build is held until their edits settle, and the owner can release it early with 'Build now' in the agent panel. You are not blocked: call gse_hibernate with what to do next and you will be woken with the result, or do unrelated work and re-run.",
				style.label,
				blocker.empty() ? std::string_view("unnamed") : blocker
			);
		case build_hold::editor_restart:
			return std::format("{} - your request is preserved across the restart and resumes automatically", style.label);
		case build_hold::building:
			return std::format("{} - you are queued behind it", style.label);
		case build_hold::none:
			break;
	}
	return {};
}

auto gse::ide::agent::hold_label(const data& d, const queued_build& queued) -> std::string {
	if (queued.reported == build_hold::none) {
		return "starting";
	}

	const hold_style style = annotation_from_enum<hold_style>(queued.reported, {});
	if (queued.reported != build_hold::tree_busy) {
		return style.label;
	}

	return std::format("held until '{}' finishes editing", blocker_name(d, queued.blocker));
}

auto gse::ide::agent::queue_label(const queued_build& queued) -> std::string {
	std::string out = queued.target == build_runner::build_target::editor ? "editor" : "game";
	if (queued.tree) {
		out += " \xC2\xB7 " + queued.tree->name;
	}
	if (queued.run) {
		out += " \xC2\xB7 run";
	}
	return out;
}

auto gse::ide::agent::accept_requests(data& d) -> void {
	const time now = system_clock::now<time>();

	for (build_inbox::request& incoming : build_inbox::peek_requests()) {
		const config::worktree* origin = incoming.cwd.empty() ? &config::primary() : config::owning_worktree(incoming.cwd);
		if (!origin) {
			continue;
		}
		if (!incoming.project.empty()) {
			std::error_code ec;
			const bool same_project = std::filesystem::equivalent(incoming.project, config::project_root(), ec);
			if (ec || !same_project) {
				continue;
			}
		}
		build_inbox::consume_request(incoming.id);

		queued_build queued{
			.id = std::move(incoming.id),
			.agent = std::move(incoming.agent),
			.profile = std::move(incoming.profile),
			.config = std::move(incoming.config),
			.cwd = std::move(incoming.cwd),
			.project = std::move(incoming.project),
			.settings = std::move(incoming.settings),
			.scenario = std::move(incoming.scenario),
			.exit_after = incoming.exit_after,
			.requested = now,
		};

		if (incoming.target == "editor") {
			queued.target = build_runner::build_target::editor;
		}
		else if (incoming.target != "game") {
			build_inbox::publish({
				.id = queued.id,
				.outcome = build_inbox::status::rejected,
				.lines = { std::format("unknown build target '{}'; expected 'game' or 'editor'", incoming.target) },
			});
			continue;
		}
		queued.run_only = incoming.run_only;
		queued.run = incoming.run || incoming.run_only;
		queued.tree = queued.target == build_runner::build_target::editor ? nullptr : origin;

		if (queued.run_only && queued.target == build_runner::build_target::editor) {
			build_inbox::publish({
				.id = queued.id,
				.outcome = build_inbox::status::rejected,
				.lines = { "the editor cannot be launched as a run target - it is the process answering you" },
			});
			continue;
		}

		if (!incoming.tree.empty()) {
			const std::span<const config::worktree> trees = config::worktrees();
			const auto named = std::ranges::find(trees, incoming.tree, &config::worktree::name);
			if (named == trees.end()) {
				build_inbox::publish({
					.id = queued.id,
					.outcome = build_inbox::status::rejected,
					.lines = { std::format("unknown worktree '{}'", incoming.tree) },
				});
				continue;
			}
			queued.tree = &*named;
		}

		const auto same_slot = [&queued](const queued_build& existing) {
			return !existing.agent.empty() && existing.agent == queued.agent
				&& existing.target == queued.target
				&& existing.run == queued.run
				&& existing.run_only == queued.run_only
				&& existing.settings == queued.settings
				&& existing.scenario == queued.scenario
				&& existing.exit_after == queued.exit_after
				&& existing.tree == queued.tree
				&& existing.profile == queued.profile
				&& existing.config == queued.config;
		};
		if (const auto held = std::ranges::find_if(d.inbox_queue, same_slot); held != d.inbox_queue.end()) {
			log::println(log::level::info, log::category::task, "build inbox: agent '{}' re-attached to its queued {} build ({} -> {})", queued.agent, incoming.target, held->id, queued.id);
			held->id = std::move(queued.id);
			held->reported = build_hold::none;
			held->blocker.clear();
			continue;
		}

		log::println(log::level::info, log::category::task, "build inbox: agent '{}' queued a {} build ({})", queued.agent, incoming.target, queued.id);
		d.inbox_queue.push_back(std::move(queued));
	}
}

auto gse::ide::agent::report_hold(queued_build& queued, const build_hold_state& hold, const std::string_view blocker) -> void {
	if (queued.reported == hold.reason && queued.blocker == hold.blocker) {
		return;
	}
	queued.reported = hold.reason;
	queued.blocker = hold.blocker;

	if (hold.reason == build_hold::none) {
		build_inbox::withdraw(queued.id);
		return;
	}
	build_inbox::publish({
		.id = queued.id,
		.outcome = build_inbox::status::waiting,
		.lines = { hold_message(hold, blocker) },
	});
}

auto gse::ide::agent::claimed_session(data& d, const std::string_view agent, const std::filesystem::path& cwd) -> session* {
	if (!cwd.empty() && !config::owning_worktree(cwd)) {
		return nullptr;
	}

	const auto found = std::ranges::find_if(d.sessions, [agent](const session& s) {
		return s.info.agent_id == agent;
	});
	return found != d.sessions.end() ? &*found : nullptr;
}

auto gse::ide::agent::accept_hibernations(data& d) -> bool {
	bool recorded = false;

	for (build_inbox::hibernate_request& incoming : build_inbox::peek_hibernations()) {
		session* found = claimed_session(d, incoming.agent, incoming.cwd);
		if (!found) {
			continue;
		}
		build_inbox::consume_hibernation(incoming.id);

		found->hibernating = true;
		found->wake_prompt = std::move(incoming.prompt);
		recorded = true;

		const std::string awaited = found->unbuilt.empty() ? "the next build" : unbuilt_label(*found);

		log::println(log::level::info, log::category::task, "agent: chat '{}' is hibernating until {} lands", found->name, awaited);
		append_row(*found, {
			.kind = row_kind::note,
			.text = std::format("hibernating until built: {}", awaited),
		});

		build_inbox::publish({
			.id = incoming.id,
			.outcome = build_inbox::status::ok,
			.lines = { std::format("hibernating until {} covers your edits - end your turn now; the editor will prompt you when it lands", awaited) },
		});
	}

	return recorded;
}

auto gse::ide::agent::hibernating_count(const data& d) -> std::size_t {
	return static_cast<std::size_t>(std::ranges::count_if(d.sessions, [](const session& s) {
		return s.hibernating;
	}));
}

auto gse::ide::agent::nudge_unclosed_phases(data& d) -> void {
	constexpr std::uint32_t nudge_limit = 1;

	for (session& s : d.sessions) {
		if (!s.turn_ended) {
			continue;
		}
		s.turn_ended = false;

		const task_phase phase = current_phase(s);
		if (!s.editor_turn || s.nudges >= nudge_limit || phase == task_phase::settled) {
			continue;
		}
		if (s.hibernating || s.gate || s.pending_transition || s.question || !s.running) {
			continue;
		}

		++s.nudges;
		log::println(log::level::info, log::category::task, "agent: chat '{}' ended a turn in {} without reporting the phase - nudging it once", s.name, enum_to_string(phase));
		send_to_session(s, std::format(
			"Your turn ended in the {} phase without calling gse_phase_done, so the task is sitting still - the editor only moves this chat on when you report. If the work this phase owns is finished, call gse_phase_done now with your summary. If it is not finished, carry on with it and do not call it yet. This is the only time you are asked.",
			std::string_view(policy_of(phase).label)
		), {}, true);
	}
}

auto gse::ide::agent::wake_session(session& s, const std::string_view lead) -> void {
	s.hibernating = false;
	const std::string prompt = std::move(s.wake_prompt);
	s.wake_prompt.clear();

	std::string message(lead);
	if (!prompt.empty()) {
		message += "\n\n" + prompt;
	}
	resume_with(s, std::move(message));
	s.woke_by = "the build it slept on finished";
}

auto gse::ide::agent::resume_with(session& s, std::string message, std::optional<transcript_row> shown) -> void {
	if (!s.running && !launch_session(s)) {
		append_row(s, {
			.kind = row_kind::failure,
			.text = "'claude' could not be relaunched to continue this chat",
		});
		return;
	}

	if (const task_phase phase = current_phase(s); phase != task_phase::settled) {
		message += std::format(
			"\n\nThis chat is in its {} phase, and that phase ends with gse_phase_done. If the work it owns is finished when you pick this back up, call it with your summary rather than only reporting back.",
			std::string_view(policy_of(phase).label)
		);
	}

	send_to_session(s, message, {}, true, std::move(shown));
}

auto gse::ide::agent::wake_observers(data& d, const build_runner::build_finished& finished) -> void {
	const auto broke_this_build = [&finished](const session& s) {
		return s.blame_build == finished.key && !s.blame.empty();
	};

	std::string red_owner;
	bool orphaned = false;
	if (!finished.succeeded) {
		const auto broke_it = std::ranges::find_if(d.sessions, broke_this_build);
		orphaned = broke_it == d.sessions.end() && d.inbox_queue.empty();
		red_owner = broke_it != d.sessions.end()
			? std::format("errors owned by chat '{}'", broke_it->name)
			: "errors nobody here wrote";
	}

	std::string orphan_errors;
	for (const build_runner::build_error& e : finished.errors | std::views::take(10)) {
		orphan_errors += std::format("\n  {}:{}: {}", e.file.generic_string(), e.line, e.message);
	}
	if (orphan_errors.empty()) {
		orphan_errors = "\n  (no compiler diagnostic was parsed - the tail of the build output follows)";
		for (const std::string& line : finished.output) {
			orphan_errors += "\n  " + line;
		}
	}

	for (session& s : d.sessions) {
		if (!s.hibernating || !(s.unbuilt.empty() || s.unbuilt.contains(finished.key))) {
			continue;
		}

		if (orphaned && !broke_this_build(s)) {
			s.asleep_blocker.clear();
			log::println(log::level::warning, log::category::task, "agent: waking chat '{}' - the build failed on errors no chat owns and nothing else is queued", s.name);
			append_row(s, {
				.kind = row_kind::failure,
				.text = "the build failed on errors no chat owns - nothing else is queued to clear it",
				.detail = orphan_errors.substr(1),
			});
			wake_session(s, "The build you were waiting on failed, and none of its errors are on lines any chat wrote. Every waiting chat was asleep on it, so nothing would ever have rebuilt - you are being woken instead. These are not yours: do not fix them. Report the state of the tree to the owner and stop." + orphan_errors);
			continue;
		}

		if (!finished.succeeded && !broke_this_build(s)) {
			const std::string reason = "blocked on a red tree - " + red_owner;
			if (s.asleep_blocker != reason) {
				s.asleep_blocker = reason;
				log::println(log::level::info, log::category::task, "agent: chat '{}' stays asleep - the failed build had no errors of its own ({})", s.name, red_owner);
				append_row(s, {
					.kind = row_kind::note,
					.text = std::format("staying asleep - the build failed on {}", red_owner),
					.detail = std::format("still hibernating until {} lands", s.unbuilt.empty() ? "the next build" : unbuilt_label(s)),
				});
			}
			continue;
		}

		if (finished.succeeded && !edits_built(d, s)) {
			const std::string pending = unbuilt_label(s);
			const std::string reason = "that build started before these edits";
			if (s.asleep_blocker != reason) {
				s.asleep_blocker = reason;
				log::println(log::level::info, log::category::task, "agent: chat '{}' stays asleep - that build started before its latest edits to {}", s.name, pending);
				append_row(s, {
					.kind = row_kind::note,
					.text = "staying asleep - that build started before your latest edits",
					.detail = std::format("still hibernating until {} lands", pending),
				});
			}
			continue;
		}
		s.asleep_blocker.clear();

		std::string lead = finished.succeeded
			? "The build you were waiting on succeeded and your edits are in the current binaries."
			: "The build you were waiting on failed. These errors land on lines you wrote:\n" + blame_prompt({}, s.blame);
		if (!finished.succeeded) {
			s.blame.clear();
			s.blame_build = {};
		}

		log::println(log::level::info, log::category::task, "agent: waking chat '{}' - its edits are in the build", s.name);
		wake_session(s, lead);
	}
}

auto gse::ide::agent::wake_covered(data& d) -> void {
	for (session& s : d.sessions) {
		if (!s.hibernating || s.unbuilt.empty() || !edits_built(d, s)) {
			continue;
		}

		log::println(log::level::info, log::category::task, "agent: waking chat '{}' - a build outside this editor covered its edits", s.name);
		wake_session(s, std::format(
			"A build covering {} succeeded, so your edits are in the current binaries. It landed outside this editor's own build - another editor instance, or the editor rebuild that restarted this one - so there is no error list attached to it.",
			unbuilt_label(s)
		));
	}
}

auto gse::ide::agent::resume_interrupted(data& d) -> void {
	constexpr std::uint32_t resume_limit = 3;

	const std::vector<build_inbox::request> carried = build_inbox::peek_requests();

	for (session& s : d.sessions) {
		if (s.hibernating && !s.running && !s.info.agent_id.empty() && std::ranges::none_of(carried, [&s](const build_inbox::request& r) {
			return r.agent == s.info.agent_id;
		})) {
			if (s.resume_attempts >= resume_limit) {
				log::println(log::level::warning, log::category::task, "agent: chat '{}' has been woken after a restart {} times in a row - leaving it asleep", s.name, s.resume_attempts);
				continue;
			}
			++s.resume_attempts;
			log::println(log::level::info, log::category::task, "agent: waking chat '{}' - the build it was hibernating on did not survive the editor restart", s.name);
			wake_session(
				s,
				"The editor closed or crashed while you were hibernating, and the build you were waiting on was dropped with it - it never ran. "
				"Call gse_build again for the same request. You are not blocked: if the tree is busy the build defers and you can hibernate again."
			);
			continue;
		}

		if (!s.turn_open || s.running || s.hibernating || s.pending_transition || s.gate || s.question || s.info.agent_id.empty()) {
			continue;
		}

		if (s.resume_attempts >= resume_limit) {
			log::println(log::level::warning, log::category::task, "agent: chat '{}' has been cut off mid-turn {} times in a row - leaving it stopped", s.name, s.resume_attempts);
			s.turn_open = false;
			append_row(s, {
				.kind = row_kind::failure,
				.text = std::format("cut off mid-turn {} times in a row - not continuing it again on its own", s.resume_attempts),
				.detail = "send a message to pick the turn back up",
			});
			continue;
		}

		++s.resume_attempts;
		log::println(log::level::info, log::category::task, "agent: chat '{}' was cut off mid-turn - continuing it (attempt {})", s.name, s.resume_attempts);
		wake_session(
			s,
			"The editor died while you were mid-turn, so 'claude' was killed with it and this chat has been relaunched against the same conversation. "
			"Whatever you were partway through - an edit, a build, a run - may have only half landed, so check the state of it before you carry on. Then continue the turn."
		);
	}
}

auto gse::ide::agent::request_of(const queued_build& queued) -> build_inbox::request {
	return {
		.id = queued.id,
		.agent = queued.agent,
		.target = queued.target == build_runner::build_target::editor ? "editor" : "game",
		.tree = queued.tree ? queued.tree->name : std::string{},
		.profile = queued.profile,
		.config = queued.config,
		.cwd = queued.cwd,
		.project = queued.project,
		.settings = queued.settings,
		.scenario = queued.scenario,
		.exit_after = queued.exit_after,
		.run = queued.run,
		.run_only = queued.run_only,
	};
}

auto gse::ide::agent::cancel_queued(data& d, const std::string_view id) -> void {
	const auto found = std::ranges::find(d.inbox_queue, id, &queued_build::id);
	if (found == d.inbox_queue.end()) {
		return;
	}

	log::println(log::level::info, log::category::task, "build inbox: cancelled queued build {}", found->id);
	build_inbox::publish({
		.id = found->id,
		.outcome = build_inbox::status::aborted,
		.lines = { "cancelled from the agents panel" },
	});
	d.inbox_queue.erase(found);
}

auto gse::ide::agent::force_queued(data& d, const std::string_view id) -> void {
	const auto found = std::ranges::find(d.inbox_queue, id, &queued_build::id);
	if (found == d.inbox_queue.end()) {
		return;
	}

	log::println(log::level::info, log::category::task, "build inbox: {} was released to build now", found->id);
	found->forced = true;
}

auto gse::ide::agent::hand_off_builds(data& d, const bool relaunching) -> void {
	for (const queued_build& waiter : d.inbox_active) {
		if (relaunching && waiter.target != build_runner::build_target::editor) {
			build_inbox::restore(request_of(waiter));
			continue;
		}
		build_inbox::publish({
			.id = waiter.id,
			.outcome = relaunching ? build_inbox::status::ok : build_inbox::status::aborted,
			.lines = { relaunching
				? "the editor is relaunching into the new image"
				: "the editor shut down before the build finished" },
		});
	}

	for (const queued_build& waiter : d.inbox_queue) {
		if (relaunching) {
			build_inbox::restore(request_of(waiter));
			continue;
		}
		build_inbox::publish({
			.id = waiter.id,
			.outcome = build_inbox::status::aborted,
			.lines = { "the editor shut down before this queued build started" },
		});
	}

	if (relaunching) {
		log::println(log::level::info, log::category::task, "build inbox: handing {} queued request(s) to the relaunched editor", d.inbox_queue.size());
	}

	d.inbox_active.clear();
	d.inbox_queue.clear();
}

auto gse::ide::agent::poll_build_inbox(data& d, const channel_write<build_runner::build_request> builds, const bool building) -> bool {
	const time now = system_clock::now<time>();

	if (!d.inbox_active.empty()) {
		d.inbox_started = d.inbox_started || building;
		if (!d.inbox_started && !building && now >= d.inbox_dispatch_deadline) {
			log::println(log::level::info, log::category::task, "build inbox: the editor took a different build, so {} request(s) go back in the queue", d.inbox_active.size());
			d.inbox_queue.insert(d.inbox_queue.begin(), std::make_move_iterator(d.inbox_active.begin()), std::make_move_iterator(d.inbox_active.end()));
			d.inbox_active.clear();
		}
	}

	if (now < d.next_inbox_poll) {
		return false;
	}
	d.next_inbox_poll = now + seconds(0.25f);

	const bool hibernated = accept_hibernations(d);
	const bool phases_recorded = accept_phase_reports(d);
	const bool awaits_recorded = accept_awaits(d);
	const bool woken = service_awaits(d);
	const bool gaps_recorded = accept_gap_reports(d);
	const bool asked = accept_questions(d);
	const bool answered = service_questions(d);
	const bool drew = accept_draws(d);
	const bool stepped = accept_step_reports(d);
	nudge_unclosed_phases(d);
	const bool recorded = hibernated || phases_recorded || awaits_recorded || woken || gaps_recorded || asked || answered || drew || stepped;
	d.presence = build_inbox::take_presence();

	accept_requests(d);
	if (d.inbox_queue.empty()) {
		return recorded;
	}

	for (queued_build& queued : d.inbox_queue) {
		const build_hold_state hold = hold_for(d, queued, building);
		report_hold(queued, hold, blocker_name(d, hold.blocker));
	}

	if (!d.inbox_active.empty()) {
		return recorded;
	}

	const auto ready = [](const queued_build& queued) {
		return queued.reported == build_hold::none;
	};
	if (std::ranges::none_of(d.inbox_queue, ready)) {
		return recorded;
	}

	const auto is_editor = [](const queued_build& queued) {
		return queued.target == build_runner::build_target::editor;
	};
	const auto starving_editor = std::ranges::find_if(d.inbox_queue, [&is_editor, &ready, now](const queued_build& queued) {
		return is_editor(queued) && ready(queued) && (queued.forced || now >= queued.requested + build_patience());
	});
	const auto project_first = std::ranges::find_if(d.inbox_queue, [&is_editor, &ready](const queued_build& queued) {
		return !is_editor(queued) && ready(queued);
	});

	const queued_build& head = starving_editor != d.inbox_queue.end() ? *starving_editor
		: project_first != d.inbox_queue.end() ? *project_first
		: *std::ranges::find_if(d.inbox_queue, ready);

	const build_runner::build_target target = head.target;
	const bool run = head.run;
	const bool run_only = head.run_only;
	const config::worktree* tree = head.tree;
	const std::string profile = head.profile;
	const std::string config = head.config;
	const std::vector<std::string> settings = head.settings;
	const std::string scenario = head.scenario;
	const time exit_after = head.exit_after;

	std::vector<queued_build> group;
	std::vector<queued_build> deferred;
	for (queued_build& queued : d.inbox_queue) {
		if (queued.target == target && queued.run == run && queued.run_only == run_only && queued.tree == tree && queued.profile == profile && queued.config == config && queued.settings == settings && queued.scenario == scenario && queued.exit_after == exit_after) {
			group.push_back(std::move(queued));
		}
		else {
			deferred.push_back(std::move(queued));
		}
	}

	log::println(log::level::info, log::category::task, "build inbox: one {} build is answering {} queued request(s), {} deferred", target == build_runner::build_target::editor ? "editor" : "project", group.size(), deferred.size());

	builds.push<build_runner::build_request>({
		.target = target,
		.run_after = run,
		.skip_build = run_only,
		.settings = settings,
		.bounds = { .scenario = scenario, .exit_after = exit_after },
		.config = config,
		.profile = profile,
		.tree = tree != nullptr ? tree->name : std::string{},
		.inbox_id = group.front().id,
	});

	d.inbox_active = std::move(group);
	d.inbox_dispatch_deadline = now + seconds(5.f);
	d.inbox_started = false;
	d.inbox_queue = std::move(deferred);
	return recorded;
}

auto gse::ide::agent::publish_inbox_result(data& d, const build_runner::build_finished& finished) -> void {
	if (d.inbox_active.empty()) {
		return;
	}

	std::vector<blame_match> owners;
	owners.reserve(finished.errors.size());
	for (const build_runner::build_error& error : finished.errors) {
		owners.push_back(blame_owner(d, error));
	}

	for (const queued_build& waiter : d.inbox_active) {
		build_inbox::result outcome{
			.id = waiter.id,
			.outcome = finished.succeeded ? build_inbox::status::ok : build_inbox::status::failed,
		};
		if (finished.succeeded) {
			build_inbox::publish(outcome);
			continue;
		}

		std::vector<std::string> mine;
		std::vector<std::string> theirs;
		std::vector<std::string> unattributed;
		std::uint32_t owned = 0;
		for (std::size_t index = 0; index < finished.errors.size(); ++index) {
			const build_runner::build_error& error = finished.errors[index];
			const blame_match& match = owners[index];
			const session* owner = match.owner;
			const bool is_mine = owner && !waiter.agent.empty() && owner->info.agent_id == waiter.agent;
			std::vector<std::string>& into = is_mine ? mine : owner ? theirs : unattributed;
			owned += is_mine ? 1 : 0;

			into.push_back(std::format("{}:{}: {}", error.file.generic_display_string(), error.line, error.message));
			for (const std::string& note : error.notes) {
				into.push_back("  " + note);
			}
			for (const std::filesystem::path& related : error.related) {
				const session* touching = touch_owner(d, std::span(&related, 1));
				into.push_back(touching && touching->info.agent_id != waiter.agent
					? std::format("  related {} - chat '{}' is editing it", related.generic_display_string(), touching->name)
					: "  related " + related.generic_display_string());
			}
			if (is_mine && !match.via.empty()) {
				mine.push_back(std::format("  ^ nobody edited this file - it depends on {}, which you changed", match.via.generic_display_string()));
			}
			if (owner && !is_mine) {
				const std::string_view owner_name = owner->name.empty() ? "unnamed" : owner->name;
				theirs.push_back(!match.via.empty()
					? std::format("  ^ chat '{}' edited {}, which this file depends on", owner_name, match.via.generic_display_string())
					: match.at_line
					? std::format("  ^ chat '{}' wrote this line", owner_name)
					: std::format("  ^ chat '{}' edited this file, but not this line", owner_name));
			}
		}

		outcome.owned = owned;
		if (mine.empty() && theirs.empty() && unattributed.empty()) {
			outcome.lines.emplace_back("the build failed without a parseable diagnostic; open the build stream in the editor terminal");
		}
		if (!mine.empty()) {
			outcome.lines.emplace_back("your errors:");
			outcome.lines.insert(outcome.lines.end(), mine.begin(), mine.end());
		}
		if (!theirs.empty()) {
			outcome.lines.emplace_back(mine.empty()
				? "none of these errors land on lines you wrote - your own work is not implicated, and fixing them is not your job:"
				: "errors owned by other chats - do not act on these:");
			outcome.lines.insert(outcome.lines.end(), theirs.begin(), theirs.end());
		}
		if (!unattributed.empty()) {
			outcome.lines.emplace_back(mine.empty()
				? "errors nobody in this editor edited - another editor instance or a hand edit owns them. Check whether they relate to your change before touching anything:"
				: "errors nobody in this editor edited - likely another editor instance; do not assume they are yours:");
			outcome.lines.insert(outcome.lines.end(), unattributed.begin(), unattributed.end());

			const std::string elsewhere = active_elsewhere(d, waiter);
			if (!elsewhere.empty()) {
				outcome.lines.push_back("  " + elsewhere);
			}
		}
		build_inbox::publish(outcome);
	}
	d.inbox_active.clear();
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
	d.overview_active = false;
	send_to_session(*found, blame_prompt("The build failed on files this session edited:", found->blame), {}, true);
	found->blame.clear();
	found->blame_build = {};
}

auto gse::ide::agent::fix_unclaimed(data& d, const std::span<const std::string> denied) -> void {
	if (d.unclaimed.empty()) {
		return;
	}

	const blamed_error& first = d.unclaimed.front();
	const std::span<const config::worktree> trees = config::worktrees();
	const auto tree = std::ranges::find(trees, d.unclaimed_tree, &config::worktree::name);
	session& started = create_session(d, (tree != trees.end() ? *tree : config::primary()).project_root, task_phase::apply);
	started.kind = chat_kind::build_error;
	started.name = first.file.empty() ? std::string("fix build") : std::format("fix {}", first.file.filename().generic_display_string());
	started.fix = build_fix{
		.errors = d.unclaimed,
		.denied = { denied.begin(), denied.end() },
	};

	if (!launch_session(started)) {
		append_row(started, {
			.kind = row_kind::failure,
			.text = "failed to launch 'claude' - is it on PATH?",
		});
		return;
	}

	std::string prompt = blame_prompt("The build failed and no chat owns these edits:", d.unclaimed);
	if (!denied.empty()) {
		prompt += std::format("\nThese chats were asked and said their edits did not cause it: {}.", denied | std::views::join_with(std::string_view(", ")) | std::ranges::to<std::string>());
	}
	send_to_session(started, prompt, {}, true);
	d.unclaimed.clear();
	d.unclaimed_build = {};
}

auto gse::ide::agent::record_field(const std::string_view record, const std::string_view key) -> std::string_view {
	for (const auto line : record | std::views::split('\n')) {
		const std::string_view text(line.begin(), line.end());
		if (text.size() > key.size() + 1 && text.starts_with(key) && text.substr(key.size(), 2) == ": ") {
			return text.substr(key.size() + 2);
		}
	}
	return {};
}

auto gse::ide::agent::crash_prompt(const std::span<const pending_crash> reports) -> std::string {
	const pending_crash& newest = reports.front();
	std::string prompt = std::format(
		"The editor died and restarted itself. This is its crash record, written by the crash handler before the process went down:\n\n"
		"{}\n"
		"The record is at {}, and the runtime log of the run that crashed is named in it - read that log with gse_log_query using its run stamp.\n\n",
		newest.record,
		newest.path.generic_display_string()
	);

	if (reports.size() > 1) {
		prompt += std::format("This is the newest of {} crash reports handed to you together, so it has happened more than once. The others are:\n", reports.size());
		for (const pending_crash& other : reports | std::views::drop(1)) {
			prompt += std::format("  {} - {}\n", other.path.generic_display_string(), record_field(other.record, "reason"));
		}
		prompt += "They may be the same crash or different ones; read them before assuming either.\n\n";
	}

	prompt +=
		"Find the cause and fix it. Every report here was recorded against the binary that is on disk right now, so the stack frames can be symbolized - "
		"but do it before you build anything, because a build replaces the binary and the offsets stop meaning anything:\n"
		"  addr2line -e <exe from the record> -f -C -s -p <0x140000000 + RVA>\n"
		"using the addr2line from the toolchain the binary was built with.\n\n"
		"Nobody is watching this chat, so your scope is approved automatically and you carry the task through to a fix yourself.";
	return prompt;
}

auto gse::ide::agent::set_aside(const std::filesystem::path& report, const std::string_view suffix) -> std::filesystem::path {
	std::filesystem::path moved = report;
	moved += suffix;
	std::error_code ec;
	std::filesystem::rename(report, moved, ec);
	if (ec) {
		log::println(log::level::warning, log::category::task, "agent: crash report {} could not be renamed to {} ({})", report, moved, ec.message());
		return {};
	}
	return moved;
}

auto gse::ide::agent::triage_crashes(data& d) -> void {
	const auto in_flight = std::ranges::find_if(d.sessions, [](const session& s) {
		return s.kind == chat_kind::crash && current_phase(s) != task_phase::settled;
	});
	if (in_flight != d.sessions.end()) {
		log::println(log::level::info, log::category::task, "agent: chat '{}' is still working on a crash, so new crash reports wait until it settles", in_flight->name);
		return;
	}

	std::error_code ec;
	const std::string prefix = std::format("{}.", gse::config::executable_stem());
	std::vector<std::pair<std::filesystem::file_time_type, std::filesystem::path>> found;
	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(gse::config::crash_dir(), ec)) {
		const std::string name = entry.path().filename().generic_string();
		if (!name.starts_with(prefix) || !name.ends_with(crash_report_extension)) {
			continue;
		}
		const std::filesystem::file_time_type written = entry.last_write_time(ec);
		if (!ec) {
			found.emplace_back(written, entry.path());
		}
	}
	std::ranges::sort(found, std::ranges::greater{}, &decltype(found)::value_type::first);

	const std::string current = gse::binary_identity();
	std::vector<pending_crash> fresh;
	std::size_t stale = 0;
	for (const std::filesystem::path& report : found | std::views::values) {
		const std::vector<std::byte> bytes = read_file_bytes(report);
		std::string record(reinterpret_cast<const char*>(bytes.data()), bytes.size());

		if (current.empty() || record_field(record, binary_field) != current) {
			stale += set_aside(report, stale_report_suffix).empty() ? 0 : 1;
			continue;
		}
		if (std::filesystem::path triaged = set_aside(report, triaged_report_suffix); !triaged.empty()) {
			fresh.push_back({ .path = std::move(triaged), .record = std::move(record) });
		}
	}

	if (stale > 0) {
		log::println(log::level::info, log::category::task, "agent: set aside {} crash report(s) recorded against a binary that has since been rebuilt", stale);
	}
	if (fresh.empty()) {
		return;
	}

	session& started = create_session(d, config::primary().project_root, task_phase::scope);
	started.kind = chat_kind::crash;
	started.name = std::format("crash {}", std::filesystem::path(fresh.front().path).replace_extension().stem().generic_display_string());

	if (!launch_session(started)) {
		append_row(started, {
			.kind = row_kind::failure,
			.text = "failed to launch 'claude' - is it on PATH?",
		});
		return;
	}

	log::println(log::level::info, log::category::task, "agent: chat '{}' is triaging {} crash report(s)", started.name, fresh.size());
	send_to_session(started, crash_prompt(fresh), {}, true);
}

auto gse::ide::agent::blame_label(const session& s) -> std::string {
	if (s.blame.empty()) {
		return "-";
	}

	const blamed_error& first = s.blame.front();
	const std::string head = first.file.empty()
		? std::string("build failure")
		: std::format("{}:{}", first.file.filename().generic_display_string(), first.line);
	return s.blame.size() > 1
		? std::format("{} +{}", head, s.blame.size() - 1)
		: head;
}

auto gse::ide::agent::blame_prompt(const std::string_view lead, const std::span<const blamed_error> errors) -> std::string {
	std::string prompt(lead);
	prompt += '\n';
	prompt += describe_errors(errors);
	prompt += "Fix them.";
	return prompt;
}

auto gse::ide::agent::describe_errors(const std::span<const blamed_error> errors) -> std::string {
	std::string out;
	for (const blamed_error& error : errors) {
		if (error.file.empty()) {
			out += error.message + ". The tail of the build output:\n";
			for (const std::string& line : error.output) {
				out += "  " + line + "\n";
			}
			continue;
		}
		out += std::format("{}:{}: error: {}\n", error.file.generic_display_string(), error.line, error.message);
		for (const std::string& note : error.notes) {
			out += "  " + note + "\n";
		}
		if (!error.via.empty()) {
			out += std::format("  nobody edited this file - it depends on {}, which you changed\n", error.via.generic_display_string());
		}
	}
	return out;
}

auto gse::ide::agent::arm_retry(session& s) -> void {
	if (!s.retry.held) {
		return;
	}

	s.retry.waiting = true;
	++s.retry.attempts;

	const std::int64_t remaining = s.limited_until - unix_now();
	append_row(s, {
		.kind = row_kind::note,
		.text = remaining > 0
			? std::format("usage limit reached - holding this turn, resending in {}m", std::max<std::int64_t>(remaining / 60, 1))
			: "holding this turn - it will be resent once the connection is back",
	});
}

auto gse::ide::agent::link_delay(const data& d) -> time {
	const time first = seconds(5.f);
	const time longest = seconds(20.f);
	return std::min<time>(first * static_cast<float>(1u << std::min(d.link_misses, 5u)), longest);
}

auto gse::ide::agent::resume_waiting(data& d) -> void {
	for (session& s : d.sessions) {
		if (!s.retry.waiting || s.limited_until > unix_now()) {
			continue;
		}
		if (!s.running && !launch_session(s)) {
			continue;
		}

		append_row(s, {
			.kind = row_kind::note,
			.text = std::format("continuing the interrupted turn (attempt {})", s.retry.attempts + 1),
		});
		s.retry.waiting = false;
		send_to_session(s, "continue", {}, s.editor_turn);
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

	if (const std::int64_t remaining = s.limited_until - unix_now(); remaining > 0) {
		return remaining >= 120
			? std::format("usage limit reached - the held turn resumes in {}m", remaining / 60)
			: std::format("usage limit reached - the held turn resumes in {}s", remaining);
	}

	const std::string_view lead = d.link_misses > 0 ? "offline - " : "";
	if (net::active(d.link)) {
		return std::format("{}checking the connection (attempt {})", lead, s.retry.attempts);
	}

	const time waited = d.link_clock ? d.link_clock->elapsed() : time{};
	const time remaining = std::max<time>(link_delay(d) - waited, time{});
	return std::format("{}resending the held turn in {:.0f:s} (attempt {})", lead, remaining,s.retry.attempts);
}