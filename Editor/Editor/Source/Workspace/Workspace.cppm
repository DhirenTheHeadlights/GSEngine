export module gse.ide.workspace:workspace;

import std;
import gse;
import gse.win32;

import gse.ide.alloc;
import gse.ide.docs;
import gse.ide.profile;

import :documents;

export namespace gse::ide {
	enum class document_save_mode {
		preserve_external,
		overwrite_external,
	};

	enum class document_save_result {
		saved,
		unchanged,
		conflict,
		failed,
	};

	enum class document_prompt_kind {
		close_dirty,
		save_conflict,
	};

	struct hover_state {
		std::uint32_t line = 0;
		std::uint32_t column = 0;
		std::string ident;
		gse::time since{};
		bool resolved = false;
		bool has_card = false;
		bool pending = false;
		std::string title;
		std::string kind;
		std::string body;
		std::vector<std::string> issues;
		std::string url;
		bool from_cppref = false;
		bool body_is_code = false;
		float scroll = 0.f;
		float scroll_x = 0.f;
		gse::gui::scroll_axis x_axis;
		gse::gui::scroll_axis y_axis;
		std::vector<gse::gui::text_span> code_spans;
		gse::vec4f kind_color;
		gse::vec2f anchor;
		gse::rect_t<gse::vec2f> panel;
		gse::rect_t<gse::vec2f> code_rect;
	};

	struct quickfix_popup {
		gse::id document_id;
		std::uint32_t line = 0;
		std::uint32_t start_col = 0;
		gse::vec2f anchor;
		gse::rect_t<gse::vec2f> panel;
	};

	struct document_prompt {
		gse::id document_id;
		document_prompt_kind kind = document_prompt_kind::close_dirty;
		bool close_after_resolution = false;
	};

	enum class fs_node_role {
		entry,
		browse_root,
	};

	struct fs_node {
		std::filesystem::path path;
		std::string name;
		std::uint64_t key = 0;
		bool is_dir = false;
		fs_node_role role = fs_node_role::entry;
		gse::gui::symbol_glyph glyph = nullptr;
		bool loaded = false;
		std::filesystem::file_time_type write_time{};
		std::vector<fs_node> children;
	};

	struct explorer_target {
		std::filesystem::path path;
		std::uint64_t key = 0;
		bool is_dir = false;
		fs_node_role role = fs_node_role::entry;
	};

	struct navigation_entry {
		std::filesystem::path path;
		std::uint32_t line = 0;
		std::uint32_t column = 0;
	};

	struct pending_explorer_name {
		std::filesystem::path path;
		std::filesystem::path parent;
		std::string name;
		std::uint64_t key = 0;
		bool is_dir = false;
		bool created = false;
		bool focus_requested = false;
		gse::gui::text_input_state input;
	};

	struct explorer_reveal {
		std::vector<std::uint64_t> expand;
		std::uint64_t key = 0;
	};

	enum class game_view_kind {
		game [[= view_label{ .text = "Game" }]],
		graph [[= view_label{ .text = "Graph" }]],
		profile [[= view_label{ .text = "Profile" }]],
		alloc [[= view_label{ .text = "Alloc" }]],
	};

	constexpr auto game_view_label(
		game_view_kind kind
	) -> view_label;

	struct workspace {
		struct data {
			open_documents documents;
			std::vector<navigation_entry> back_stack;
			std::vector<navigation_entry> forward_stack;
			gse::gui::tab_strip_state tab_strip;
			gse::gui::tab_strip_state game_tab_strip;
			game_view_kind game_view = game_view_kind::game;
			profile_view_state profile;
			alloc_view_state alloc;
			bool game_captured = false;
			int game_capture_settle_frames = 0;
			gse::vec2f game_cursor{ 0.f, 0.f };
			gse::vec2f game_input_scale{ 1.f, 1.f };
			gse::file_watcher watcher;

			fs_node fs_root;
			gse::gui::draw::tree_selection explorer_selection;
			std::unordered_set<std::uint64_t> explorer_selection_seen;
			std::optional<pending_explorer_name> pending_name;
			std::optional<std::filesystem::path> reveal_path;
			std::optional<explorer_reveal> pending_reveal;
			std::vector<std::uint64_t> explorer_load_requests;
			gse::interval_timer<> explorer_scan_timer{ gse::milliseconds(500.f) };
			bool commit_name_requested = false;
			bool cancel_name_requested = false;
			std::string explorer_error;

			std::optional<gse::id> diagnostics_pending;
			gse::clock diagnostics_clock;
			bool diagnostics_paused_for_build = false;
			std::optional<document_prompt> pending_document_prompt;

			std::vector<hover_state> hover_stack;
			std::optional<quickfix_popup> quickfix;
			docs::cppref_index cppref;
		};

		static auto unique_tab_name(
			const data& d,
			const std::filesystem::path& path
		) -> std::string;

		static auto open_file(
			data& d,
			const std::filesystem::path& path
		) -> gse::id;

		[[nodiscard]] static auto game_active(
			const data& d
		) -> bool;

		[[nodiscard]] static auto active_document_id(
			const data& d
		) -> std::optional<gse::id>;

		[[nodiscard]] static auto active_tab_id(
			const data& d
		) -> gse::id;

