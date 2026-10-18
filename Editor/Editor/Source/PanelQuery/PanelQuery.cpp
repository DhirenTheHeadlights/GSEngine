module gse.ide.panel_query;

import gse;
import gse.ide.agent;
import gse.ide.alloc;
import gse.ide.analysis;
import gse.ide.app;
import gse.ide.build;
import gse.ide.config;
import gse.ide.diagnostic;
import gse.ide.git;
import gse.ide.graph;
import gse.ide.highlight;
import gse.ide.lint_panel;
import gse.ide.problems;
import gse.ide.profile;
import gse.ide.search;
import gse.ide.search_panel;
import gse.ide.source_control;
import gse.ide.terminal;
import gse.ide.workspace;
import std;

auto gse::ide::field(std::vector<std::string>& out, const std::string_view key, const std::string_view value) -> void {
	std::string text(value);
	for (char& c : text) {
		if (c == '\n' || c == '\r' || c == '\t') {
			c = ' ';
		}
	}
	out.push_back(std::format("field {} {}", key, text));
}

auto gse::ide::count(std::vector<std::string>& out, const std::string_view key, const std::size_t value) -> void {
	const std::string opened = std::format("section {}", key);
	assert(std::ranges::find(out, opened) == out.end(), "panel report emits '{}' as both a report-level count and a section; the flat grammar resolves that by overwrite, so one of them would silently never reach the agent", key);

	out.push_back(std::format("count {} {}", key, value));
}

auto gse::ide::section(std::vector<std::string>& out, const std::string_view name) -> void {
	const std::string opened = std::format("section {}", name);
	const std::string scalar_count = std::format("count {} ", name);
	const std::string scalar_field = std::format("field {} ", name);

	bool report_level = true;
	for (const std::string& line : out) {
		const bool collides = line == opened
			|| line.starts_with(scalar_count)
			|| (report_level && line.starts_with(scalar_field));
		assert(!collides, "panel report emits '{}' as both a section and a report-level key; the flat grammar resolves that by overwrite, so one of them would silently never reach the agent", name);
		report_level = report_level && !line.starts_with("section ");
	}

	out.push_back(opened);
}

auto gse::ide::item(std::vector<std::string>& out) -> void {
	out.emplace_back("item");
}

auto gse::ide::panel_reports() -> std::span<const panel_report> {
	static const std::array table = {
		panel_report{
			.name = "Explorer",
			.summary = "project file tree, selection and expansion",
			.writer = { .write = write_explorer },
		},
		panel_report{
			.name = "Code",
			.summary = "open documents, caret, dirty state and per-document analysis",
			.writer = { .write = write_code },
		},
		panel_report{
			.name = "Problems",
			.summary = "diagnostics and lints across open documents, filtered as the panel filters them",
			.writer = { .write = write_problems },
		},
		panel_report{
			.name = "Search",
			.summary = "search panel query, scopes and results",
			.writer = { .write = write_search },
		},
		panel_report{
			.name = "Lints",
			.summary = "lint findings grouped by rule",
			.writer = { .write = write_lints },
		},
		panel_report{
			.name = "Source Control",
			.summary = "repositories, branches, ahead/behind and per-file change state",
			.writer = { .write = write_source_control },
		},
		panel_report{
			.name = "Terminal",
			.summary = "terminal instances and their tail output",
			.writer = { .write = write_terminal },
		},
		panel_report{
			.name = "Agent",
			.summary = "chats, their phase, model and unbuilt or blamed files",
			.writer = { .write = write_agent },
		},
		panel_report{
			.name = "Game",
			.summary = "play session shape, attached instances and server status",
			.writer = { .write = write_game },
		},
		panel_report{
			.name = "Profile",
			.summary = "profiler mode and the loaded report's hottest records",
			.writer = { .write = write_profile },
		},
		panel_report{
			.name = "Alloc",
			.summary = "address space usage and the largest live allocation sites",
			.writer = { .write = write_alloc },
		},
		panel_report{
			.name = "Graph",
			.summary = "system graph nodes and edges",
			.writer = { .write = write_graph },
		},
		panel_report{
			.name = "Quick Search",
			.summary = "the quick search overlay's query and results",
			.writer = { .write = write_quick_search },
			.docked = false,
		},
	};
	return table;
}

