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

	struct repository_status {
		std::filesystem::path root;
		std::unordered_map<id, file_status> entries;
		std::unordered_set<id> dirty_dirs;
	};

	using repository_snapshot = std::shared_ptr<const repository_status>;

	class status_map {
	public:
		auto status_of(
			const std::filesystem::path& path
		) const -> file_status;

		auto dir_has_changes(
			const std::filesystem::path& path
		) const -> bool;

		auto replace(
			repository_status status
		) -> void;

		auto retain(
			std::span<const std::filesystem::path> roots
		) -> void;

		auto repository_count() const -> std::size_t;

		auto empty() const -> bool;

	private:
		std::vector<repository_snapshot> m_repositories;
	};

	using status_snapshot = std::shared_ptr<const status_map>;

	struct status_updated {
		status_snapshot status;
		std::vector<std::filesystem::path> rootless;
	};

	struct status_failure {
		std::filesystem::path root;
		std::string message;
	};

	using repository_result = std::expected<repository_status, status_failure>;
	using repo_discovery = std::expected<std::optional<std::filesystem::path>, std::string>;

	struct status_check {
		std::atomic<bool> done = false;
		std::vector<repository_result> results;
	};

	struct status_runner {
		static auto start(
			const std::shared_ptr<status_check>& check,
			std::vector<std::filesystem::path> repo_roots
		) -> void;
	};

	auto find_repo_root(
		const std::filesystem::path& start
	) -> repo_discovery;

	auto initialize(
		const std::filesystem::path& root
	) -> std::expected<void, std::string>;

	struct init_check {
		std::atomic<bool> done = false;
		std::filesystem::path root;
		std::expected<void, std::string> result;
	};

	struct init_runner {
		static auto start(
			const std::shared_ptr<init_check>& check,
			std::filesystem::path root
		) -> void;
	};
}

namespace gse::ide::git {
	auto classify(
		char index,
		char work
	) -> file_status;

	auto mark_ancestors(
		repository_status& status,
		const std::filesystem::path& file
	) -> void;

	auto parse_status(
		std::string_view text,
		const std::filesystem::path& repo_root
	) -> std::expected<repository_status, std::string>;

	auto read_file_text(
		const std::filesystem::path& path
	) -> std::expected<std::string, std::string>;

	auto query_status(
		const std::filesystem::path& repo_root
	) -> repository_result;
}