		static auto activate_game(
			data& d
		) -> void;

		static auto activate_document(
			data& d,
			gse::id document_id
		) -> void;

		static auto reorder_document(
			data& d,
			gse::id document_id,
			std::size_t index
		) -> void;

		static auto jump_to(
			data& d,
			const std::filesystem::path& path,
			std::uint32_t line,
			std::uint32_t column
		) -> void;

		static auto go_back(
			data& d
		) -> void;

		static auto go_forward(
			data& d
		) -> void;

		static auto save_document(
			data& d,
			gse::id document_id,
			document_save_mode mode = document_save_mode::preserve_external
		) -> document_save_result;

		static auto close_document(
			data& d,
			gse::id document_id
		) -> bool;

		static auto discard_document(
			data& d,
			gse::id document_id
		) -> void;

		static auto discard_document_changes(
			data& d,
			gse::id document_id
		) -> bool;

		static auto save_dirty_documents(
			data& d
		) -> void;

		static auto load_children(
			fs_node& node
		) -> void;

		static auto make_root(
			const std::filesystem::path& path,
			std::string name,
			gse::gui::symbol_glyph glyph
		) -> fs_node;

		static auto find_node(
			fs_node& root,
			std::uint64_t key
		) -> fs_node*;

		static auto reveal_nodes(
			data& d,
			const std::filesystem::path& path
		) -> explorer_reveal;

		static auto target(
			const fs_node& node
		) -> explorer_target;

		static auto can_create_entry(
			const data& d,
			const explorer_target& target
		) -> bool;

		static auto can_mutate_entry(
			const data& d,
			const explorer_target& target
		) -> bool;

		static auto create_entry(
			data& d,
			explorer_target target,
			bool is_dir
		) -> void;

		static auto rename_entry(
			data& d,
			explorer_target target
		) -> void;

		static auto commit_pending_name(
			data& d
		) -> void;

		static auto cancel_pending_name(
			data& d
		) -> void;

		static auto delete_entry(
			data& d,
			explorer_target target
		) -> void;

		static auto request_children(
			data& d,
			std::uint64_t key
		) -> void;

		static auto update_explorer(
			data& d
		) -> void;

		static auto reload_document_from_disk(
			data& d,
			const std::filesystem::path& path
		) -> void;
	};
}

namespace gse::ide {
	constexpr std::size_t max_navigation_history = 256;

	struct loaded_document {
		gse::gui::text_buffer buffer;
		std::filesystem::file_time_type write_time;
	};

	auto fs_node_key(
		const std::filesystem::path& path
	) -> std::uint64_t;

	auto refresh_directory(
		workspace::data& d,
		const std::filesystem::path& path
	) -> void;

	auto sync_directory_changes(
		fs_node& node
	) -> void;

	auto normalized_path(
		const std::filesystem::path& path
	) -> std::filesystem::path;

	auto normalized_navigation_entry(
		const std::filesystem::path& path,
		const std::uint32_t line,
		const std::uint32_t column
	) -> navigation_entry;

	auto same_navigation_path(
		const std::filesystem::path& lhs,
		const std::filesystem::path& rhs
	) -> bool;

	auto path_within(
		const std::filesystem::path& base,
		const std::filesystem::path& path
	) -> bool;

	auto path_strictly_within(
		const std::filesystem::path& base,
		const std::filesystem::path& path
	) -> bool;

	auto same_navigation_entry(
		const navigation_entry& lhs,
		const navigation_entry& rhs
	) -> bool;

	auto current_navigation_entry(
		const workspace::data& d
	) -> std::optional<navigation_entry>;

	auto push_navigation_entry(
		std::vector<navigation_entry>& stack,
		const navigation_entry& entry
	) -> void;

	auto jump_to_without_history(
		workspace::data& d,
		const navigation_entry& target
	) -> bool;

	auto highlightable_path(
		const std::filesystem::path& path
	) -> bool;

	auto tab_name_taken(
		const workspace::data& d,
		std::string_view candidate
	) -> bool;

	auto write_document_file(
		const std::filesystem::path& path,
		std::span<const std::string> lines
	) -> std::expected<void, std::string>;

	auto file_write_time(
		const std::filesystem::path& path
	) -> std::expected<std::filesystem::file_time_type, std::string>;

	auto read_document_file(
		const std::filesystem::path& path
	) -> std::expected<loaded_document, std::string>;

	auto preserve_document_recovery(
		const document& doc,
		gse::id document_id
	) -> void;

	auto remapped_path(
		const std::filesystem::path& path,
		const std::filesystem::path& old_base,
		const std::filesystem::path& new_base,
		bool include_descendants
	) -> std::optional<std::filesystem::path>;

	auto remap_navigation_entries(
		std::vector<navigation_entry>& entries,
		const std::filesystem::path& old_base,
		const std::filesystem::path& new_base,
		bool include_descendants
	) -> void;

	auto prune_navigation_entries(
		std::vector<navigation_entry>& entries,
		const std::filesystem::path& base
	) -> void;
}

constexpr auto gse::ide::game_view_label(const game_view_kind kind) -> view_label {
	return gse::annotation_from_enum<view_label>(kind, { .text = "" });
}