auto gse::ide::report_for(const std::string_view name) -> const panel_report* {
	const std::span<const panel_report> table = panel_reports();
	const auto exact = std::ranges::find(table, name, &panel_report::name);
	if (exact != table.end()) {
		return &*exact;
	}

	const auto equal_fold = [](const std::string_view lhs, const std::string_view rhs) {
		return std::ranges::equal(lhs, rhs, [](const char a, const char b) {
			return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
		});
	};
	const auto folded = std::ranges::find_if(table, [name, &equal_fold](const panel_report& entry) {
		return equal_fold(entry.name, name);
	});
	return folded != table.end() ? &*folded : nullptr;
}

auto gse::ide::panel_visibility(const panel_sources& src, const panel_report& entry) -> std::string_view {
	if (!entry.docked) {
		return "not_a_dock_panel";
	}

	const std::span<const panel_desc> panels = editor_panels();
	const auto desc = std::ranges::find(panels, entry.name, &panel_desc::name);
	if (desc == panels.end()) {
		return "missing_from_dock_registry";
	}

	for (const dock_view& view : src.app.views) {
		if (contains_panel(view.tree, desc->id)) {
			const dock_node* leaf = view.tree.nodes.try_get(find_leaf(view.tree, desc->id));
			const bool active = leaf
				&& leaf->active_panel < leaf->panels.size()
				&& leaf->panels[leaf->active_panel] == desc->id;
			return active ? "visible" : "behind_tab";
		}
	}
	if (shelf_edge_of(src.app.shelf.entries, desc->id)) {
		return src.app.shelf.open == desc->id ? "shelf_open" : "shelf_pinned";
	}
	return "closed";
}

auto gse::ide::write_catalogue(const panel_sources& src, std::uint32_t, std::vector<std::string>& out) -> void {
	section(out, "panels");
	for (const panel_report& entry : panel_reports()) {
		item(out);
		field(out, "name", entry.name);
		field(out, "state", panel_visibility(src, entry));
		field(out, "reports", entry.summary);
	}
}

auto gse::ide::write_explorer(const panel_sources& src, const std::uint32_t rows, std::vector<std::string>& out) -> void {
	const workspace::data& ws = src.workspace.ws;

	count(out, "selected_count", ws.explorer_selection.keys.size());
	if (!ws.explorer_error.empty()) {
		field(out, "error", ws.explorer_error);
	}
	if (ws.pending_name) {
		field(out, "renaming", ws.pending_name->path.generic_display_string());
	}

	std::uint32_t emitted = 0;
	std::size_t total = 0;
	section(out, "entries");

	const auto walk = [&](auto&& self, const fs_node& node, const std::uint32_t depth) -> void {
		for (const fs_node& child : node.children) {
			++total;
			if (emitted < rows) {
				item(out);
				field(out, "path", child.path.generic_display_string());
				field(out, "name", child.name);
				field(out, "kind", child.is_dir ? "directory" : "file");
				field(out, "depth", std::format("{}", depth));
				if (child.is_dir) {
					field(out, "expanded", child.loaded ? "1" : "0");
				}
				if (ws.explorer_selection.keys.contains(child.key)) {
					field(out, "selected", "1");
				}
				++emitted;
			}
			self(self, child, depth + 1);
		}
	};
	walk(walk, ws.fs_root, 0);

	count(out, "total", total);
	count(out, "shown", emitted);
}

