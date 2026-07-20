export module gse.ide.workspace:workspace;

import std;
import gse;

import gse.ide.highlight;
import gse.ide.diagnostic;
import gse.ide.docs;

export namespace gse::ide {
	struct document {
		std::filesystem::path path;
		std::string tab_name;
		gse::gui::text_buffer buffer;
		gse::gui::text_area_state view;
		bool dirty = false;
		syntax_producer::data syntax;
		bool highlight_dirty = true;
		bool highlightable = true;
		std::vector<diagnostic> diagnostics;
		std::vector<diagnostic> lint;
		bool analysis_failed = false;
		bool analysis_crashed = false;
		gse::time analysis_duration{};
		bool diag_dirty = true;
		gse::time last_edit{};
		std::optional<std::uint32_t> pending_center_line;
	};

	struct hover_state {
		std::uint32_t line = 0;
		std::uint32_t column = 0;
		std::string ident;
		gse::time since{};
		bool resolved = false;
		bool has_card = false;
		std::string title;
		std::string kind;
		std::string body;
		std::string url;
		bool from_cppref = false;
		bool body_is_code = false;
		float scroll = 0.f;
		std::vector<gse::gui::text_span> code_spans;
		gse::vec4f kind_color;
		gse::vec2f anchor;
		gse::rect_t<gse::vec2f> panel;
		gse::rect_t<gse::vec2f> code_rect;
	};

	struct fs_node {
		std::filesystem::path path;
		std::string name;
		std::uint64_t key = 0;
		bool is_dir = false;
		mutable bool loaded = false;
		mutable std::vector<fs_node> children;
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

	struct workspace {
		struct data {
			std::filesystem::path root;
			std::unordered_map<std::uint32_t, document> documents;
			std::vector<std::uint32_t> tab_order;
			std::vector<navigation_entry> back_stack;
			std::vector<navigation_entry> forward_stack;
			gse::gui::tab_strip_state tab_strip;
			std::uint32_t next_document_id = 1;
			std::uint32_t active_document_id = 0;
			std::uint32_t primary_document_id = 0;
			bool show_game_graph = false;
			gse::file_watcher watcher;

			fs_node fs_root;
			gse::gui::draw::tree_selection explorer_selection;
			std::uint64_t last_opened_key = 0;
			std::optional<pending_explorer_name> pending_name;

			std::optional<std::uint32_t> diagnostics_pending;

			std::vector<hover_state> hover_stack;
			docs::cppref_index cppref;
		};

		static auto unique_tab_name(
			const data& d,
			const std::filesystem::path& path
		) -> std::string;

		static auto open_scratch(
			data& d
		) -> std::uint32_t;

		static auto open_file(
			data& d,
			const std::filesystem::path& path
		) -> std::uint32_t;

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
			std::uint32_t document_id
		) -> void;

		static auto close_document(
			data& d,
			std::uint32_t document_id
		) -> void;

		static auto load_children(
			const fs_node& node
		) -> void;

		static auto find_node(
			const fs_node& root,
			std::uint64_t key
		) -> const fs_node*;

		static auto find_parent(
			const fs_node& root,
			std::uint64_t key
		) -> const fs_node*;

		static auto create_entry(
			data& d,
			const fs_node& target,
			bool is_dir
		) -> void;

		static auto rename_entry(
			data& d,
			const fs_node& target
		) -> void;

		static auto commit_pending_name(
			data& d
		) -> void;

		static auto cancel_pending_name(
			data& d
		) -> void;

		static auto delete_entry(
			data& d,
			const fs_node& target
		) -> void;

		static auto reload_document_from_disk(
			data& d,
			const std::filesystem::path& path
		) -> void;
	};
}

namespace gse::ide {
	constexpr std::size_t max_navigation_history = 256;

	auto fs_node_key(
		const std::filesystem::path& path
	) -> std::uint64_t {
		return std::hash<std::string>{}(path.native_encoded_string());
	}

	auto refresh_directory(
		workspace::data& d,
		const std::filesystem::path& path
	) -> void {
		const std::uint64_t key = fs_node_key(path);
		if (const fs_node* node = workspace::find_node(d.fs_root, key)) {
			workspace::load_children(*node);
		}
		else if (path == d.fs_root.path) {
			workspace::load_children(d.fs_root);
		}
	}

