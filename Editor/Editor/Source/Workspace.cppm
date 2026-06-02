export module ide:workspace;

import std;
import gse;

export namespace ide {
	struct document {
		std::filesystem::path path;
		std::string tab_name;
		gse::gui::text_buffer buffer;
		gse::gui::text_area_state view;
		bool dirty = false;
	};

	struct fs_node {
		std::filesystem::path path;
		std::string name;
		std::uint64_t key = 0;
		bool is_dir = false;
		mutable bool loaded = false;
		mutable std::vector<fs_node> children;
	};

	struct workspace {
		struct data {
			std::filesystem::path root;
			std::unordered_map<std::uint32_t, document> documents;
			std::uint32_t next_document_id = 1;
			std::uint32_t active_document_id = 0;
			std::uint32_t primary_document_id = 0;

			fs_node fs_root;
			gse::gui::draw::tree_selection explorer_selection;
			std::uint64_t last_opened_key = 0;
		};

		static auto find_repo_root(
			const std::filesystem::path& start
		) -> std::filesystem::path;

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

		static auto save_document(
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
	};
}

auto ide::workspace::find_repo_root(const std::filesystem::path& start) -> std::filesystem::path {
	std::error_code ec;
	std::filesystem::path dir = std::filesystem::weakly_canonical(start, ec);
	if (ec) {
		dir = start;
	}

	for (std::filesystem::path p = dir; !p.empty();) {
		std::error_code exists_ec;
		if (std::filesystem::exists(p / ".git", exists_ec)) {
			return p;
		}
		const std::filesystem::path parent = p.parent_path();
		if (parent == p) {
			break;
		}
		p = parent;
	}

	return dir;
}

auto ide::workspace::unique_tab_name(const data& d, const std::filesystem::path& path) -> std::string {
	const std::string base = path.empty() ? "untitled" : path.filename().string();

	auto taken = [&](const std::string& candidate) {
		return std::ranges::any_of(d.documents, [&](const auto& entry) {
			return entry.second.tab_name == candidate;
		});
	};

	if (!taken(base)) {
		return base;
	}

	if (path.has_parent_path()) {
		if (const std::string parent = path.parent_path().filename().string(); !parent.empty()) {
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

auto ide::workspace::open_scratch(data& d) -> std::uint32_t {
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

auto ide::workspace::open_file(data& d, const std::filesystem::path& path) -> std::uint32_t {
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
	d.documents.emplace(id, std::move(doc));

	if (d.primary_document_id == 0) {
		d.primary_document_id = id;
	}
	d.active_document_id = id;
	return id;
}

auto ide::workspace::save_document(data& d, const std::uint32_t document_id) -> void {
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

auto ide::workspace::load_children(const fs_node& node) -> void {
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
		return a.first.filename().string() < b.first.filename().string();
	});

	for (const auto& [path, is_dir] : entries) {
		fs_node child;
		child.path = path;
		child.name = path.filename().string();
		child.key = std::hash<std::string>{}(path.string());
		child.is_dir = is_dir;
		node.children.push_back(std::move(child));
	}
}

auto ide::workspace::find_node(const fs_node& root, const std::uint64_t key) -> const fs_node* {
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