auto gse::ide::fs_node_key(const std::filesystem::path& path) -> std::uint64_t {
	return generate_temp_id(path).number();
}

auto gse::ide::refresh_directory(workspace::data& d, const std::filesystem::path& path) -> void {
	if (fs_node* node = workspace::find_node(d.fs_root, fs_node_key(path))) {
		workspace::load_children(*node);
	}
}

auto gse::ide::sync_directory_changes(fs_node& node) -> void {
	if (node.is_dir && node.loaded && !node.path.empty() && file_write_time(node.path).value_or(std::filesystem::file_time_type{}) != node.write_time) {
		workspace::load_children(node);
	}
	for (fs_node& child : node.children) {
		sync_directory_changes(child);
	}
}

auto gse::ide::normalized_path(const std::filesystem::path& path) -> std::filesystem::path {
	std::error_code ec;
	const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, ec);
	return (ec ? path : canonical).lexically_normal();
}

auto gse::ide::normalized_navigation_entry(const std::filesystem::path& path, const std::uint32_t line, const std::uint32_t column) -> navigation_entry {
	return {
		.path = normalized_path(path),
		.line = line,
		.column = column,
	};
}

auto gse::ide::same_navigation_path(const std::filesystem::path& lhs, const std::filesystem::path& rhs) -> bool {
	std::error_code ec;
	if (std::filesystem::equivalent(lhs, rhs, ec)) {
		return true;
	}
	return normalized_path(lhs) == normalized_path(rhs);
}

auto gse::ide::path_within(const std::filesystem::path& base, const std::filesystem::path& path) -> bool {
	const std::filesystem::path rel = normalized_path(path).lexically_relative(normalized_path(base));
	return !rel.empty() && *rel.begin() != "..";
}

auto gse::ide::path_strictly_within(const std::filesystem::path& base, const std::filesystem::path& path) -> bool {
	const std::filesystem::path rel = normalized_path(path).lexically_relative(normalized_path(base));
	return !rel.empty() && rel != "." && *rel.begin() != "..";
}

auto gse::ide::same_navigation_entry(const navigation_entry& lhs, const navigation_entry& rhs) -> bool {
	return lhs.line == rhs.line && lhs.column == rhs.column && same_navigation_path(lhs.path, rhs.path);
}

auto gse::ide::current_navigation_entry(const workspace::data& d) -> std::optional<navigation_entry> {
	const std::optional<id> active_document_id = workspace::active_document_id(d);
	if (!active_document_id) {
		return std::nullopt;
	}
	const auto it = d.documents.find(*active_document_id);
	if (it == d.documents.end() || it->second.path.empty()) {
		return std::nullopt;
	}

	const document& doc = it->second;
	const gse::gui::buffer_position caret = doc.buffer.clamp(doc.view.caret);
	return navigation_entry{
		.path = doc.path,
		.line = caret.line,
		.column = caret.column,
	};
}

auto gse::ide::push_navigation_entry(std::vector<navigation_entry>& stack, const navigation_entry& entry) -> void {
	if (!stack.empty() && same_navigation_entry(stack.back(), entry)) {
		return;
	}
	stack.push_back(entry);
	if (stack.size() > max_navigation_history) {
		stack.erase(stack.begin());
	}
}

auto gse::ide::jump_to_without_history(workspace::data& d, const navigation_entry& target) -> bool {
	const id id = workspace::open_file(d, target.path);
	const auto it = d.documents.find(id);
	if (it == d.documents.end()) {
		return false;
	}
	document& doc = it->second;
	doc.view.caret = doc.buffer.clamp({
		.line = target.line,
		.column = target.column,
	});
	doc.view.anchor = doc.view.caret;
	doc.pending_center_line = doc.view.caret.line;
	workspace::activate_document(d, id);
	return true;
}

auto gse::ide::highlightable_path(const std::filesystem::path& path) -> bool {
	std::string ext = path.extension().native_encoded_string();
	std::ranges::transform(ext, ext.begin(), [](const unsigned char ch) {
		return static_cast<char>(std::tolower(ch));
	});
	constexpr std::array extensions{
		std::string_view(".cpp"),
		std::string_view(".cppm"),
		std::string_view(".cc"),
		std::string_view(".cxx"),
		std::string_view(".ixx"),
		std::string_view(".c"),
		std::string_view(".h"),
		std::string_view(".hpp"),
		std::string_view(".hh"),
		std::string_view(".hxx"),
		std::string_view(".inl"),
	};
	return std::ranges::find(extensions, ext) != extensions.end();
}

auto gse::ide::write_document_file(const std::filesystem::path& path, const std::span<const std::string> lines) -> std::expected<void, std::string> {
	std::filesystem::path temporary = path;
	temporary += std::format(".{}.tmp", win32::GetCurrentProcessId());
	const auto remove_temporary = make_scope_exit([&] {
		std::error_code ec;
		std::filesystem::remove(temporary, ec);
	});

	std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
	if (!out) {
		return std::unexpected(std::format("could not open '{}' for writing", temporary.generic_display_string()));
	}
	for (std::size_t i = 0; i < lines.size(); ++i) {
		out << lines[i];
		if (i + 1 < lines.size()) {
			out << '\n';
		}
	}
	out.close();
	if (!out) {
		return std::unexpected(std::format("could not finish writing '{}'", temporary.generic_display_string()));
	}
	if (!win32::MoveFileExW(temporary.c_str(), path.c_str(), win32::movefile_replace_existing)) {
		return std::unexpected(std::format("could not replace '{}' (Windows error {})", path.generic_display_string(), win32::GetLastError()));
	}
	return {};
}