	auto normalized_navigation_entry(
		const std::filesystem::path& path,
		const std::uint32_t line,
		const std::uint32_t column
	) -> navigation_entry {
		std::error_code ec;
		const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, ec);
		return {
			.path = ec ? path : canonical,
			.line = line,
			.column = column
		};
	}

	auto same_navigation_path(
		const std::filesystem::path& lhs,
		const std::filesystem::path& rhs
	) -> bool {
		std::error_code ec;
		if (std::filesystem::equivalent(lhs, rhs, ec)) {
			return true;
		}

		std::error_code lhs_ec;
		std::error_code rhs_ec;
		const std::filesystem::path lhs_canonical = std::filesystem::weakly_canonical(lhs, lhs_ec);
		const std::filesystem::path rhs_canonical = std::filesystem::weakly_canonical(rhs, rhs_ec);
		const std::filesystem::path lhs_key = (lhs_ec ? lhs : lhs_canonical).lexically_normal();
		const std::filesystem::path rhs_key = (rhs_ec ? rhs : rhs_canonical).lexically_normal();
		return lhs_key == rhs_key;
	}

	auto same_navigation_entry(
		const navigation_entry& lhs,
		const navigation_entry& rhs
	) -> bool {
		return lhs.line == rhs.line &&
			lhs.column == rhs.column &&
			same_navigation_path(lhs.path, rhs.path);
	}

	auto current_navigation_entry(
		const workspace::data& d
	) -> std::optional<navigation_entry> {
		const auto it = d.documents.find(d.active_document_id);
		if (it == d.documents.end() || it->second.path.empty()) {
			return std::nullopt;
		}

		const document& doc = it->second;
		const gse::gui::buffer_position caret = doc.buffer.clamp(doc.view.caret);
		return navigation_entry{
			.path = doc.path,
			.line = caret.line,
			.column = caret.column
		};
	}

	auto push_navigation_entry(
		std::vector<navigation_entry>& stack,
		const navigation_entry& entry
	) -> void {
		if (!stack.empty() && same_navigation_entry(stack.back(), entry)) {
			return;
		}
		stack.push_back(entry);
		if (stack.size() > max_navigation_history) {
			stack.erase(stack.begin());
		}
	}

	auto jump_to_without_history(
		workspace::data& d,
		const navigation_entry& target
	) -> bool {
		const std::uint32_t id = workspace::open_file(d, target.path);
		const auto it = d.documents.find(id);
		if (it == d.documents.end()) {
			return false;
		}
		document& doc = it->second;
		doc.view.caret = doc.buffer.clamp({ .line = target.line, .column = target.column });
		doc.view.anchor = doc.view.caret;
		doc.pending_center_line = doc.view.caret.line;
		d.active_document_id = id;
		return true;
	}
}

auto gse::ide::workspace::unique_tab_name(const data& d, const std::filesystem::path& path) -> std::string {
	const std::string base = path.empty() ? "untitled" : path.filename().display_string();

	auto taken = [&](const std::string& candidate) {
		return std::ranges::any_of(d.documents, [&](const auto& entry) {
			return entry.second.tab_name == candidate;
		});
	};

	if (!taken(base)) {
		return base;
	}

	if (path.has_parent_path()) {
		if (const std::string parent = path.parent_path().filename().display_string(); !parent.empty()) {
			if (std::string scoped = base + " (" + parent + ")"; !taken(scoped)) {
				return scoped;
			}
		}
	}

	for (int suffix = 2;; ++suffix) {
		if (std::string candidate = base + " (" + std::to_string(suffix) + ")"; !taken(candidate)) {
			return candidate;
		}
	}
}

auto gse::ide::workspace::open_scratch(data& d) -> std::uint32_t {
	const std::uint32_t id = d.next_document_id++;
	document doc;
	doc.tab_name = unique_tab_name(d, {});
	doc.buffer.lines = { "" };
	d.documents.emplace(id, std::move(doc));

	if (d.primary_document_id == 0) {
		d.primary_document_id = id;
	}
	d.active_document_id = id;
	return id;
}