auto gse::ide::write_code(const panel_sources& src, const std::uint32_t rows, std::vector<std::string>& out) -> void {
	const workspace::data& ws = src.workspace.ws;
	const std::optional<id> active = ws.documents.active_document_id();

	count(out, "open_count", ws.documents.order().size());
	if (active) {
		field(out, "active", ws.documents.at(*active).path.generic_display_string());
	}

	std::uint32_t emitted = 0;
	section(out, "documents");
	for (const id doc_id : ws.documents.order()) {
		if (emitted >= rows) {
			break;
		}
		const document& doc = ws.documents.at(doc_id);
		item(out);
		field(out, "path", doc.path.generic_display_string());
		field(out, "tab_name", doc.tab_name);
		field(out, "active", active && *active == doc_id ? "1" : "0");
		field(out, "language", enum_to_string(doc.language));
		field(out, "view_mode", enum_to_string(doc.mode));
		field(out, "persistence", enum_to_string(doc.persistence));
		if (!doc.persistence_error.empty()) {
			field(out, "persistence_error", doc.persistence_error);
		}
		field(out, "line_count", std::format("{}", doc.buffer.line_count()));
		field(out, "caret_line", std::format("{}", doc.view.caret.line + 1));
		field(out, "caret_column", std::format("{}", doc.view.caret.column + 1));
		field(out, "selecting", doc.view.caret == doc.view.anchor ? "0" : "1");
		field(out, "diagnostics", std::format("{}", doc.diagnostics.size()));
		field(out, "lints", std::format("{}", doc.lint.size()));
		field(out, "analysis_status", enum_to_string(doc.analysis_status));
		if (!doc.analysis_detail.empty()) {
			field(out, "analysis_detail", doc.analysis_detail);
		}
		++emitted;
	}
	count(out, "shown", emitted);
}

auto gse::ide::write_problems(const panel_sources& src, const std::uint32_t rows, std::vector<std::string>& out) -> void {
	const std::vector<problem_row> collected = collect_problems(src.workspace.problems, src.workspace.ws);

	field(out, "show_errors", src.workspace.problems.show_errors ? "1" : "0");
	field(out, "show_warnings", src.workspace.problems.show_warnings ? "1" : "0");
	field(out, "show_hints", src.workspace.problems.show_hints ? "1" : "0");
	count(out, "total", collected.size());

	std::uint32_t emitted = 0;
	section(out, "problems");
	for (const problem_row& row : collected) {
		if (emitted >= rows) {
			break;
		}
		item(out);
		field(out, "severity", enum_to_string(row.entry->level));
		field(out, "file", row.path->generic_display_string());
		field(out, "document", row.document_name);
		field(out, "line", std::format("{}", row.entry->line + 1));
		field(out, "column", std::format("{}", row.entry->start_col + 1));
		field(out, "message", row.entry->message);
		if (row.entry->rule) {
			field(out, "rule", enum_to_string(*row.entry->rule));
		}
		if (row.entry->fix) {
			field(out, "fix", row.entry->fix->title);
		}
		++emitted;
	}
	count(out, "shown", emitted);
}

auto gse::ide::write_search(const panel_sources& src, const std::uint32_t rows, std::vector<std::string>& out) -> void {
	const search_panel_state& panel = src.workspace.search_panel;

	field(out, "query", panel.driver.query);
	field(out, "include_files", panel.include_files ? "1" : "0");
	field(out, "include_symbols", panel.include_symbols ? "1" : "0");
	field(out, "include_content", panel.include_content ? "1" : "0");
	field(out, "selected", std::format("{}", panel.driver.selected));
	count(out, "total", panel.driver.results.size());

	std::uint32_t emitted = 0;
	section(out, "results");
	for (const search::result& hit : panel.driver.results) {
		if (emitted >= rows) {
			break;
		}
		item(out);
		field(out, "domain", enum_to_string(hit.source));
		field(out, "path", hit.path.generic_display_string());
		field(out, "line", std::format("{}", hit.line + 1));
		field(out, "display", hit.display);
		if (!hit.detail.empty()) {
			field(out, "detail", hit.detail);
		}
		++emitted;
	}
	count(out, "shown", emitted);
}