auto gse::ide::file_write_time(const std::filesystem::path& path) -> std::expected<std::filesystem::file_time_type, std::string> {
	std::error_code ec;
	const std::filesystem::file_time_type value = std::filesystem::last_write_time(path, ec);
	if (ec) {
		return std::unexpected(std::format("could not inspect '{}': {}", path.generic_display_string(), ec.message()));
	}
	return value;
}

auto gse::ide::read_document_file(const std::filesystem::path& path) -> std::expected<loaded_document, std::string> {
	for (int attempt = 0; attempt < 2; ++attempt) {
		const auto before = file_write_time(path);
		if (!before) {
			return std::unexpected(before.error());
		}
		std::expected<gse::gui::text_buffer, std::string> buffer = gse::gui::text_buffer::from_file(path);
		if (!buffer) {
			return std::unexpected(buffer.error());
		}
		const auto after = file_write_time(path);
		if (!after) {
			return std::unexpected(after.error());
		}
		if (*before == *after) {
			return loaded_document{
				.buffer = std::move(*buffer),
				.write_time = *after,
			};
		}
	}
	return std::unexpected(std::format("'{}' changed while it was being read", path.generic_display_string()));
}

auto gse::ide::remapped_path(const std::filesystem::path& path, const std::filesystem::path& old_base, const std::filesystem::path& new_base, const bool include_descendants) -> std::optional<std::filesystem::path> {
	const std::filesystem::path key = normalized_path(path);
	if (key == old_base) {
		return new_base;
	}
	if (!include_descendants) {
		return std::nullopt;
	}
	const std::filesystem::path rel = key.lexically_relative(old_base);
	if (rel.empty() || *rel.begin() == "..") {
		return std::nullopt;
	}
	return new_base / rel;
}

auto gse::ide::remap_navigation_entries(std::vector<navigation_entry>& entries, const std::filesystem::path& old_base, const std::filesystem::path& new_base, const bool include_descendants) -> void {
	for (navigation_entry& entry : entries) {
		if (const auto next = remapped_path(entry.path, old_base, new_base, include_descendants)) {
			entry.path = *next;
		}
	}
}

auto gse::ide::prune_navigation_entries(std::vector<navigation_entry>& entries, const std::filesystem::path& base) -> void {
	std::erase_if(entries, [&](const navigation_entry& entry) {
		return path_within(base, entry.path);
	});
}

auto gse::ide::tab_name_taken(const workspace::data& d, const std::string_view candidate) -> bool {
	return std::ranges::any_of(std::views::values(d.documents), [candidate](const document& doc) {
		return doc.tab_name == candidate;
	});
}

auto gse::ide::workspace::unique_tab_name(const data& d, const std::filesystem::path& path) -> std::string {
	const std::string base = path.empty() ? "untitled" : path.filename().generic_display_string();

	if (!tab_name_taken(d, base)) {
		return base;
	}

	if (path.has_parent_path()) {
		if (const std::string parent = path.parent_path().filename().generic_display_string(); !parent.empty()) {
			if (std::string scoped = base + " (" + parent + ")"; !tab_name_taken(d, scoped)) {
				return scoped;
			}
		}
	}

	for (int suffix = 2;; ++suffix) {
		if (std::string candidate = base + " (" + std::to_string(suffix) + ")"; !tab_name_taken(d, candidate)) {
			return candidate;
		}
	}
}

auto gse::ide::workspace::open_file(data& d, const std::filesystem::path& path) -> id {
	const std::filesystem::path key = normalized_path(path);

	for (const auto& [id, doc] : d.documents) {
		if (doc.path == key) {
			activate_document(d, id);
			d.reveal_path = key;
			return id;
		}
	}
	std::expected<loaded_document, std::string> loaded = read_document_file(key);
	if (!loaded) {
		d.explorer_error = loaded.error();
		log::println(log::level::error, log::category::general, "workspace: {}", loaded.error());
		return {};
	}

	document doc;
	doc.path = key;
	doc.tab_name = unique_tab_name(d, key);
	doc.buffer = std::move(loaded->buffer);
	doc.highlightable = highlightable_path(key);
	doc.disk_write_time = loaded->write_time;

	const id id = d.documents.add(std::move(doc));
	d.watcher.watch(key, [&d](const std::filesystem::path& changed_path) {
		workspace::reload_document_from_disk(d, changed_path);
	});

	d.explorer_error.clear();
	activate_document(d, id);
	d.reveal_path = key;
	return id;
}

auto gse::ide::workspace::game_active(const data& d) -> bool {
	return d.documents.game_active();
}

auto gse::ide::workspace::active_document_id(const data& d) -> std::optional<id> {
	return d.documents.active_document_id();
}

auto gse::ide::workspace::active_tab_id(const data& d) -> id {
	return d.documents.active_tab_id();
}

