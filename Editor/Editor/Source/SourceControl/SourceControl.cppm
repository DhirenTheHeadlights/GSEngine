export module gse.ide.source_control;

import std;
import gse;

import gse.ide.agent;
import gse.ide.git;
import gse.ide.navigation;

export namespace gse::ide {
	struct repository_edit {
		std::string key;
		std::string commit_key;
		std::string push_key;
		std::string draft_key;
		std::string message;
		std::string draft_status;
		bool drafting = false;
		gui::text_input_state input;
	};

	struct source_control_state {
		std::unordered_map<id, repository_edit> edits;
		std::unordered_set<id> selected;
	};

	struct source_control_inputs {
		git::status_snapshot status;
		bool busy = false;
		std::string_view action_error;
		std::filesystem::path project_root;
		std::filesystem::path engine_root;
		std::string_view engine_pin;
	};

	[[nodiscard]] auto edit_for(
		source_control_state& state,
		const std::filesystem::path& root
	) -> repository_edit&;

	auto apply_draft(
		source_control_state& state,
		const agent::draft_ready& drafted
	) -> void;

	auto draw_source_control_panel(
		gui::builder& ui,
		const rectf& rect,
		source_control_state& state,
		const source_control_inputs& inputs,
		channel_write<git_system::action_request, agent::draft_request, jump_to_request> channels
	) -> void;
}

namespace gse::ide {
	struct row_context {
		gui::builder& ui;
		const rectf& rect;
		const rectf& visible;
		bool hovered = false;
		float counter_width = 0.f;
		source_control_state& state;
		const source_control_inputs& inputs;
		const git::repository_status& repository;
		const git::change* change = nullptr;
		channel_write<git_system::action_request, agent::draft_request, jump_to_request> channels;
	};

	auto draw_header_row(
		const row_context& row
	) -> void;

	auto draw_controls_row(
		const row_context& row
	) -> void;

	auto draw_change_row(
		const row_context& row
	) -> void;

	struct source_control_row_info {
		void (*draw)(const row_context&) = nullptr;
	};

	enum class source_control_row_kind {
		header [[= source_control_row_info{
			.draw = draw_header_row,
		}]],
		controls [[= source_control_row_info{
			.draw = draw_controls_row,
		}]],
		file [[= source_control_row_info{
			.draw = draw_change_row,
		}]],
	};

	struct source_control_row {
		source_control_row_kind kind = source_control_row_kind::header;
		const git::repository_status* repository = nullptr;
		const git::change* change = nullptr;
	};

	[[nodiscard]] auto contains_path(
		const std::filesystem::path& root,
		const std::filesystem::path& path
	) -> bool;

	[[nodiscard]] auto collect_rows(
		const git::status_map& status,
		const source_control_inputs& inputs
	) -> std::vector<source_control_row>;

	[[nodiscard]] auto change_id(
		const git::repository_status& repository,
		const git::change& change
	) -> id;

	[[nodiscard]] auto all_selected(
		const source_control_state& state,
		const git::repository_status& repository
	) -> bool;

	auto select_all(
		source_control_state& state,
		const git::repository_status& repository,
		bool selected
	) -> void;

	[[nodiscard]] auto selected_paths(
		const source_control_state& state,
		const git::repository_status& repository
	) -> std::vector<std::filesystem::path>;

	[[nodiscard]] auto checkbox_rect(
		const gui::draw_context& ctx,
		const rectf& row,
		float left
	) -> rectf;

	[[nodiscard]] auto counter_column_width(
		const font& face,
		const git::status_map& status,
		float scale
	) -> float;
}

auto gse::ide::contains_path(const std::filesystem::path& root, const std::filesystem::path& path) -> bool {
	const std::filesystem::path relative = path.lexically_normal().lexically_relative(root.lexically_normal());
	return !relative.empty() && *relative.begin() != "..";
}

auto gse::ide::collect_rows(const git::status_map& status, const source_control_inputs& inputs) -> std::vector<source_control_row> {
	std::vector<source_control_row> rows;
	for (const git::repository_snapshot& repository : status.repositories()) {
		if (!contains_path(repository->root, inputs.project_root) && !contains_path(repository->root, inputs.engine_root)) {
			continue;
		}
		rows.push_back({
			.kind = source_control_row_kind::header,
			.repository = repository.get(),
		});
		rows.push_back({
			.kind = source_control_row_kind::controls,
			.repository = repository.get(),
		});
		for (const git::change& change : repository->changes) {
			rows.push_back({
				.kind = source_control_row_kind::file,
				.repository = repository.get(),
				.change = &change,
			});
		}
	}
	return rows;
}

