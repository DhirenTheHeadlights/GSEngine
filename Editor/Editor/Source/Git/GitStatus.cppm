export module gse.ide.git:git_status;

import gse;
import gse.ide.analysis;
import std;

export namespace gse::ide::git {
	struct file_status_info {
		vec4f color = { 0.96f, 0.97f, 0.99f, 1.0f };
		char code = '\0';

		constexpr operator char() const;
	};

	enum class file_status {
		none,
		modified [[= file_status_info{
			.color = { 0.86f, 0.66f, 0.32f, 1.0f },
			.code = 'M',
		}]],
		added [[= file_status_info{
			.color = { 0.46f, 0.80f, 0.48f, 1.0f },
			.code = 'A',
		}]],
		untracked [[= file_status_info{
			.color = { 0.40f, 0.72f, 0.55f, 1.0f },
			.code = '?',
		}]],
		deleted [[= file_status_info{
			.color = { 0.86f, 0.40f, 0.40f, 1.0f },
			.code = 'D',
		}]],
		renamed [[= file_status_info{
			.color = { 0.46f, 0.68f, 0.90f, 1.0f },
			.code = 'R',
		}]],
		conflicted [[= file_status_info{
			.color = { 0.92f, 0.48f, 0.30f, 1.0f },
			.code = 'u',
		}]],
	};

	struct change {
		std::filesystem::path relative;
		file_status state = file_status::none;
		int added = 0;
		int deleted = 0;
	};

	struct repository_status {
		std::filesystem::path root;
		std::string head;
		std::string branch;
		std::string upstream;
		int ahead = 0;
		int behind = 0;
		std::vector<change> changes;
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

		auto find(
			const std::filesystem::path& root
		) const -> repository_snapshot;

		auto repositories() const -> std::span<const repository_snapshot>;

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
		bool busy = false;
		std::string action_error;
	};

	auto status_color(
		file_status status
	) -> vec4f;

	struct status_failure {
		std::filesystem::path root;
		std::string message;
	};

	using repository_result = std::expected<repository_status, status_failure>;
	using repo_discovery = std::expected<std::optional<std::filesystem::path>, std::string>;

	auto query_repositories(
		std::span<const std::filesystem::path> roots
	) -> std::vector<repository_result>;

	auto find_repo_root(
		const std::filesystem::path& start
	) -> repo_discovery;

	auto git_dir_of(
		const std::filesystem::path& root
	) -> std::filesystem::path;

	struct command {
		std::filesystem::path root;
		std::vector<std::string> steps;
		std::filesystem::path scratch;
	};

	struct command_result {
		std::filesystem::path root;
		std::expected<void, std::string> outcome;
	};

	auto run_command(
		command request
	) -> command_result;

	auto init_command(
		const std::filesystem::path& root
	) -> command;

	auto initialize(
		const std::filesystem::path& root
	) -> std::expected<void, std::string>;

	struct worktree_request {
		std::filesystem::path repository;
		std::filesystem::path destination;
		std::string branch;
	};

	auto add_worktree(
		const worktree_request& request
	) -> std::expected<void, std::string>;
}

namespace gse::ide::git {
	auto classify(
		char index,
		char work
	) -> file_status;

	auto after_fields(
		std::string_view record,
		std::size_t count
	) -> std::string_view;

	auto parse_header(
		repository_status& status,
		std::string_view record
	) -> void;

	auto add_change(
		repository_status& status,
		std::string_view relative,
		file_status state
	) -> void;

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

	auto capture(
		std::string_view command_line,
		const std::filesystem::path& repo_root
	) -> std::expected<std::string, std::string>;

	auto apply_numstat(
		repository_status& status,
		std::string_view text
	) -> void;

	auto count_untracked_lines(
		repository_status& status
	) -> void;

	auto query_status(
		const std::filesystem::path& repo_root
	) -> repository_result;

	auto run_steps(
		const command& request
	) -> std::expected<void, std::string>;
}

auto gse::ide::git::status_color(const file_status status) -> vec4f {
	return annotation_from_enum<file_status_info>(status, {}).color;
}

constexpr gse::ide::git::file_status_info::operator char() const {
	return code;
}

auto gse::ide::git::classify(const char index, const char work) -> file_status {
	return enum_from_annotation<file_status_info, char>(work != '.' ? work : index, file_status::modified);
}

auto gse::ide::git::after_fields(const std::string_view record, const std::size_t count) -> std::string_view {
	std::size_t offset = 0;
	for (std::size_t field = 0; field < count; ++field) {
		const std::size_t space = record.find(' ', offset);
		if (space == std::string_view::npos) {
			return {};
		}
		offset = space + 1;
	}
	return record.substr(offset);
}

