export module gse.ide.git:git_status;

import std;
import gse;
import gse.ide.analysis;

export namespace gse::ide::git {
	enum class file_status {
		none,
		modified,
		added,
		untracked,
		deleted,
		renamed,
		conflicted,
	};

	struct status_map {
		std::unordered_map<gse::id, file_status> entries;
		std::unordered_set<gse::id> dirty_dirs;

		auto status_of(
			const std::filesystem::path& path
		) const -> file_status;

		auto dir_has_changes(
			const std::filesystem::path& path
		) const -> bool;
	};

	using status_snapshot = std::shared_ptr<const status_map>;

	struct status_updated {
		status_snapshot status;
	};

	struct status_check {
		std::atomic<bool> done = false;
		status_snapshot result;
	};

	struct status_runner {
		static auto start(
			const std::shared_ptr<status_check>& check,
			const std::filesystem::path& repo_root
		) -> void;
	};

	auto find_repo_root(
		const std::filesystem::path& start
	) -> std::filesystem::path;
}

namespace gse::ide::git {
	auto classify(const char index, const char work) -> file_status {
		if (index == '?' || work == '?') {
			return file_status::untracked;
		}
		if (index == 'U' || work == 'U' || (index == 'A' && work == 'A') || (index == 'D' && work == 'D')) {
			return file_status::conflicted;
		}
		const char code = (work != ' ' && work != '\0') ? work : index;
		switch (code) {
			case 'M':
			case 'T':
				return file_status::modified;
			case 'A':
				return file_status::added;
			case 'D':
				return file_status::deleted;
			case 'R':
			case 'C':
				return file_status::renamed;
			default:
				return file_status::modified;
		}
	}

	auto mark_ancestors(status_map& map, const std::filesystem::path& repo_root, const std::filesystem::path& file) -> void {
		const gse::id root_id = gse::generate_temp_id(repo_root);
		for (std::filesystem::path dir = file.parent_path(); !dir.empty();) {
			const gse::id id = gse::generate_temp_id(dir);
			map.dirty_dirs.insert(id);
			if (id == root_id) {
				break;
			}
			const std::filesystem::path parent = dir.parent_path();
			if (parent == dir) {
				break;
			}
			dir = parent;
		}
	}

	auto parse_status(const std::string& text, const std::filesystem::path& repo_root) -> status_map {
		status_map map;
		std::size_t i = 0;
		while (i + 3 <= text.size()) {
			const char index = text[i];
			const char work = text[i + 1];
			if (text[i + 2] != ' ') {
				const std::size_t skip = text.find('\0', i);
				if (skip == std::string::npos) {
					break;
				}
				i = skip + 1;
				continue;
			}

			const std::size_t path_start = i + 3;
			const std::size_t nul = text.find('\0', path_start);
			if (nul == std::string::npos) {
				break;
			}

			const file_status status = classify(index, work);
			if (const std::string first = text.substr(path_start, nul - path_start); !first.empty()) {
				const std::filesystem::path abs = repo_root / std::filesystem::path(first);
				map.entries[gse::generate_temp_id(abs)] = status;
				mark_ancestors(map, repo_root, abs);
			}
			i = nul + 1;

			if (index == 'R' || index == 'C' || work == 'R' || work == 'C') {
				const std::size_t orig_nul = text.find('\0', i);
				if (orig_nul == std::string::npos) {
					break;
				}
				if (const std::string second = text.substr(i, orig_nul - i); !second.empty()) {
					const std::filesystem::path abs = repo_root / std::filesystem::path(second);
					map.entries[gse::generate_temp_id(abs)] = status;
					mark_ancestors(map, repo_root, abs);
				}
				i = orig_nul + 1;
			}
		}
		return map;
	}

	auto read_file_text(const std::filesystem::path& path) -> std::string {
		std::ifstream in(path, std::ios::binary);
		if (!in) {
			return {};
		}
		std::ostringstream stream;
		stream << in.rdbuf();
		return stream.str();
	}
}

auto gse::ide::git::status_map::status_of(const std::filesystem::path& path) const -> file_status {
	const auto it = entries.find(gse::generate_temp_id(path));
	return it == entries.end() ? file_status::none : it->second;
}

auto gse::ide::git::status_map::dir_has_changes(const std::filesystem::path& path) const -> bool {
	return dirty_dirs.contains(gse::generate_temp_id(path));
}

auto gse::ide::git::find_repo_root(const std::filesystem::path& start) -> std::filesystem::path {
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

auto gse::ide::git::status_runner::start(const std::shared_ptr<status_check>& check, const std::filesystem::path& repo_root) -> void {
	std::thread([check, repo_root] {
		const std::filesystem::path out_path = std::filesystem::temp_directory_path() / "gseditor_git_status.txt";
		const std::string working_dir = repo_root.native_encoded_string();
		const std::string out = out_path.native_encoded_string();

		const analysis::process::run_result run = analysis::process::run_capture_stderr(
			"git --no-optional-locks status --porcelain=v1 -z -uall",
			working_dir.c_str(),
			out.c_str()
		);

		if (run.launched && !run.timed_out && run.exit_code == 0) {
			check->result = std::make_shared<const status_map>(parse_status(read_file_text(out_path), repo_root));
		}
		else {
			check->result = std::make_shared<const status_map>();
		}

		std::error_code ec;
		std::filesystem::remove(out_path, ec);
		check->done.store(true, std::memory_order_release);
	}).detach();
}