auto gse::ide::edit_for(source_control_state& state, const std::filesystem::path& root) -> repository_edit& {
	const id root_id = generate_temp_id(root);
	const auto found = state.edits.find(root_id);
	if (found != state.edits.end()) {
		return found->second;
	}
	const std::string root_key = root.generic_display_string();
	return state.edits.emplace(root_id, repository_edit{
		.key = "##source_control_message_" + root_key,
		.commit_key = "##source_control_commit_" + root_key,
		.push_key = "##source_control_push_" + root_key,
		.draft_key = "##source_control_draft_" + root_key,
	}).first->second;
}

auto gse::ide::apply_draft(source_control_state& state, const agent::draft_ready& drafted) -> void {
	repository_edit& edit = edit_for(state, drafted.root);
	edit.drafting = false;
	if (!drafted.failure.empty()) {
		edit.draft_status = "draft failed: " + drafted.failure;
		return;
	}
	edit.draft_status.clear();
	edit.message = drafted.message;
	edit.input = {};
}

auto gse::ide::change_id(const git::repository_status& repository, const git::change& change) -> id {
	return generate_temp_id(repository.root / change.relative);
}

auto gse::ide::all_selected(const source_control_state& state, const git::repository_status& repository) -> bool {
	return !repository.changes.empty() && std::ranges::all_of(repository.changes, [&](const git::change& change) {
		return state.selected.contains(change_id(repository, change));
	});
}

auto gse::ide::select_all(source_control_state& state, const git::repository_status& repository, const bool selected) -> void {
	for (const git::change& change : repository.changes) {
		if (selected) {
			state.selected.insert(change_id(repository, change));
		}
		else {
			state.selected.erase(change_id(repository, change));
		}
	}
}

auto gse::ide::selected_paths(const source_control_state& state, const git::repository_status& repository) -> std::vector<std::filesystem::path> {
	std::vector<std::filesystem::path> paths;
	for (const git::change& change : repository.changes) {
		if (state.selected.contains(change_id(repository, change))) {
			paths.push_back(change.relative);
		}
	}
	return paths;
}

auto gse::ide::checkbox_rect(const gui::draw_context& ctx, const rectf& row, const float left) -> rectf {
	const float extent = gui::checkbox_extent(ctx.style);
	return rectf::from_position_size(
		{ left, row.center().y() + extent * 0.5f },
		{ extent, extent }
	);
}

auto gse::ide::counter_column_width(const font& face, const git::status_map& status, const float scale) -> float {
	float widest = 0.f;
	for (const git::repository_snapshot& repository : status.repositories()) {
		for (const git::change& change : repository->changes) {
			widest = std::max({
				widest,
				face.width(std::format("+{}", change.added), scale),
				face.width(std::format("-{}", change.deleted), scale),
			});
		}
	}
	return widest;
}

auto gse::ide::draw_header_row(const row_context& row) -> void {
	const auto& ctx = row.ui.ctx;
	const auto& sty = ctx.style;
	const auto text_view = ctx.fonts.text.resolve();
	const float pad = sty.padding;
	const git::repository_status& repository = row.repository;

	ctx.queue_sprite({
		.rect = row.rect,
		.color = sty.color_panel_alt,
		.texture = ctx.blank_texture,
	});
	ctx.queue_sprite({
		.rect = rectf::from_position_size({ row.rect.left(), row.rect.top() }, { sty.accent_bar_width, row.rect.height() }),
		.color = sty.color_accent,
		.texture = ctx.blank_texture,
	});

	const rectf box = checkbox_rect(ctx, row.rect, row.rect.left() + sty.accent_bar_width + pad);
	const bool all = all_selected(row.state, repository);
	if (!repository.changes.empty() && ctx.clicked_in_rect(box.intersection(row.visible))) {
		select_all(row.state, repository, !all);
	}
	gui::draw_checkbox(ctx, box, all);

	const std::string name = repository.root.filename().generic_display_string();
	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = name,
		.position = { box.right() + pad, row.rect.center().y() + text_view->vertical_center_offset(sty.font_size) },
		.scale = sty.font_size,
		.color = sty.color_text,
		.clip_rect = row.rect,
	});

	const bool engine = generate_temp_id(repository.root) == generate_temp_id(row.inputs.engine_root);
	const std::string_view pin = row.inputs.engine_pin;
	std::string branch = repository.branch;
	if (repository.ahead > 0) {
		branch += std::format(" +{}", repository.ahead);
	}
	if (repository.behind > 0) {
		branch += std::format(" -{}", repository.behind);
	}
	if (repository.upstream.empty()) {
		branch += " (no upstream)";
	}
	const bool drifted = engine && !pin.empty() && pin != repository.head;
	if (engine) {
		branch += pin.empty() ? " · unpinned" : drifted ? std::format(" · drifted from {}", pin.substr(0, 7)) : " · pinned";
	}
	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = branch,
		.position = { row.rect.right() - pad - text_view->width(branch, sty.font_size), row.rect.center().y() + text_view->vertical_center_offset(sty.font_size) },
		.scale = sty.font_size,
		.color = drifted ? sty.color_warning : sty.color_text_secondary,
		.clip_rect = row.rect,
	});
}