auto gse::ide::workspace::activate_game(data& d) -> void {
	d.documents.activate_game();
}

auto gse::ide::workspace::activate_document(data& d, const id document_id) -> void {
	d.documents.activate(document_id);
}

auto gse::ide::workspace::reorder_document(data& d, const id document_id, const std::size_t index) -> void {
	d.documents.reorder(document_id, index);
}

auto gse::ide::workspace::jump_to(data& d, const std::filesystem::path& path, const std::uint32_t line, const std::uint32_t column) -> void {
	const navigation_entry target = normalized_navigation_entry(path, line, column);
	const std::optional<navigation_entry> current = current_navigation_entry(d);
	const bool record_current = current && !same_navigation_entry(*current, target);
	if (!jump_to_without_history(d, target)) {
		return;
	}
	if (record_current) {
		push_navigation_entry(d.back_stack, *current);
		d.forward_stack.clear();
	}
}

auto gse::ide::workspace::go_back(data& d) -> void {
	const std::optional<navigation_entry> current = current_navigation_entry(d);
	while (!d.back_stack.empty()) {
		const navigation_entry target = d.back_stack.back();
		d.back_stack.pop_back();
		if (current && same_navigation_entry(*current, target)) {
			continue;
		}
		if (jump_to_without_history(d, target)) {
			if (current) {
				push_navigation_entry(d.forward_stack, *current);
			}
			return;
		}
	}
}

auto gse::ide::workspace::go_forward(data& d) -> void {
	const std::optional<navigation_entry> current = current_navigation_entry(d);
	while (!d.forward_stack.empty()) {
		const navigation_entry target = d.forward_stack.back();
		d.forward_stack.pop_back();
		if (current && same_navigation_entry(*current, target)) {
			continue;
		}
		if (jump_to_without_history(d, target)) {
			if (current) {
				push_navigation_entry(d.back_stack, *current);
			}
			return;
		}
	}
}

auto gse::ide::workspace::save_document(data& d, const id document_id, const document_save_mode mode) -> document_save_result {
	const auto it = d.documents.find(document_id);
	if (it == d.documents.end() || it->second.path.empty()) {
		return document_save_result::failed;
	}

	document& doc = it->second;
	if (doc.persistence == document_persistence::clean) {
		return document_save_result::unchanged;
	}
	if (doc.persistence == document_persistence::conflicted && mode == document_save_mode::preserve_external) {
		d.pending_document_prompt = {
			.document_id = document_id,
			.kind = document_prompt_kind::save_conflict,
		};
		return document_save_result::conflict;
	}
	if (mode == document_save_mode::preserve_external) {
		const auto write_time = file_write_time(doc.path);
		if (!write_time || !doc.disk_write_time || *write_time != *doc.disk_write_time) {
			doc.persistence = document_persistence::conflicted;
			doc.persistence_error = write_time ? "file changed on disk while this document had local changes" : write_time.error();
			d.pending_document_prompt = {
				.document_id = document_id,
				.kind = document_prompt_kind::save_conflict,
			};
			return document_save_result::conflict;
		}
	}

	const std::expected<void, std::string> saved = write_document_file(doc.path, doc.buffer.lines);
	if (!saved) {
		doc.persistence_error = saved.error();
		log::println(log::level::error, log::category::general, "workspace: {}", saved.error());
		return document_save_result::failed;
	}
	doc.persistence = document_persistence::clean;
	doc.persistence_error.clear();
	if (const auto write_time = file_write_time(doc.path)) {
		doc.disk_write_time = *write_time;
	}
	else {
		doc.disk_write_time.reset();
		doc.persistence_error = write_time.error();
	}
	return document_save_result::saved;
}

auto gse::ide::workspace::reload_document_from_disk(data& d, const std::filesystem::path& path) -> void {
	for (document& doc : std::views::values(d.documents)) {
		if (doc.path.empty() || !same_navigation_path(doc.path, path)) {
			continue;
		}
		if (doc.persistence != document_persistence::clean) {
			doc.persistence = document_persistence::conflicted;
			doc.persistence_error = "file changed on disk while this document had local changes";
			return;
		}

		std::expected<loaded_document, std::string> loaded = read_document_file(doc.path);
		if (!loaded) {
			doc.persistence = document_persistence::conflicted;
			doc.persistence_error = loaded.error();
			log::println(log::level::error, log::category::general, "workspace: {}", loaded.error());
			return;
		}
		const gse::gui::buffer_position caret = doc.view.caret;
		const gse::gui::buffer_position anchor = doc.view.anchor;
		doc.buffer = std::move(loaded->buffer);
		doc.disk_write_time = loaded->write_time;
		++doc.revision.value;
		doc.view.caret = doc.buffer.clamp(caret);
		doc.view.anchor = doc.buffer.clamp(anchor);
		doc.syntax = {};
		doc.highlight_dirty = true;
		doc.diag_dirty = true;
		doc.edit_clock.reset();
		doc.persistence_error.clear();
		return;
	}
}