auto gse::ide::git::classify(const char index, const char work) -> file_status {
	if (index == '?' || work == '?') {
		return file_status::untracked;
	}
	if (index == 'U' || work == 'U' || (index == 'A' && work == 'A') || (index == 'D' && work == 'D')) {
		return file_status::conflicted;
	}
	const char code = work != ' ' && work != '\0' ? work : index;
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

auto gse::ide::git::mark_ancestors(repository_status& status, const std::filesystem::path& file) -> void {
	const id root_id = generate_temp_id(status.root);
	for (std::filesystem::path dir = file; !dir.empty();) {
		const id dir_id = generate_temp_id(dir);
		status.dirty_dirs.insert(dir_id);
		if (dir_id == root_id) {
			break;
		}
		const std::filesystem::path parent = dir.parent_path();
		if (parent == dir) {
			break;
		}
		dir = parent;
	}
}

auto gse::ide::git::parse_status(const std::string_view text, const std::filesystem::path& repo_root) -> std::expected<repository_status, std::string> {
	repository_status status{
		.root = repo_root,
	};
	std::size_t i = 0;
	while (i < text.size()) {
		if (i + 3 > text.size() || text[i + 2] != ' ') {
			return std::unexpected("git status returned a malformed record header");
		}

		const char index = text[i];
		const char work = text[i + 1];
		const std::size_t path_start = i + 3;
		const std::size_t nul = text.find('\0', path_start);
		if (nul == std::string_view::npos) {
			return std::unexpected("git status returned an unterminated path");
		}

		const file_status file_state = classify(index, work);
		const std::string_view first = text.substr(path_start, nul - path_start);
		if (first.empty()) {
			return std::unexpected("git status returned an empty path");
		}
		const std::filesystem::path first_path = repo_root / std::filesystem::path(first);
		status.entries[generate_temp_id(first_path)] = file_state;
		mark_ancestors(status, first_path);
		i = nul + 1;

		if (index == 'R' || index == 'C' || work == 'R' || work == 'C') {
			const std::size_t original_nul = text.find('\0', i);
			if (original_nul == std::string_view::npos) {
				return std::unexpected("git status returned an unterminated rename source");
			}
			const std::string_view original = text.substr(i, original_nul - i);
			if (original.empty()) {
				return std::unexpected("git status returned an empty rename source");
			}
			const std::filesystem::path original_path = repo_root / std::filesystem::path(original);
			status.entries[generate_temp_id(original_path)] = file_state;
			mark_ancestors(status, original_path);
			i = original_nul + 1;
		}
	}
	return status;
}

auto gse::ide::git::read_file_text(const std::filesystem::path& path) -> std::expected<std::string, std::string> {
	std::ifstream in(path, std::ios::binary);
	if (!in) {
		return std::unexpected(std::format("could not read {}", path.generic_display_string()));
	}
	std::ostringstream stream;
	stream << in.rdbuf();
	if (in.bad()) {
		return std::unexpected(std::format("could not finish reading {}", path.generic_display_string()));
	}
	return stream.str();
}

auto gse::ide::git::query_status(const std::filesystem::path& repo_root) -> repository_result {
	const std::filesystem::path out_path = analysis::process::temporary_path("git_status", "txt");
	const auto remove_output = make_scope_exit([&out_path] {
		std::error_code ec;
		std::filesystem::remove(out_path, ec);
	});
	const analysis::process::run_outcome run = analysis::process::run_capture_stderr(
		"git --no-optional-locks status --porcelain=v1 -z -uall",
		repo_root,
		out_path
	);
	const std::expected<std::string, std::string> output = read_file_text(out_path);

	if (!run) {
		return std::unexpected(status_failure{
			.root = repo_root,
			.message = run.error() == analysis::process::run_error::timed_out
				? "git status timed out"
				: "git status could not be launched",
		});
	}
	if (*run != 0) {
		return std::unexpected(status_failure{
			.root = repo_root,
			.message = output && !output->empty()
				? std::format("git status exited with code {}: {}", *run, output->substr(0, 2000))
				: std::format("git status exited with code {}", *run),
		});
	}
	if (!output) {
		return std::unexpected(status_failure{
			.root = repo_root,
			.message = output.error(),
		});
	}
	std::expected<repository_status, std::string> parsed = parse_status(*output, repo_root);
	if (!parsed) {
		return std::unexpected(status_failure{
			.root = repo_root,
			.message = std::move(parsed.error()),
		});
	}
	return std::move(*parsed);
}

auto gse::ide::git::initialize(const std::filesystem::path& root) -> std::expected<void, std::string> {
	const std::filesystem::path out_path = analysis::process::temporary_path("git_init", "txt");
	const auto remove_output = make_scope_exit([&out_path] {
		std::error_code ec;
		std::filesystem::remove(out_path, ec);
	});

	constexpr std::array<std::string_view, 3> steps = {
		"git init",
		"git add -A",
		"git commit -m \"Initial commit\"",
	};

	for (const std::string_view step : steps) {
		const analysis::process::run_outcome run = analysis::process::run_capture_stderr(step, root, out_path);
		if (!run) {
			return std::unexpected(run.error() == analysis::process::run_error::timed_out
				? std::format("{} timed out", step)
				: std::format("{} could not be launched, is git on PATH?", step));
		}
		if (*run != 0) {
			const std::expected<std::string, std::string> output = read_file_text(out_path);
			return std::unexpected(output && !output->empty()
				? std::format("{} exited with code {}: {}", step, *run, output->substr(0, 2000))
				: std::format("{} exited with code {}", step, *run));
		}
	}
	return {};
}

auto gse::ide::git::init_runner::start(const std::shared_ptr<init_check>& check, std::filesystem::path root) -> void {
	check->root = root;
	std::thread([check, root = std::move(root)] {
		check->result = initialize(root);
		check->done.store(true, std::memory_order_release);
	}).detach();
}

auto gse::ide::git::status_map::status_of(const std::filesystem::path& path) const -> file_status {
	const id path_id = generate_temp_id(path);
	for (const repository_snapshot& repository : m_repositories) {
		if (const auto found = repository->entries.find(path_id); found != repository->entries.end()) {
			return found->second;
		}
	}
	return file_status::none;
}

auto gse::ide::git::status_map::dir_has_changes(const std::filesystem::path& path) const -> bool {
	const id path_id = generate_temp_id(path);
	return std::ranges::any_of(m_repositories, [path_id](const repository_snapshot& repository) {
		return repository->dirty_dirs.contains(path_id);
	});
}

auto gse::ide::git::status_map::replace(repository_status status) -> void {
	const id root_id = generate_temp_id(status.root);
	const auto existing = std::ranges::find_if(m_repositories, [root_id](const repository_snapshot& repository) {
		return generate_temp_id(repository->root) == root_id;
	});
	repository_snapshot replacement = std::make_shared<const repository_status>(std::move(status));
	if (existing == m_repositories.end()) {
		m_repositories.push_back(std::move(replacement));
	}
	else {
		*existing = std::move(replacement);
	}
}

auto gse::ide::git::status_map::retain(const std::span<const std::filesystem::path> roots) -> void {
	std::unordered_set<id> retained;
	retained.reserve(roots.size());
	for (const std::filesystem::path& root : roots) {
		retained.insert(generate_temp_id(root));
	}
	std::erase_if(m_repositories, [&retained](const repository_snapshot& repository) {
		return !retained.contains(generate_temp_id(repository->root));
	});
}

auto gse::ide::git::status_map::repository_count() const -> std::size_t {
	return m_repositories.size();
}

auto gse::ide::git::status_map::empty() const -> bool {
	return m_repositories.empty();
}

auto gse::ide::git::find_repo_root(const std::filesystem::path& start) -> repo_discovery {
	std::error_code ec;
	std::filesystem::path dir = std::filesystem::weakly_canonical(start, ec);
	if (ec) {
		dir = start.lexically_normal();
	}

	for (std::filesystem::path path = dir; !path.empty();) {
		std::error_code exists_ec;
		const bool marker_exists = std::filesystem::exists(path / ".git", exists_ec);
		if (exists_ec) {
			return std::unexpected(std::format("could not inspect {}: {}", path.generic_display_string(), exists_ec.message()));
		}
		if (marker_exists) {
			return std::optional<std::filesystem::path>(path);
		}
		const std::filesystem::path parent = path.parent_path();
		if (parent == path) {
			break;
		}
		path = parent;
	}
	return std::optional<std::filesystem::path>{};
}

auto gse::ide::git::status_runner::start(const std::shared_ptr<status_check>& check, std::vector<std::filesystem::path> repo_roots) -> void {
	std::thread([check, repo_roots = std::move(repo_roots)] {
		check->results.reserve(repo_roots.size());
		for (const std::filesystem::path& repo_root : repo_roots) {
			check->results.push_back(query_status(repo_root));
		}
		check->done.store(true, std::memory_order_release);
	}).detach();
}