auto gse::ide::write_lints(const panel_sources& src, const std::uint32_t rows, std::vector<std::string>& out) -> void {
	const search::index_state* index = src.search.index;
	if (!index) {
		field(out, "index", "unavailable");
		count(out, "rule_count", 0);
		count(out, "total", 0);
		return;
	}

	const std::shared_ptr<const search::search_snapshot> snapshot = index->query_snapshot();
	const std::vector<lint_rule_group> groups = group_lints(*snapshot->lints);

	std::size_t total = 0;
	for (const lint_rule_group& group : groups) {
		total += group.rows.size();
	}
	count(out, "rule_count", groups.size());
	count(out, "total", total);

	std::uint32_t emitted = 0;
	section(out, "lints");
	for (const lint_rule_group& group : groups) {
		for (const lint_row& row : group.rows) {
			if (emitted >= rows) {
				break;
			}
			item(out);
			field(out, "rule", enum_to_string(group.rule));
			field(out, "title", annotation_from_enum(group.rule, lint_rule_info{}).title);
			field(out, "location", row.location);
			field(out, "message", row.message);
			++emitted;
		}
	}
	count(out, "shown", emitted);
}

auto gse::ide::write_source_control(const panel_sources& src, const std::uint32_t rows, std::vector<std::string>& out) -> void {
	field(out, "busy", src.workspace.git_busy ? "1" : "0");
	if (!src.workspace.git_action_error.empty()) {
		field(out, "error", src.workspace.git_action_error);
	}
	if (!src.workspace.engine_pin.empty()) {
		field(out, "engine_pin", src.workspace.engine_pin);
	}
	if (!src.workspace.source_control.sync_root.empty()) {
		field(out, "syncing", src.workspace.source_control.sync_root.generic_display_string());
	}
	if (!src.workspace.source_control.sync_summary.empty()) {
		field(out, "last_sync", src.workspace.source_control.sync_summary);
	}

	const git::status_snapshot status = src.workspace.git_status;
	if (!status) {
		count(out, "repository_count", 0);
		return;
	}

	std::size_t reported = 0;
	std::size_t total = 0;
	for (const git::repository_snapshot& repo : status->repositories()) {
		if (repo) {
			++reported;
			total += repo->changes.size();
		}
	}
	count(out, "repository_count", reported);
	count(out, "total", total);

	section(out, "repositories");
	for (const git::repository_snapshot& repo : status->repositories()) {
		if (!repo) {
			continue;
		}
		item(out);
		field(out, "root", repo->root.generic_display_string());
		field(out, "branch", repo->branch);
		field(out, "head", repo->head);
		field(out, "upstream", repo->upstream);
		field(out, "ahead", std::format("{}", repo->ahead));
		field(out, "behind", std::format("{}", repo->behind));
		field(out, "changes", std::format("{}", repo->changes.size()));

		const id repo_id = generate_temp_id(repo->root);
		if (const auto edit = src.workspace.source_control.edits.find(repo_id); edit != src.workspace.source_control.edits.end()) {
			if (!edit->second.message.empty()) {
				field(out, "commit_message", edit->second.message);
			}
		}
	}

	std::uint32_t emitted = 0;
	section(out, "changes");
	for (const git::repository_snapshot& repo : status->repositories()) {
		if (!repo) {
			continue;
		}
		for (const git::change& change : repo->changes) {
			if (emitted >= rows) {
				break;
			}
			item(out);
			field(out, "repository", repo->root.generic_display_string());
			field(out, "path", change.relative.generic_display_string());
			field(out, "state", enum_to_string(change.state));
			field(out, "added", std::format("{}", change.added));
			field(out, "deleted", std::format("{}", change.deleted));
			field(out, "staged", change.needs_stage ? "0" : "1");
			if (!change.original.empty()) {
				field(out, "renamed_from", change.original.generic_display_string());
			}
			++emitted;
		}
	}
	count(out, "shown", emitted);
}