auto gse::ide::preserve_document_recovery(const document& doc, const id document_id) -> void {
	const std::filesystem::path recovery_directory = gse::config::user_state_dir() / "recovery";
	std::error_code ec;
	std::filesystem::create_directories(recovery_directory, ec);
	if (ec) {
		log::println(log::level::error, log::category::general, "workspace: could not create recovery directory '{}': {}", recovery_directory, ec.message());
		return;
	}
	const std::filesystem::path recovery = recovery_directory / std::format("{}.{}.{}.recovery", doc.path.filename().native_encoded_string(), win32::GetCurrentProcessId(), document_id);
	const std::expected<void, std::string> saved = write_document_file(recovery, doc.buffer.lines);
	if (saved) {
		log::println(log::level::warning, log::category::general, "workspace: preserved unsaved document at '{}'", recovery);
	}
	else {
		log::println(log::level::error, log::category::general, "workspace: {}", saved.error());
	}
}

auto gse::ide::workspace::close_document(data& d, const id document_id) -> bool {
	const auto it = d.documents.find(document_id);
	if (it == d.documents.end()) {
		return true;
	}
	if (it->second.persistence != document_persistence::clean) {
		d.pending_document_prompt = {
			.document_id = document_id,
			.kind = document_prompt_kind::close_dirty,
		};
		return false;
	}
	discard_document(d, document_id);
	return true;
}

auto gse::ide::workspace::discard_document(data& d, const id document_id) -> void {
	if (const auto it = d.documents.find(document_id); it != d.documents.end() && !it->second.path.empty()) {
		d.watcher.unwatch(it->second.path);
	}
	d.documents.erase(document_id);
	if (d.pending_document_prompt && d.pending_document_prompt->document_id == document_id) {
		d.pending_document_prompt.reset();
	}
}

auto gse::ide::workspace::discard_document_changes(data& d, const id document_id) -> bool {
	const auto it = d.documents.find(document_id);
	if (it == d.documents.end() || it->second.path.empty()) {
		return false;
	}
	document& doc = it->second;
	std::expected<loaded_document, std::string> loaded = read_document_file(doc.path);
	if (!loaded) {
		doc.persistence_error = loaded.error();
		log::println(log::level::error, log::category::general, "workspace: {}", loaded.error());
		return false;
	}
	doc.buffer = std::move(loaded->buffer);
	doc.disk_write_time = loaded->write_time;
	++doc.revision.value;
	doc.view.caret = doc.buffer.clamp(doc.view.caret);
	doc.view.anchor = doc.buffer.clamp(doc.view.anchor);
	doc.syntax = {};
	doc.persistence = document_persistence::clean;
	doc.persistence_error.clear();
	doc.highlight_dirty = true;
	doc.diag_dirty = true;
	doc.edit_clock.reset();
	return true;
}

auto gse::ide::workspace::save_dirty_documents(data& d) -> void {
	for (const id id : std::views::keys(d.documents)) {
		document& doc = d.documents.at(id);
		if (doc.persistence == document_persistence::clean) {
			continue;
		}
		if (doc.persistence == document_persistence::dirty) {
			const document_save_result result = save_document(d, id);
			if (result == document_save_result::saved || result == document_save_result::unchanged) {
				continue;
			}
		}
		preserve_document_recovery(doc, id);
	}
}

auto gse::ide::workspace::make_root(const std::filesystem::path& path, std::string name, const gse::gui::symbol_glyph glyph) -> fs_node {
	const std::filesystem::path normalized = normalized_path(path);
	return {
		.path = normalized,
		.name = std::move(name),
		.key = fs_node_key(normalized),
		.is_dir = true,
		.role = fs_node_role::browse_root,
		.glyph = glyph,
	};
}

auto gse::ide::workspace::load_children(fs_node& node) -> void {
	if (!node.is_dir || node.path.empty()) {
		node.loaded = true;
		return;
	}

	node.loaded = true;
	node.write_time = file_write_time(node.path).value_or(std::filesystem::file_time_type{});

	std::error_code ec;
	std::vector<std::pair<std::filesystem::path, bool>> entries;
	for (const auto& entry : std::filesystem::directory_iterator(node.path, std::filesystem::directory_options::skip_permission_denied, ec)) {
		std::error_code entry_ec;
		entries.emplace_back(entry.path(), entry.is_directory(entry_ec));
	}

	std::ranges::sort(entries, [](const auto& a, const auto& b) {
		if (a.second != b.second) {
			return a.second;
		}
		return a.first.filename().generic_display_string() < b.first.filename().generic_display_string();
	});

	std::vector<fs_node> previous = std::exchange(node.children, {});
	node.children.reserve(entries.size());
	for (const auto& [path, is_dir] : entries) {
		const std::filesystem::path normalized = normalized_path(path);
		const std::uint64_t key = fs_node_key(normalized);
		const auto existing = std::ranges::find_if(previous, [key, is_dir](const fs_node& n) {
			return n.key == key && n.is_dir == is_dir;
		});
		if (existing != previous.end()) {
			node.children.push_back(std::move(*existing));
			continue;
		}
		node.children.push_back({
			.path = normalized,
			.name = path.filename().generic_display_string(),
			.key = key,
			.is_dir = is_dir,
		});
	}
}