auto gse::ide::workspace::open_file(data& d, const std::filesystem::path& path) -> std::uint32_t {
	std::error_code ec;
	const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, ec);
	const std::filesystem::path key = ec ? path : canonical;

	for (const auto& [id, doc] : d.documents) {
		if (doc.path == key) {
			d.active_document_id = id;
			return id;
		}
	}

	const std::uint32_t id = d.next_document_id++;
	document doc;
	doc.path = key;
	doc.tab_name = unique_tab_name(d, key);
	doc.buffer = gse::gui::text_buffer::from_file(key);

	std::string ext = key.extension().native_encoded_string();
	std::ranges::transform(ext, ext.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
	doc.highlightable =
		ext == ".cpp" || ext == ".cppm" || ext == ".cc" || ext == ".cxx" ||
		ext == ".ixx" || ext == ".c" || ext == ".h" || ext == ".hpp" ||
		ext == ".hh" || ext == ".hxx" || ext == ".inl";

	d.documents.emplace(id, std::move(doc));
	d.watcher.watch(key, [&d](const std::filesystem::path& changed_path) {
		workspace::reload_document_from_disk(d, changed_path);
	});

	if (d.primary_document_id == 0) {
		d.primary_document_id = id;
	}
	d.active_document_id = id;
	return id;
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

auto gse::ide::workspace::save_document(data& d, const std::uint32_t document_id) -> void {
	const auto it = d.documents.find(document_id);
	if (it == d.documents.end() || it->second.path.empty()) {
		return;
	}

	document& doc = it->second;
	std::ofstream out(doc.path, std::ios::binary | std::ios::trunc);
	if (!out) {
		return;
	}

	for (std::size_t i = 0; i < doc.buffer.lines.size(); ++i) {
		out << doc.buffer.lines[i];
		if (i + 1 < doc.buffer.lines.size()) {
			out << '\n';
		}
	}
	doc.dirty = false;
}

auto gse::ide::workspace::reload_document_from_disk(data& d, const std::filesystem::path& path) -> void {
	for (auto& [id, doc] : d.documents) {
		if (doc.path.empty() || doc.dirty || !same_navigation_path(doc.path, path)) {
			continue;
		}

		const gse::gui::buffer_position caret = doc.view.caret;
		const gse::gui::buffer_position anchor = doc.view.anchor;
		doc.buffer = gse::gui::text_buffer::from_file(doc.path);
		doc.view.caret = doc.buffer.clamp(caret);
		doc.view.anchor = doc.buffer.clamp(anchor);
		doc.syntax = {};
		doc.highlight_dirty = true;
		doc.diag_dirty = true;
		doc.last_edit = {};
		return;
	}
}

auto gse::ide::workspace::close_document(data& d, const std::uint32_t document_id) -> void {
	if (const auto it = d.documents.find(document_id); it != d.documents.end() && !it->second.path.empty()) {
		d.watcher.unwatch(it->second.path);
	}
	d.documents.erase(document_id);
	if (d.active_document_id == document_id) {
		d.active_document_id = d.documents.empty() ? 0 : d.documents.begin()->first;
	}
	if (d.primary_document_id == document_id) {
		d.primary_document_id = 0;
	}
}

auto gse::ide::workspace::load_children(const fs_node& node) -> void {
	node.loaded = true;
	node.children.clear();
	if (!node.is_dir) {
		return;
	}

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
		return a.first.filename().display_string() < b.first.filename().display_string();
	});

	for (const auto& [path, is_dir] : entries) {
		fs_node child;
		child.path = path;
		child.name = path.filename().display_string();
		child.key = fs_node_key(path);
		child.is_dir = is_dir;
		node.children.push_back(std::move(child));
	}
}

auto gse::ide::workspace::find_node(const fs_node& root, const std::uint64_t key) -> const fs_node* {
	if (root.key == key) {
		return &root;
	}
	for (const fs_node& child : root.children) {
		if (const fs_node* found = find_node(child, key)) {
			return found;
		}
	}
	return nullptr;
}

auto gse::ide::workspace::find_parent(const fs_node& root, const std::uint64_t key) -> const fs_node* {
	for (const fs_node& child : root.children) {
		if (child.key == key) {
			return &root;
		}
		if (const fs_node* found = find_parent(child, key)) {
			return found;
		}
	}
	return nullptr;
}

auto gse::ide::workspace::create_entry(data& d, const fs_node& target, const bool is_dir) -> void {
	cancel_pending_name(d);

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
			return;
		}
	}
	if (ec) {
		return;
	}

	if (const fs_node* dir_node = target.is_dir ? &target : find_parent(d.fs_root, target.key)) {
		load_children(*dir_node);
	}

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
	d.last_opened_key = d.pending_name->key;
}