auto gse::ide::git::parse_header(repository_status& status, const std::string_view record) -> void {
	constexpr std::string_view oid = "# branch.oid ";
	constexpr std::string_view head = "# branch.head ";
	constexpr std::string_view upstream = "# branch.upstream ";
	constexpr std::string_view ab = "# branch.ab ";
	if (record.starts_with(oid)) {
		status.head = record.substr(oid.size());
	}
	else if (record.starts_with(head)) {
		status.branch = record.substr(head.size());
	}
	else if (record.starts_with(upstream)) {
		status.upstream = record.substr(upstream.size());
	}
	else if (record.starts_with(ab)) {
		const std::string_view counts = record.substr(ab.size());
		const std::size_t space = counts.find(' ');
		if (space == std::string_view::npos || space + 1 >= counts.size()) {
			return;
		}
		const std::string_view ahead = counts.substr(1, space - 1);
		const std::string_view behind = counts.substr(space + 2);
		std::from_chars(ahead.data(), ahead.data() + ahead.size(), status.ahead);
		std::from_chars(behind.data(), behind.data() + behind.size(), status.behind);
	}
}

auto gse::ide::git::add_change(repository_status& status, const std::string_view relative, const file_status state) -> void {
	std::filesystem::path relative_path(relative);
	const std::filesystem::path full = status.root / relative_path;
	status.entries[generate_temp_id(full)] = state;
	mark_ancestors(status, full);
	status.changes.push_back({
		.relative = std::move(relative_path),
		.state = state,
	});
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
		const std::size_t nul = text.find('\0', i);
		if (nul == std::string_view::npos) {
			return std::unexpected("git status returned an unterminated record");
		}
		const std::string_view record = text.substr(i, nul - i);
		i = nul + 1;
		if (record.size() < 2) {
			return std::unexpected("git status returned a malformed record");
		}

		std::string_view path;
		file_status file_state = file_status::none;
		switch (record.front()) {
			case '#':
				parse_header(status, record);
				continue;
			case '1':
				path = after_fields(record, 8);
				file_state = classify(record[2], record[3]);
				break;
			case '2':
				path = after_fields(record, 9);
				file_state = classify(record[2], record[3]);
				break;
			case 'u':
				path = after_fields(record, 10);
				file_state = classify(record.front(), '.');
				break;
			case '?':
				path = record.substr(2);
				file_state = classify(record.front(), '.');
				break;
			default:
				return std::unexpected("git status returned an unknown record type");
		}
		if (path.empty()) {
			return std::unexpected("git status returned an empty path");
		}
		add_change(status, path, file_state);

		if (record.front() == '2') {
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

auto gse::ide::git::capture(const std::string_view command_line, const std::filesystem::path& repo_root) -> std::expected<std::string, std::string> {
	const std::filesystem::path out_path = analysis::process::temporary_path("git_capture", "txt");
	const auto _ = make_scope_exit([&out_path] {
		std::error_code ec;
		std::filesystem::remove(out_path, ec);
	});
	const analysis::process::run_outcome run = analysis::process::run_capture_stderr(command_line, repo_root, out_path);
	std::expected<std::string, std::string> output = read_file_text(out_path);

	if (!run) {
		return std::unexpected(run.error() == analysis::process::run_error::timed_out
			? std::format("{} timed out", command_line)
			: std::format("{} could not be launched", command_line));
	}
	if (*run != 0) {
		return std::unexpected(output && !output->empty()
			? std::format("{} exited with code {}: {}", command_line, *run, output->substr(0, 2000))
			: std::format("{} exited with code {}", command_line, *run));
	}
	return output;
}

auto gse::ide::git::apply_numstat(repository_status& status, const std::string_view text) -> void {
	std::unordered_map<id, change*> by_path;
	for (change& entry : status.changes) {
		by_path[generate_temp_id(status.root / entry.relative)] = &entry;
	}

	std::size_t i = 0;
	while (i < text.size()) {
		const std::size_t nul = text.find('\0', i);
		if (nul == std::string_view::npos) {
			return;
		}
		const std::string_view record = text.substr(i, nul - i);
		i = nul + 1;

		const std::size_t first_tab = record.find('\t');
		const std::size_t second_tab = first_tab == std::string_view::npos ? first_tab : record.find('\t', first_tab + 1);
		if (second_tab == std::string_view::npos) {
			continue;
		}
		const std::string_view added = record.substr(0, first_tab);
		const std::string_view deleted = record.substr(first_tab + 1, second_tab - first_tab - 1);
		std::string_view path = record.substr(second_tab + 1);
		if (path.empty()) {
			const std::size_t old_nul = text.find('\0', i);
			if (old_nul == std::string_view::npos) {
				return;
			}
			const std::size_t new_nul = text.find('\0', old_nul + 1);
			if (new_nul == std::string_view::npos) {
				return;
			}
			path = text.substr(old_nul + 1, new_nul - old_nul - 1);
			i = new_nul + 1;
		}

		const auto found = by_path.find(generate_temp_id(status.root / std::filesystem::path(path)));
		if (found == by_path.end()) {
			continue;
		}
		std::from_chars(added.data(), added.data() + added.size(), found->second->added);
		std::from_chars(deleted.data(), deleted.data() + deleted.size(), found->second->deleted);
	}
}

auto gse::ide::git::count_untracked_lines(repository_status& status) -> void {
	for (change& entry : status.changes) {
		if (entry.state != file_status::untracked) {
			continue;
		}
		const std::expected<std::string, std::string> text = read_file_text(status.root / entry.relative);
		if (!text) {
			continue;
		}
		entry.added = static_cast<int>(std::ranges::count(*text, '\n'));
		if (!text->empty() && text->back() != '\n') {
			++entry.added;
		}
	}
}

auto gse::ide::git::query_status(const std::filesystem::path& repo_root) -> repository_result {
	const std::expected<std::string, std::string> output = capture("git --no-optional-locks status --porcelain=v2 --branch -z -uall", repo_root);
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
	if (!parsed->changes.empty()) {
		const std::expected<std::string, std::string> numstat = capture("git --no-optional-locks diff HEAD --numstat -z", repo_root);
		if (numstat) {
			apply_numstat(*parsed, *numstat);
		}
		count_untracked_lines(*parsed);
	}
	return std::move(*parsed);
}

auto gse::ide::git::run_steps(const command& request) -> std::expected<void, std::string> {
	const std::filesystem::path out_path = analysis::process::temporary_path("git_command", "txt");
	const auto _ = make_scope_exit([&request, &out_path] {
		std::error_code ec;
		std::filesystem::remove(out_path, ec);
		if (!request.scratch.empty()) {
			std::filesystem::remove(request.scratch, ec);
		}
	});

	for (const std::string& step : request.steps) {
		const analysis::process::run_outcome run = analysis::process::run_capture_stderr(step, request.root, out_path);
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

auto gse::ide::git::init_command(const std::filesystem::path& root) -> command {
	return {
		.root = root,
		.steps = { "git init", "git add -A", "git commit -m \"Initial commit\"" },
	};
}

auto gse::ide::git::initialize(const std::filesystem::path& root) -> std::expected<void, std::string> {
	return run_steps(init_command(root));
}

auto gse::ide::git::add_worktree(const worktree_request& request) -> std::expected<void, std::string> {
	return run_steps({
		.root = request.repository,
		.steps = { std::format("git worktree add \"{}\" -b \"{}\"", request.destination.generic_display_string(), request.branch) },
	});
}

auto gse::ide::git::run_command(command request) -> command_result {
	return {
		.root = request.root,
		.outcome = run_steps(request),
	};
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

auto gse::ide::git::status_map::find(const std::filesystem::path& root) const -> repository_snapshot {
	const id root_id = generate_temp_id(root);
	const auto found = std::ranges::find_if(m_repositories, [root_id](const repository_snapshot& repository) {
		return generate_temp_id(repository->root) == root_id;
	});
	return found == m_repositories.end() ? repository_snapshot{} : *found;
}

auto gse::ide::git::status_map::repositories() const -> std::span<const repository_snapshot> {
	return m_repositories;
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

auto gse::ide::git::git_dir_of(const std::filesystem::path& root) -> std::filesystem::path {
	const std::filesystem::path marker = root / ".git";
	std::error_code ec;
	if (!std::filesystem::is_regular_file(marker, ec)) {
		return marker;
	}
	const std::expected<std::string, std::string> text = read_file_text(marker);
	constexpr std::string_view prefix = "gitdir: ";
	if (!text || !text->starts_with(prefix)) {
		return marker;
	}
	std::string_view value = std::string_view(*text).substr(prefix.size());
	while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) {
		value.remove_suffix(1);
	}
	const std::filesystem::path dir(value);
	return (dir.is_absolute() ? dir : root / dir).lexically_normal();
}

auto gse::ide::git::query_repositories(const std::span<const std::filesystem::path> roots) -> std::vector<repository_result> {
	std::vector<repository_result> results;
	results.reserve(roots.size());
	for (const std::filesystem::path& root : roots) {
		results.push_back(query_status(root));
	}
	return results;
}