auto gse::ide::write_terminal(const panel_sources& src, const std::uint32_t rows, std::vector<std::string>& out) -> void {
	count(out, "instance_count", src.term.instances.size());

	section(out, "instances");
	for (const terminal::instance& inst : src.term.instances) {
		item(out);
		field(out, "name", inst.name);
		field(out, "active", inst.instance_id == src.term.active ? "1" : "0");
		field(out, "interactive", inst.interactive ? "1" : "0");
		field(out, "stream", enum_to_string(inst.kind));
		field(out, "follows_log", inst.follows_log ? "1" : "0");
		field(out, "line_count", std::format("{}", inst.buffer.line_count()));
		if (!inst.input.empty()) {
			field(out, "pending_input", inst.input);
		}
		if (inst.attached_instance) {
			field(out, "attached_instance", std::format("{}", *inst.attached_instance));
		}
	}

	section(out, "tail");
	for (const terminal::instance& inst : src.term.instances) {
		const std::size_t lines = inst.buffer.line_count();
		const std::size_t first = lines > rows ? lines - rows : 0;
		for (std::size_t line = first; line < lines; ++line) {
			item(out);
			field(out, "instance", inst.name);
			field(out, "line", std::format("{}", line + 1));
			field(out, "text", inst.buffer.line(line));
		}
	}
}

auto gse::ide::write_agent(const panel_sources& src, const std::uint32_t rows, std::vector<std::string>& out) -> void {
	count(out, "chat_count", src.chats.sessions.size());
	field(out, "active", std::format("{}", src.chats.active));

	std::uint32_t emitted = 0;
	section(out, "chats");
	for (const agent::session& chat : src.chats.sessions) {
		if (emitted >= rows) {
			break;
		}
		item(out);
		field(out, "id", std::format("{}", chat.id));
		field(out, "name", chat.name);
		field(out, "cwd", chat.cwd.generic_display_string());
		field(out, "running", chat.running ? "1" : "0");
		field(out, "turn_open", chat.turn_open ? "1" : "0");
		field(out, "hibernating", chat.hibernating ? "1" : "0");
		if (chat.hibernating && !chat.asleep_blocker.empty()) {
			field(out, "asleep_reason", chat.asleep_blocker);
		}
		if (chat.sync) {
			field(out, "syncing", chat.sync->root.generic_display_string());
		}
		field(out, "transcript_mode", enum_to_string(chat.mode));
		if (!chat.model_id.empty()) {
			field(out, "model", chat.model_id);
		}
		field(out, "effort", enum_to_string(chat.requested_effort));
		if (!chat.runs.empty()) {
			field(out, "phase", enum_to_string(chat.runs.back().phase));
			field(out, "phase_findings", std::format("{}", chat.runs.back().findings));
		}
		if (chat.gate) {
			field(out, "awaiting_gate", enum_to_string(chat.gate->from));
		}
		if (chat.question && !chat.question->answered) {
			field(out, "awaiting_answer", chat.question->question);
		}
		field(out, "unbuilt_files", std::format("{}", chat.unbuilt.size()));
		field(out, "touched_files", std::format("{}", chat.touched.size()));
		field(out, "blamed_errors", std::format("{}", chat.blame.size()));
		field(out, "turns", std::format("{}", chat.info.turns));
		field(out, "cost", std::format("{:.4f}", chat.info.cost));
		field(out, "context_used", std::format("{}", chat.info.context_used));
		if (!chat.info.failure.empty()) {
			field(out, "failure", chat.info.failure);
		}
		++emitted;
	}
	count(out, "shown", emitted);
}