auto gse::ide::workspace::rename_entry(data& d, const fs_node& target) -> void {
	cancel_pending_name(d);

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
	d.last_opened_key = target.key;
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
	if (target != pending.path && std::filesystem::exists(target, ec)) {
		d.pending_name = std::move(pending);
		d.pending_name->focus_requested = true;
		return;
	}

	std::vector<std::uint32_t> renamed_documents;
	const std::filesystem::path old_base = std::filesystem::weakly_canonical(pending.path, ec);
	const std::filesystem::path old_key = ec ? pending.path.lexically_normal() : old_base.lexically_normal();

	if (target != pending.path) {
		std::filesystem::rename(pending.path, target, ec);
		if (ec) {
			d.pending_name = std::move(pending);
			d.pending_name->focus_requested = true;
			return;
		}

		const std::filesystem::path new_base = std::filesystem::weakly_canonical(target, ec);
		const std::filesystem::path new_key = ec ? target.lexically_normal() : new_base.lexically_normal();
		for (auto& [id, doc] : d.documents) {
			if (doc.path.empty()) {
				continue;
			}

			std::filesystem::path next_path;
			const std::filesystem::path doc_key = doc.path.lexically_normal();
			if (doc_key == old_key) {
				next_path = new_key;
			}
			else if (pending.is_dir) {
				const std::filesystem::path rel = doc_key.lexically_relative(old_key);
				if (!rel.empty() && *rel.begin() != "..") {
					next_path = new_key / rel;
				}
			}

			if (next_path.empty()) {
				continue;
			}

			d.watcher.unwatch(doc.path);
			doc.path = next_path;
			d.watcher.watch(doc.path, [&d](const std::filesystem::path& changed_path) {
				workspace::reload_document_from_disk(d, changed_path);
			});
			std::string ext = doc.path.extension().native_encoded_string();
			std::ranges::transform(ext, ext.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
			doc.highlightable =
				ext == ".cpp" || ext == ".cppm" || ext == ".cc" || ext == ".cxx" ||
				ext == ".ixx" || ext == ".c" || ext == ".h" || ext == ".hpp" ||
				ext == ".hh" || ext == ".hxx" || ext == ".inl";
			doc.tab_name.clear();
			renamed_documents.push_back(id);
		}
		for (const std::uint32_t id : renamed_documents) {
			document& doc = d.documents.at(id);
			doc.tab_name = unique_tab_name(d, doc.path);
		}
	}

	refresh_directory(d, pending.parent);
	d.explorer_selection.keys.clear();
	d.explorer_selection.keys.insert(fs_node_key(target));
	d.last_opened_key = 0;
	d.pending_name.reset();
}

auto gse::ide::workspace::cancel_pending_name(data& d) -> void {
	if (!d.pending_name) {
		return;
	}

	const pending_explorer_name pending = std::move(*d.pending_name);
	if (pending.created) {
		std::error_code ec;
		std::filesystem::remove_all(pending.path, ec);
	}

	refresh_directory(d, pending.parent);
	if (pending.created) {
		d.explorer_selection.keys.erase(pending.key);
	}
	else {
		d.explorer_selection.keys.clear();
		d.explorer_selection.keys.insert(pending.key);
	}
	if (d.last_opened_key == pending.key) {
		d.last_opened_key = 0;
	}
	d.pending_name.reset();
}

auto gse::ide::workspace::delete_entry(data& d, const fs_node& target) -> void {
	std::error_code canon_ec;
	const std::filesystem::path canonical = std::filesystem::weakly_canonical(target.path, canon_ec);
	const std::filesystem::path base = canon_ec ? target.path : canonical;

	std::vector<std::uint32_t> orphaned;
	for (const auto& [id, doc] : d.documents) {
		if (doc.path.empty()) {
			continue;
		}
		if (const std::filesystem::path rel = doc.path.lexically_relative(base); !rel.empty() && *rel.begin() != "..") {
			orphaned.push_back(id);
		}
	}
	for (const std::uint32_t id : orphaned) {
		close_document(d, id);
	}

	std::error_code ec;
	std::filesystem::remove_all(target.path, ec);
	if (const fs_node* parent = find_parent(d.fs_root, target.key)) {
		load_children(*parent);
	}
}