auto gse::ide::workspace::find_node(fs_node& root, const std::uint64_t key) -> fs_node* {
	if (root.key == key) {
		return &root;
	}
	for (fs_node& child : root.children) {
		if (fs_node* found = find_node(child, key)) {
			return found;
		}
	}
	return nullptr;
}

auto gse::ide::workspace::reveal_nodes(data& d, const std::filesystem::path& path) -> explorer_reveal {
	for (fs_node& root : d.fs_root.children) {
		if (!path_within(root.path, path)) {
			continue;
		}

		explorer_reveal reveal;
		fs_node* current = &root;
		while (current) {
			if (same_navigation_path(current->path, path)) {
				reveal.key = current->key;
				return reveal;
			}

			reveal.expand.push_back(current->key);
			if (!current->loaded) {
				load_children(*current);
			}

			const auto child = std::ranges::find_if(current->children, [&path](const fs_node& n) {
				return path_within(n.path, path);
			});
			current = child == current->children.end() ? nullptr : &*child;
		}
	}

	return {};
}

auto gse::ide::workspace::target(const fs_node& node) -> explorer_target {
	return {
		.path = node.path,
		.key = node.key,
		.is_dir = node.is_dir,
		.role = node.role,
	};
}

auto gse::ide::workspace::can_create_entry(const data& d, const explorer_target& target) -> bool {
	const std::filesystem::path directory = target.is_dir ? target.path : target.path.parent_path();
	return std::ranges::any_of(d.fs_root.children, [&](const fs_node& root) {
		return path_within(root.path, directory);
	});
}

auto gse::ide::workspace::can_mutate_entry(const data& d, const explorer_target& target) -> bool {
	if (target.role == fs_node_role::browse_root) {
		return false;
	}
	return std::ranges::any_of(d.fs_root.children, [&](const fs_node& root) {
		return path_strictly_within(root.path, target.path);
	});
}

auto gse::ide::workspace::create_entry(data& d, explorer_target target, const bool is_dir) -> void {
	if (!can_create_entry(d, target)) {
		d.explorer_error = "the selected location is outside the editable workspace roots";
		return;
	}
	cancel_pending_name(d);
	if (d.pending_name) {
		return;
	}

	const std::filesystem::path dir = target.is_dir ? target.path : target.path.parent_path();

	std::filesystem::path candidate = dir / (is_dir ? "New Folder" : "new_file.txt");
	for (int suffix = 2; std::filesystem::exists(candidate); ++suffix) {
		candidate = is_dir
			? dir / ("New Folder " + std::to_string(suffix))
			: dir / ("new_file_" + std::to_string(suffix) + ".txt");
	}

	std::error_code ec;
	if (is_dir) {
		std::filesystem::create_directory(candidate, ec);
	}
	else {
		std::ofstream file(candidate);
		if (!file) {
			d.explorer_error = std::format("could not create '{}'", candidate.generic_display_string());
			return;
		}
		file.close();
		if (!file) {
			d.explorer_error = std::format("could not finish creating '{}'", candidate.generic_display_string());
			return;
		}
	}
	if (ec) {
		d.explorer_error = std::format("could not create '{}': {}", candidate.generic_display_string(), ec.message());
		return;
	}

	refresh_directory(d, dir);

	d.pending_name = pending_explorer_name{
		.path = candidate,
		.parent = dir,
		.name = candidate.filename().native_encoded_string(),
		.key = fs_node_key(candidate),
		.is_dir = is_dir,
		.created = true,
		.focus_requested = true,
	};
	d.pending_name->input.caret = static_cast<int>(d.pending_name->name.size());
	d.pending_name->input.anchor = 0;
	d.explorer_selection.keys.clear();
	d.explorer_selection.keys.insert(d.pending_name->key);
	d.explorer_selection_seen.insert(d.pending_name->key);
	d.explorer_error.clear();
}

auto gse::ide::workspace::rename_entry(data& d, explorer_target target) -> void {
	if (!can_mutate_entry(d, target)) {
		d.explorer_error = "workspace roots cannot be renamed";
		return;
	}
	cancel_pending_name(d);
	if (d.pending_name) {
		return;
	}

	d.pending_name = pending_explorer_name{
		.path = target.path,
		.parent = target.path.parent_path(),
		.name = target.path.filename().native_encoded_string(),
		.key = target.key,
		.is_dir = target.is_dir,
		.focus_requested = true,
	};
	d.pending_name->input.caret = static_cast<int>(d.pending_name->name.size());
	d.pending_name->input.anchor = 0;
	d.explorer_selection.keys.clear();
	d.explorer_selection.keys.insert(target.key);
	d.explorer_selection_seen.insert(target.key);
	d.explorer_error.clear();
}