auto gse::ide::draw_controls_row(const row_context& row) -> void {
	gui::builder& ui = row.ui;
	const auto& ctx = ui.ctx;
	const auto& sty = ctx.style;
	const auto text_view = ctx.fonts.text.resolve();
	const float pad = sty.padding;
	const float button_w = text_view->width("Commit", sty.font_size) + pad * 2.f;
	const float control_h = std::max(0.f, row.rect.height() - pad * 0.5f);
	const float top = row.rect.top() - pad * 0.25f;
	const git::repository_status& repository = row.repository;
	repository_edit& edit = edit_for(row.state, repository.root);

	const rectf push_rect = rectf::from_position_size({ row.rect.right() - pad - button_w, top }, { button_w, control_h });
	const rectf commit_rect = rectf::from_position_size({ push_rect.left() - pad * 0.5f - button_w, top }, { button_w, control_h });
	const rectf draft_rect = rectf::from_position_size({ commit_rect.left() - pad * 0.5f - button_w, top }, { button_w, control_h });
	const rectf input_rect = rectf::from_position_size({ row.rect.left() + pad, top }, { std::max(0.f, draft_rect.left() - pad - row.rect.left() - pad), control_h });

	ui.draw<gui::text_input>({
		.name = edit.key,
		.buffer = edit.message,
		.state = edit.input,
		.rect = input_rect,
	});
	if (edit.message.empty()) {
		const bool failed = !edit.draft_status.empty();
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = edit.drafting ? "Drafting a commit message..." : failed ? edit.draft_status : "Commit message",
			.position = { input_rect.left() + pad, input_rect.center().y() + text_view->vertical_center_offset(sty.font_size) },
			.scale = sty.font_size,
			.color = failed ? sty.color_error : sty.color_text_secondary,
			.clip_rect = input_rect,
		});
	}

	std::vector<std::filesystem::path> paths = selected_paths(row.state, repository);
	const bool can_draft = !row.inputs.busy && !edit.drafting && !paths.empty();
	if (ui.draw<gui::button>({ .text = "Draft", .rect = draft_rect, .key = edit.draft_key, .enabled = can_draft })) {
		edit.drafting = true;
		edit.draft_status.clear();
		std::vector<agent::draft_file> files;
		for (const git::change& change : repository.changes) {
			if (!row.state.selected.contains(change_id(repository, change))) {
				continue;
			}
			files.push_back({
				.relative = change.relative,
				.code = annotation_from_enum<git::file_status_info>(change.state, {}).code,
				.added = change.added,
				.deleted = change.deleted,
			});
		}
		row.channels.push<agent::draft_request>({
			.root = repository.root,
			.branch = repository.branch,
			.files = std::move(files),
		});
	}

	const bool can_commit = !row.inputs.busy && !edit.message.empty() && !paths.empty();
	if (ui.draw<gui::button>({ .text = "Commit", .rect = commit_rect, .key = edit.commit_key, .enabled = can_commit })) {
		select_all(row.state, repository, false);
		row.channels.push<git_system::action_request>({
			.root = repository.root,
			.kind = git_system::action::commit,
			.message = edit.message,
			.paths = std::move(paths),
		});
		edit.message.clear();
		edit.draft_status.clear();
		edit.input = {};
	}

	const bool can_push = !row.inputs.busy && (repository.ahead > 0 || repository.upstream.empty());
	if (ui.draw<gui::button>({ .text = "Push", .rect = push_rect, .key = edit.push_key, .enabled = can_push })) {
		row.channels.push<git_system::action_request>({
			.root = repository.root,
			.kind = git_system::action::push,
		});
	}
}

