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
			.summary = "chats, their phase, model and unbuilt or blamed files, the canvases gse_draw placed in them, apply step progress with the files attributed to each step, answered questions with their picks and hidden question echoes, and live ui motion counts",
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
	count(out, "motion_live", src.chats.motion.live);
	count(out, "motion_moving", src.chats.motion.animating);
	for (const agent::split_kind kind : enum_values<agent::split_kind>()) {
		field(out, std::format("split_{}", annotation_from_enum<agent::split_style>(kind, {}).label), std::format("{:.3f}", agent::split_ratio(src.chats.splits, kind)));
	}

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
		const bool ready = agent::summary_ready(chat);
		field(out, "summary", ready ? "ready" : "none");
		field(out, "view", chat.mode == agent::transcript_mode::summary && ready ? "summary" : chat.mode == agent::transcript_mode::everything ? "everything" : "transcript");
		field(out, "toggle", ready ? "shown" : "hidden");
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

	std::uint32_t drawn = 0;
	section(out, "canvases");
	for (const agent::session& chat : src.chats.sessions) {
		for (const agent::canvas_record& canvas : chat.canvases) {
			if (drawn >= rows) {
				break;
			}
			const auto row = std::ranges::find_if(chat.rows, [&canvas](const agent::transcript_row& r) {
				return r.kind == agent::row_kind::canvas && r.text == canvas.name;
			});
			const agent::transcript_view& view = chat.mode == agent::transcript_mode::everything ? chat.everything : chat.full;
			const auto placed = row == chat.rows.end()
				? view.canvases.end()
				: std::ranges::find(view.canvases, static_cast<std::uint32_t>(std::distance(chat.rows.begin(), row)), &agent::canvas_view::row);

			item(out);
			field(out, "chat", std::format("{}", chat.id));
			field(out, "name", canvas.name);
			field(out, "commands", std::format("{}", canvas.steps.size()));
			field(out, "size", std::format("{}", canvas.size));
			field(out, "row", row == chat.rows.end() ? std::string("unhydrated") : std::format("{}", std::distance(chat.rows.begin(), row)));
			field(out, "line", placed == view.canvases.end() || placed->first_line == agent::unplaced_line ? std::string("unplaced") : std::format("{}", placed->first_line));
			const agent::canvas_tally tally = placed == view.canvases.end() ? agent::canvas_tally{} : placed->tally;
			field(out, "sprites", std::format("{}", tally.sprites));
			field(out, "texts", std::format("{}", tally.texts));
			field(out, "dropped", std::format("{}", tally.dropped));
			field(out, "clipped", std::format("{}", tally.clipped));

			std::vector<std::string_view> sketches;
			for (const agent::canvas_step& step : canvas.steps) {
				if (!step.origin.empty() && std::ranges::find(sketches, step.origin) == sketches.end()) {
					sketches.push_back(step.origin);
				}
			}
			field(out, "sketches", sketches.empty() ? std::string("none") : sketches | std::views::join_with(',') | std::ranges::to<std::string>());
			field(out, "viewing", row != chat.rows.end() && view.viewing_canvas == static_cast<std::uint32_t>(std::distance(chat.rows.begin(), row)) ? "true" : "false");
			++drawn;
		}
	}
	count(out, "canvas_shown", drawn);

	std::uint32_t answered = 0;
	section(out, "answers");
	for (const agent::session& chat : src.chats.sessions) {
		const agent::transcript_view& view = chat.mode == agent::transcript_mode::everything ? chat.everything : chat.full;
		std::uint32_t suppressed = 0;
		for (std::size_t at = 0; at < chat.rows.size(); ++at) {
			const agent::transcript_row& row = chat.rows[at];
			if (row.kind == agent::row_kind::question && view.filter == agent::row_filter::conversation) {
				++suppressed;
			}
			if (row.kind != agent::row_kind::answer || answered >= rows) {
				continue;
			}
			std::vector<std::string> picked;
			for (std::size_t option = 0; option < row.options.size(); ++option) {
				if (row.options[option].chosen) {
					picked.push_back(std::format("{}", option));
				}
			}
			item(out);
			field(out, "chat", std::format("{}", chat.id));
			field(out, "row", std::format("{}", at));
			field(out, "question", row.text);
			field(out, "options", std::format("{}", row.options.size()));
			field(out, "picked", picked.empty() ? std::string("none") : picked | std::views::join_with(',') | std::ranges::to<std::string>());
			field(out, "steering", row.detail.empty() ? "false" : "true");
			++answered;
		}
		item(out);
		field(out, "chat", std::format("{}", chat.id));
		field(out, "suppressed_echoes", std::format("{}", suppressed));
	}
	count(out, "answers_shown", answered);

	std::uint32_t listed = 0;
	section(out, "steps");
	for (const agent::session& chat : src.chats.sessions) {
		if (!chat.gate) {
			continue;
		}
		for (std::size_t at = 0; at < chat.gate->steps.size() && listed < rows; ++at) {
			const build_inbox::plan_step& step = chat.gate->steps[at];
			item(out);
			field(out, "chat", std::format("{}", chat.id));
			field(out, "index", std::format("{}", at));
			field(out, "text", step.text);
			field(out, "files", std::format("{}", step.files.size()));
			field(out, "selected", chat.plan_selected == at ? "true" : "false");
			++listed;
		}
	}
	count(out, "steps_shown", listed);

	section(out, "plans");
	for (const agent::session& chat : src.chats.sessions) {
		const agent::lifecycle life = agent::lifecycle_of(chat);
		const agent::plan_paging paging = agent::paging_of(life.pages, chat.lifecycle_page);
		const auto page_name = [](const agent::summary_page& page) {
			return std::format("{}:{}", enum_to_string(page.phase), enum_to_string(page.kind));
		};
		item(out);
		field(out, "chat", std::format("{}", chat.id));
		field(out, "pages", life.pages | std::views::transform(page_name) | std::views::join_with(',') | std::ranges::to<std::string>());
		field(out, "page", std::format("{}/{} {}", paging.page, paging.pages, page_name(paging.shown)));
		field(out, "scope_steps", std::format("{}", life.steps.size()));
		field(out, "progress", life.progress.reported ? "reported" : "unreported");
		field(out, "run", life.progress.ended ? "ended" : "live");
		field(out, "split", agent::followup_of(chat) ? "on" : "off");
		const agent::summary_anchor opening = agent::opening_page(chat, agent::since_of(chat, chat.arrived, chat.entered));
		const agent::lifecycle opened = agent::lifecycle_at(chat, agent::shown_goal(chat) ? std::optional(opening.position) : std::nullopt);
		const agent::plan_paging landed = agent::paging_of(opened.pages, opening.page);
		field(out, "opening", std::format("cycle {} page {}/{} {}", opening.position, landed.page, landed.pages, page_name(landed.shown)));
		field(out, "waiting", chat.wait_shown.empty() ? std::string("none") : chat.wait_shown);
		if (chat.gate && !agent::gate_summary(chat).empty()) {
			const agent::plan_paging gate = agent::paging_of(agent::scope_pages(agent::gate_summary(chat), chat.gate->steps.size()), chat.plan_page);
			field(out, "gate_page", std::format("{}/{} {}", gate.page, gate.pages, page_name(gate.shown)));
		}
	}

	std::uint32_t tracked = 0;
	section(out, "progress");
	for (const agent::session& chat : src.chats.sessions) {
		const agent::lifecycle life = agent::lifecycle_of(chat);
		if (life.edits.empty() && !life.progress.reported) {
			continue;
		}
		const auto files_of = [&](const std::optional<std::size_t> group) {
			std::vector<std::string> files;
			for (const agent::step_edit* edit : agent::edits_in_group(life.edits, life.steps.size(), group)) {
				std::string name = edit->file.filename().generic_display_string();
				if (!std::ranges::contains(files, name)) {
					files.push_back(std::move(name));
				}
			}
			return files.empty() ? std::string("none") : files | std::views::join_with(',') | std::ranges::to<std::string>();
		};
		for (std::size_t at = 0; at < life.steps.size() && tracked < rows; ++at) {
			item(out);
			field(out, "chat", std::format("{}", chat.id));
			field(out, "index", std::format("{}", at));
			field(out, "state", enum_to_string(life.progress.states[at]));
			field(out, "edits", std::format("{}", life.progress.edits[at]));
			field(out, "files", files_of(at));
			++tracked;
		}
		if (life.progress.unattributed > 0 && tracked < rows) {
			item(out);
			field(out, "chat", std::format("{}", chat.id));
			field(out, "index", "unattributed");
			field(out, "edits", std::format("{}", life.progress.unattributed));
			field(out, "files", files_of(life.steps.size()));
			++tracked;
		}
	}
	count(out, "progress_shown", tracked);

	std::uint32_t cycled = 0;
	section(out, "review_cycles");
	for (const agent::session& chat : src.chats.sessions) {
		const agent::review_loop loop = agent::review_loop_of(chat);
		for (std::size_t at = 0; at < loop.cycles.size() && cycled < rows; ++at) {
			const agent::review_cycle& cycle = loop.cycles[at];
			item(out);
			field(out, "chat", std::format("{}", chat.id));
			field(out, "cycle", std::format("{}", at + 1));
			field(out, "findings", std::format("{}", cycle.findings));
			field(out, "reviewed", cycle.reviewed ? "true" : "false");
			field(out, "revised", cycle.revise_begun ? (cycle.revise_reported ? "reported" : "running") : "no");
			field(out, "edits", std::format("{}", cycle.edits));
			field(out, "open", loop.capped && at + 1 == loop.cycles.size() ? "capped" : "false");
			++cycled;
		}
	}
	count(out, "review_cycles_shown", cycled);

	std::uint32_t reported = 0;
	section(out, "reports");
	for (const agent::session& chat : src.chats.sessions) {
		const agent::lifecycle life = agent::lifecycle_of(chat);
		if (life.report.empty() || reported >= rows) {
			continue;
		}
		item(out);
		field(out, "chat", std::format("{}", chat.id));
		field(out, "head", agent::report_head(life.progress, life.review));
		field(out, "diffs_link", agent::edits_label(agent::task_edits(life)));
		field(out, "chars", std::format("{}", life.report.size()));
		++reported;
	}
	count(out, "reports_shown", reported);

	std::uint32_t fixing = 0;
	section(out, "errors");
	for (const agent::session& chat : src.chats.sessions) {
		if (chat.kind != agent::chat_kind::build_error || fixing >= rows) {
			continue;
		}
		const agent::build_fix* fix = chat.fix ? &*chat.fix : nullptr;
		item(out);
		field(out, "chat", std::format("{}", chat.id));
		field(out, "head", agent::errors_head(fix));
		field(out, "errors", fix ? std::format("{}", fix->errors.size()) : std::string("unrecorded"));
		field(out, "denied", fix ? std::format("{}", fix->denied.size()) : std::string("0"));
		field(out, "strip_lines", std::format("{}", agent::error_strip(fix).size()));
		field(out, "diagnosis", fix && !fix->diagnosis.empty() ? "on" : "off");
		++fixing;
	}
	count(out, "errors_shown", fixing);

	std::uint32_t steered = 0;
	std::uint32_t judged = 0;
	section(out, "goals");
	for (const agent::session& chat : src.chats.sessions) {
		const std::optional<agent::goal_dashboard> goal = agent::goal_dashboard_of(chat);
		if (!goal || steered >= rows) {
			continue;
		}
		item(out);
		field(out, "chat", std::format("{}", chat.id));
		field(out, "head", agent::goal_head(*goal));
		field(out, "standing", enum_to_string(goal->standing));
		field(out, "steers_left", std::format("{}", goal->steers_left));
		field(out, "aim", goal->aim.empty() ? std::string_view("none") : goal->aim);
		field(out, "tally", agent::criteria_tally(goal->criteria));
		field(out, "route", agent::route_line(goal->route));
		field(out, "ledger_shown", std::format("{}", goal->ledger.size()));
		if (!goal->ledger.empty()) {
			field(out, "ledger_latest", agent::ledger_head(goal->ledger.front()));
		}
		++steered;
	}
	count(out, "goals_shown", steered);

	section(out, "criteria");
	for (const agent::session& chat : src.chats.sessions) {
		const agent::goal_state* shown = agent::shown_goal(chat);
		if (!shown) {
			continue;
		}
		for (const agent::criterion_report& criterion : shown->criteria) {
			if (judged >= rows) {
				break;
			}
			item(out);
			field(out, "chat", std::format("{}", chat.id));
			field(out, "id", criterion.id);
			field(out, "state", enum_to_string(criterion.state));
			++judged;
		}
	}
	count(out, "criteria_shown", judged);

	std::uint32_t cycled_goals = 0;
	section(out, "cycles");
	for (const agent::session& chat : src.chats.sessions) {
		if (!agent::shown_goal(chat)) {
			continue;
		}
		const std::vector<agent::goal_cycle> cycles = agent::goal_cycles(chat);
		for (std::size_t at = 0; at < cycles.size() && cycled_goals < rows; ++at) {
			const agent::goal_cycle& cycle = cycles[at];
			const agent::lifecycle life = agent::lifecycle_at(chat, static_cast<std::uint32_t>(at + 1));
			const auto stage_name = [](const agent::summary_page& page) {
				return std::format("{}:{}", enum_to_string(page.phase), enum_to_string(page.kind));
			};
			item(out);
			field(out, "chat", std::format("{}", chat.id));
			field(out, "index", std::format("{}", at + 1));
			field(out, "started", std::format("{}", cycle.started));
			field(out, "runs", std::format("{}", cycle.runs.size()));
			field(out, "verdict", cycle.verdict ? enum_to_string(*cycle.verdict) : std::string_view("none recorded"));
			field(out, "findings", std::format("{}", cycle.findings));
			field(out, "edits", std::format("{}", cycle.edits));
			field(out, "stages", life.pages | std::views::transform(stage_name) | std::views::join_with(',') | std::ranges::to<std::string>());
			++cycled_goals;
		}
	}
	count(out, "cycles_shown", cycled_goals);

	section(out, "seen");
	for (const agent::session& chat : src.chats.sessions) {
		const agent::lifecycle life = agent::lifecycle_of(chat);
		const agent::since_digest since = agent::since_of(chat, chat.arrived, chat.entered);
		item(out);
		field(out, "chat", std::format("{}", chat.id));
		field(out, "seen", std::format("{}", chat.seen));
		field(out, "arrived", std::format("{}", chat.arrived));
		field(out, "entered", std::format("{}", chat.entered));
		field(out, "cursor", life.cycle.shown ? agent::cycle_label(life.cycle) : std::string("task"));
		field(out, "anchor", since.anchor ? std::format("cycle {} page {}", since.anchor->position, since.anchor->page + 1) : std::string("none"));
		field(out, "digest", agent::digest_line(since).empty() ? std::string("none") : agent::digest_line(since));
		field(out, "fork", chat.fork.process ? (chat.fork.process->verdict == agent::inquiry_verdict::pending ? std::string("asking") : std::string("answered")) : chat.fork.open ? std::string("open") : std::string("off"));
		if (chat.fork.process) {
			field(out, "fork_about", chat.fork.about);
			field(out, "fork_answer_chars", std::format("{}", chat.fork.process->answer.size()));
		}
	}

	std::uint32_t sectioned = 0;
	section(out, "diff_sections");
	for (const agent::session& chat : src.chats.sessions) {
		for (const agent::diff_section& found : agent::diff_sections_of(chat)) {
			if (sectioned >= rows) {
				break;
			}
			item(out);
			field(out, "chat", std::format("{}", chat.id));
			field(out, "section", found.label);
			field(out, "row", std::format("{}", found.row));
			++sectioned;
		}
	}
	count(out, "diff_sections_shown", sectioned);
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