auto gse::ide::write_game(const panel_sources& src, std::uint32_t, std::vector<std::string>& out) -> void {
	field(out, "building", src.build.building_session ? "1" : "0");
	field(out, "clients", std::format("{}", src.build.session.clients));
	field(out, "dedicated_server", src.build.session.dedicated_server ? "1" : "0");
	field(out, "attached", src.build.session.attached ? "1" : "0");
	field(out, "base_port", std::format("{}", src.build.session.base_port));
	field(out, "generation", std::format("{}", src.build.game_generation));
	if (!src.build.session_error.empty()) {
		field(out, "error", src.build.session_error);
	}
	field(out, "server_running", src.build.server.running ? "1" : "0");
	if (src.build.server.running) {
		field(out, "server_port", std::format("{}", src.build.server.port));
	}

	count(out, "live_instances", static_cast<std::size_t>(std::ranges::count_if(src.build.sessions, [](const build_runner::attached_session& session) {
		return session.generation != 0;
	})));

	section(out, "instances");
	for (const build_runner::attached_session& session : src.build.sessions) {
		if (session.generation == 0) {
			continue;
		}
		item(out);
		field(out, "instance", std::format("{}", session.instance));
		field(out, "generation", std::format("{}", session.generation));
		field(out, "status", enum_to_string(session.status));
	}
}

auto gse::ide::write_profile(const panel_sources& src, const std::uint32_t rows, std::vector<std::string>& out) -> void {
	const profile_view_state& view = src.workspace.ws.profile;

	field(out, "mode", enum_to_string(view.mode));
	field(out, "source", enum_to_string(view.source));
	field(out, "enabled", view.enabled ? "1" : "0");
	count(out, "captured_frames", src.profile.frames.size());
	field(out, "report_loaded", src.profile.report_loaded ? "1" : "0");
	if (!src.profile.report_loaded) {
		return;
	}

	const profile::report_file& report = src.profile.report;
	field(out, "report_frames", std::format("{}", report.frames));
	field(out, "frame_time_ms", std::format("{:.3f}", report.frame_time.as<milliseconds>()));

	const auto emit_records = [&out, rows](const std::string_view name, const std::span<const profile::report_record> records) {
		std::vector<const profile::report_record*> ordered;
		ordered.reserve(records.size());
		for (const profile::report_record& record : records) {
			ordered.push_back(&record);
		}
		std::ranges::sort(ordered, [](const profile::report_record* a, const profile::report_record* b) {
			return a->per_frame > b->per_frame;
		});

		section(out, name);
		std::uint32_t emitted = 0;
		for (const profile::report_record* record : ordered) {
			if (emitted >= rows) {
				break;
			}
			item(out);
			field(out, "tag", record->tag);
			field(out, "per_frame_ms", std::format("{:.4f}", record->per_frame.as<milliseconds>()));
			field(out, "peak_ms", std::format("{:.4f}", record->peak.as<milliseconds>()));
			field(out, "calls_per_frame", std::format("{:.2f}", record->calls_per_frame));
			++emitted;
		}
	};
	emit_records("cpu", report.cpu);
	emit_records("gpu", report.gpu);
}

auto gse::ide::write_alloc(const panel_sources& src, const std::uint32_t rows, std::vector<std::string>& out) -> void {
	const alloc_view_state& view = src.workspace.ws.alloc;
	const alloc::address_space usage = alloc::address_space_usage();

	std::vector<alloc::site> sites;
	alloc::snapshot(sites);
	std::ranges::sort(sites, std::ranges::greater{}, &alloc::site::live);

	field(out, "sampling", alloc::enabled() ? "on" : "paused");
	field(out, "private_committed_bytes", std::format("{:.0f}", usage.private_committed.as<bytes>()));
	field(out, "image_bytes", std::format("{:.0f}", usage.image.as<bytes>()));
	field(out, "mapped_bytes", std::format("{:.0f}", usage.mapped.as<bytes>()));
	field(out, "reserved_bytes", std::format("{:.0f}", usage.reserved.as<bytes>()));
	count(out, "total", sites.size());

	std::uint32_t emitted = 0;
	section(out, "sites");
	for (const alloc::site& site : sites) {
		if (emitted >= rows) {
			break;
		}
		item(out);
		field(out, "pc", std::format("{:#x}", site.pc));
		const auto cached = view.labels.find(site.pc);
		field(out, "label", cached != view.labels.end() ? cached->second : alloc::label_of(site.pc));
		field(out, "live_bytes", std::format("{:.0f}", site.live.as<bytes>()));
		field(out, "since_mark_bytes", std::format("{:.0f}", site.since_mark.as<bytes>()));
		field(out, "samples", std::format("{}", site.live_samples));
		++emitted;
	}
	count(out, "shown", emitted);
}