auto gse::ide::draw_change_row(const row_context& row) -> void {
	const auto& ctx = row.ui.ctx;
	const auto& sty = ctx.style;
	const auto text_view = ctx.fonts.text.resolve();
	const float pad = sty.padding;
	const float text_y = row.rect.center().y() + text_view->vertical_center_offset(sty.font_size);
	const git::change& change = *row.change;

	if (row.hovered) {
		ctx.queue_sprite({
			.rect = row.rect,
			.color = sty.color_widget_hovered,
			.texture = ctx.blank_texture,
		});
	}

	const rectf box = checkbox_rect(ctx, row.rect, row.rect.left() + sty.accent_bar_width + pad);
	const id file_id = change_id(row.repository, change);
	if (ctx.clicked_in_rect(box.intersection(row.visible))) {
		if (!row.state.selected.erase(file_id)) {
			row.state.selected.insert(file_id);
		}
	}
	else if (ctx.clicked_in_rect(row.visible)) {
		row.channels.push<jump_to_request>({
			.path = row.repository.root / change.relative,
		});
	}
	gui::draw_checkbox(ctx, box, row.state.selected.contains(file_id));

	const std::string deleted = std::format("-{}", change.deleted);
	const float deleted_x = row.rect.right() - pad - text_view->width(deleted, sty.font_size);
	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = deleted,
		.position = { deleted_x, text_y },
		.scale = sty.font_size,
		.color = git::status_color(git::file_status::deleted),
		.clip_rect = row.rect,
	});

	const std::string added = std::format("+{}", change.added);
	const float added_right = row.rect.right() - pad - row.counter_width - pad * 0.5f;
	const float added_x = added_right - text_view->width(added, sty.font_size);
	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = added,
		.position = { added_x, text_y },
		.scale = sty.font_size,
		.color = git::status_color(git::file_status::added),
		.clip_rect = row.rect,
	});

	ctx.queue_sprite({
		.rect = rectf::from_position_size({ row.rect.left(), row.rect.top() }, { sty.accent_bar_width, row.rect.height() }),
		.color = git::status_color(change.state),
		.texture = ctx.blank_texture,
	});

	const float text_x = box.right() + pad;
	const float text_right = added_right - row.counter_width - pad;
	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = change.relative.generic_display_string(),
		.position = { text_x, text_y },
		.scale = sty.font_size,
		.color = git::status_color(change.state),
		.clip_rect = rectf::from_position_size({ text_x, row.rect.top() }, { std::max(0.f, text_right - text_x), row.rect.height() }),
	});
}

auto gse::ide::draw_source_control_panel(gui::builder& ui, const rectf& rect, source_control_state& state, const source_control_inputs& inputs, const channel_write<git_system::action_request, agent::draft_request, jump_to_request> channels) -> void {
	auto& ctx = ui.ctx;
	const auto& sty = ctx.style;
	const auto text_view = ctx.fonts.text.resolve();
	const float pad = sty.padding;
	const float row_h = text_view->line_height(sty.font_size) + pad;

	rectf list_rect = rect;
	if (!inputs.action_error.empty()) {
		const rectf error_rect = rectf::from_position_size({ rect.left(), rect.top() }, { rect.width(), std::min(row_h, rect.height()) });
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = inputs.action_error,
			.position = { error_rect.left() + pad, error_rect.center().y() + text_view->vertical_center_offset(sty.font_size) },
			.scale = sty.font_size,
			.color = sty.color_error,
			.clip_rect = error_rect,
		});
		list_rect = rectf::from_position_size({ rect.left(), error_rect.bottom() }, { rect.width(), std::max(0.f, rect.height() - error_rect.height()) });
	}

	if (!inputs.status || inputs.status->empty()) {
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = "No git repositories in the open roots.",
			.position = { list_rect.left() + pad, list_rect.top() - row_h },
			.scale = sty.font_size,
			.color = sty.color_text_secondary,
			.clip_rect = list_rect,
		});
		return;
	}

	const std::vector<source_control_row> rows = collect_rows(*inputs.status, inputs);
	const float counter_width = counter_column_width(*text_view, *inputs.status, sty.font_size);

	ui.row_list({
		.id = "##source_control_list",
		.bounds = list_rect,
		.row_height = row_h,
		.row_count = rows.size(),
	}, [&](gui::builder& b, const gui::row& r) {
		const source_control_row& item = rows[r.index];
		annotation_from_enum<source_control_row_info>(item.kind, {}).draw({
			.ui = b,
			.rect = r.rect,
			.visible = r.visible,
			.hovered = r.hovered,
			.counter_width = counter_width,
			.state = state,
			.inputs = inputs,
			.repository = *item.repository,
			.change = item.change,
			.channels = channels,
		});
	});
}