auto gse::ide::workspace::commit_pending_name(data& d) -> void {
	if (!d.pending_name) {
		return;
	}

	pending_explorer_name pending = std::move(*d.pending_name);
	if (pending.name.empty() || pending.name.find_first_of("/\\") != std::string::npos) {
		d.pending_name = std::move(pending);
		d.pending_name->focus_requested = true;
		return;
	}

	const std::filesystem::path target = pending.parent / pending.name;
	std::error_code ec;
	if (target != pending.path && std::filesystem::exists(target, ec) && !same_navigation_path(target, pending.path)) {
		d.pending_name = std::move(pending);
		d.pending_name->focus_requested = true;
		return;
	}

	std::vector<id> renamed_documents;
	const std::filesystem::path old_key = normalized_path(pending.path);

	if (target != pending.path) {
		std::filesystem::rename(pending.path, target, ec);
		if (ec) {
			d.pending_name = std::move(pending);
			d.pending_name->focus_requested = true;
			return;
		}

		const std::filesystem::path new_key = normalized_path(target);
		for (auto& [id, doc] : d.documents) {
			if (doc.path.empty()) {
				continue;
			}

			const std::optional<std::filesystem::path> next_path = remapped_path(doc.path, old_key, new_key, pending.is_dir);
			if (!next_path) {
				continue;
			}

			d.watcher.unwatch(doc.path);
			doc.path = *next_path;
			d.watcher.watch(doc.path, [&d](const std::filesystem::path& changed_path) {
				workspace::reload_document_from_disk(d, changed_path);
			});
			doc.highlightable = highlightable_path(doc.path);
			doc.tab_name.clear();
			renamed_documents.push_back(id);
		}
		for (const id id : renamed_documents) {
			document& doc = d.documents.at(id);
			doc.tab_name = unique_tab_name(d, doc.path);
		}
		remap_navigation_entries(d.back_stack, old_key, new_key, pending.is_dir);
		remap_navigation_entries(d.forward_stack, old_key, new_key, pending.is_dir);
		if (d.reveal_path) {
			if (const auto next = remapped_path(*d.reveal_path, old_key, new_key, pending.is_dir)) {
				d.reveal_path = *next;
			}
		}
		d.pending_reveal.reset();
	}

	refresh_directory(d, pending.parent);
	const std::uint64_t target_key = fs_node_key(target);
	d.explorer_selection.keys.clear();
	d.explorer_selection.keys.insert(target_key);
	d.explorer_selection_seen.erase(target_key);
	d.pending_name.reset();
	d.explorer_error.clear();
}

auto gse::ide::workspace::cancel_pending_name(data& d) -> void {
	if (!d.pending_name) {
		return;
	}

	const pending_explorer_name pending = std::move(*d.pending_name);
	if (pending.created) {
		std::error_code ec;
		std::filesystem::remove_all(pending.path, ec);
		if (ec) {
			d.pending_name = std::move(pending);
			d.pending_name->focus_requested = true;
			d.explorer_error = std::format("could not remove '{}': {}", d.pending_name->path.generic_display_string(), ec.message());
			return;
		}
	}

	refresh_directory(d, pending.parent);
	if (pending.created) {
		d.explorer_selection.keys.erase(pending.key);
	}
	else {
		d.explorer_selection.keys.clear();
		d.explorer_selection.keys.insert(pending.key);
	}
	d.explorer_selection_seen.erase(pending.key);
	d.pending_name.reset();
	d.explorer_error.clear();
}

auto gse::ide::workspace::delete_entry(data& d, explorer_target target) -> void {
	if (!can_mutate_entry(d, target)) {
		d.explorer_error = "workspace roots cannot be deleted";
		return;
	}
	const std::filesystem::path base = normalized_path(target.path);
	const std::filesystem::path parent = base.parent_path();

	std::vector<id> orphaned;
	for (const auto& [id, doc] : d.documents) {
		if (doc.path.empty()) {
			continue;
		}
		if (const std::filesystem::path rel = doc.path.lexically_relative(base); !rel.empty() && *rel.begin() != "..") {
			orphaned.push_back(id);
		}
	}
	std::error_code ec;
	std::filesystem::remove_all(target.path, ec);
	if (ec) {
		d.explorer_error = std::format("could not delete '{}': {}", target.path.generic_display_string(), ec.message());
		return;
	}
	for (const id id : orphaned) {
		discard_document(d, id);
	}
	prune_navigation_entries(d.back_stack, base);
	prune_navigation_entries(d.forward_stack, base);
	if (d.reveal_path && path_within(base, *d.reveal_path)) {
		d.reveal_path.reset();
	}
	d.pending_reveal.reset();
	refresh_directory(d, parent);
	d.explorer_error.clear();
}

auto gse::ide::workspace::request_children(data& d, const std::uint64_t key) -> void {
	if (!std::ranges::contains(d.explorer_load_requests, key)) {
		d.explorer_load_requests.push_back(key);
	}
}

auto gse::ide::workspace::update_explorer(data& d) -> void {
	std::vector<std::uint64_t> load_requests = std::exchange(d.explorer_load_requests, {});
	for (const std::uint64_t key : load_requests) {
		if (fs_node* node = find_node(d.fs_root, key); node && node->is_dir && !node->loaded) {
			load_children(*node);
		}
	}
	if (d.explorer_scan_timer.tick()) {
		sync_directory_changes(d.fs_root);
	}
	if (d.cancel_name_requested) {
		d.cancel_name_requested = false;
		d.commit_name_requested = false;
		cancel_pending_name(d);
	}
	else if (d.commit_name_requested) {
		d.commit_name_requested = false;
		commit_pending_name(d);
	}
	if (d.reveal_path) {
		d.pending_reveal = reveal_nodes(d, *d.reveal_path);
		d.reveal_path.reset();
	}
}