auto gse::ide::write_graph(const panel_sources& src, const std::uint32_t rows, std::vector<std::string>& out) -> void {
	const introspection::system_graph& graph = src.workspace.graph.snapshot;

	count(out, "node_count", graph.nodes.size());
	count(out, "edge_count", graph.edges.size());

	std::uint32_t emitted = 0;
	section(out, "systems");
	for (const introspection::graph_node& node : graph.nodes) {
		if (emitted >= rows) {
			break;
		}
		item(out);
		field(out, "name", node.display.empty() ? node.name : node.display);
		field(out, "category", node.category);
		field(out, "file", node.file);
		field(out, "line", std::format("{}", node.line));
		field(out, "has_init", node.has_init ? "1" : "0");
		field(out, "has_run", node.has_run ? "1" : "0");
		field(out, "has_frame", node.has_frame ? "1" : "0");
		field(out, "reads", std::format("{}", node.reads.size()));
		field(out, "writes", std::format("{}", node.writes.size()));
		++emitted;
	}
	count(out, "shown", emitted);
}

auto gse::ide::write_quick_search(const panel_sources& src, const std::uint32_t rows, std::vector<std::string>& out) -> void {
	const auto& quick = src.workspace.search;

	field(out, "query", quick.driver.query);
	field(out, "selected", std::format("{}", quick.driver.selected));
	count(out, "total", quick.driver.results.size());

	std::uint32_t emitted = 0;
	section(out, "results");
	for (const search::result& hit : quick.driver.results) {
		if (emitted >= rows) {
			break;
		}
		item(out);
		field(out, "domain", enum_to_string(hit.source));
		field(out, "path", hit.path.generic_display_string());
		field(out, "line", std::format("{}", hit.line + 1));
		field(out, "display", hit.display);
		++emitted;
	}
	count(out, "shown", emitted);
}

auto gse::ide::answer_panel_query(const panel_sources& src, const build_inbox::panel_query& query) -> std::vector<std::string> {
	const std::uint32_t rows = query.rows > 0 ? query.rows : default_report_rows;
	std::vector<std::string> out;

	if (query.panel.empty()) {
		out.emplace_back("panel catalogue");
		write_catalogue(src, rows, out);
		return out;
	}

	const panel_report* entry = report_for(query.panel);
	if (!entry) {
		out.push_back(std::format("error no panel named '{}'", query.panel));
		write_catalogue(src, rows, out);
		return out;
	}

	out.push_back(std::format("panel {}", entry->name));
	field(out, "state", panel_visibility(src, *entry));
	entry->writer.write(src, rows, out);
	return out;
}

auto gse::ide::panel_query_system::run(context&, data& d, const shared_view<editor_app::data> app_d, const shared_view<workspace_system::data> workspace_d, const shared_view<terminal::data> terminal_d, const shared_view<agent::data> agent_d, const shared_view<build_runner::data> build_d, const shared_view<profile_system::data> profile_d, const shared_view<search_system::data> search_d) -> async::task<> {
	if (!d.poll.tick()) {
		co_return;
	}

	const std::vector<build_inbox::panel_query> queries = build_inbox::peek_panel_queries();
	if (queries.empty()) {
		co_return;
	}

	const panel_sources src{
		.app = app_d,
		.workspace = workspace_d,
		.term = terminal_d,
		.chats = agent_d,
		.build = build_d,
		.profile = profile_d,
		.search = search_d,
	};

	for (const build_inbox::panel_query& query : queries) {
		if (!config::owns_request(query.cwd, query.project)) {
			continue;
		}
		build_inbox::consume_panel_query(query.id);
		build_inbox::publish({
			.id = query.id,
			.outcome = build_inbox::status::ok,
			.lines = answer_panel_query(src, query),
		});
	}
	co_return;
}
