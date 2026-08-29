export module gse.fs:file_watcher;

import std;

import gse.math;
import gse.time;

export namespace gse {
	class file_watcher {
	public:
		using callback = std::function<void(const std::filesystem::path&)>;
		using directory_filter = std::function<bool(const std::filesystem::path&)>;

		auto watch(
			const std::filesystem::path& path,
			callback on_change
		) -> void;

		auto watch_directory(
			const std::filesystem::path& directory,
			callback on_change,
			std::span<const std::string> extensions = {},
			bool recursive = true,
			directory_filter recurse_into = {}
		) -> void;

		auto unwatch(
			const std::filesystem::path& path
		) -> void;

		auto poll() -> std::size_t;
		auto poll_now() -> std::size_t;

		auto clear() -> void;

	private:
		struct watch_entry {
			std::filesystem::path path;
			std::filesystem::file_time_type last_modified;
			callback on_change;
			bool is_directory;
			bool recursive;
			std::vector<std::string> extensions;
			directory_filter recurse_into;
		};

		std::vector<watch_entry> m_watches;
		std::unordered_map<std::filesystem::path, std::filesystem::file_time_type> m_directory_files;
		mutable std::mutex m_mutex;
		interval_timer<> m_poll_timer{ milliseconds(500.f) };

		static auto matches_extensions(
			const std::filesystem::path& path,
			std::span<const std::string> extensions
		) -> bool;

		static auto scan_directory(
			const std::filesystem::path& directory,
			std::span<const std::string> extensions,
			bool recursive,
			const directory_filter& recurse_into
		) -> std::
			unordered_map<std::filesystem::path, std::filesystem::file_time_type>;

		auto poll_locked() -> std::size_t;
	};
}

auto gse::file_watcher::watch(const std::filesystem::path& path, callback on_change) -> void {
	std::lock_guard lock(m_mutex);

	if (!std::filesystem::exists(path)) {
		return;
	}

	m_watches.push_back({
		.path = path,
		.last_modified = std::filesystem::last_write_time(path),
		.on_change = std::move(on_change),
		.is_directory = false,
		.recursive = false,
		.extensions = {},
		.recurse_into = {},
	});
}

auto gse::file_watcher::watch_directory(const std::filesystem::path& directory, callback on_change, std::span<const std::string> extensions, const bool recursive, directory_filter recurse_into) -> void {
	std::lock_guard lock(m_mutex);

	if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
		return;
	}

	std::vector<std::string> ext_vec(extensions.begin(), extensions.end());

	for (const auto& [file_path, mod_time] : scan_directory(directory, extensions, recursive, recurse_into)) {
		m_directory_files[file_path] = mod_time;
	}

	m_watches.push_back({
		.path = directory,
		.last_modified = {},
		.on_change = std::move(on_change),
		.is_directory = true,
		.recursive = recursive,
		.extensions = std::move(ext_vec),
		.recurse_into = std::move(recurse_into),
	});
}

auto gse::file_watcher::unwatch(const std::filesystem::path& path) -> void {
	std::lock_guard lock(m_mutex);

	std::erase_if(
		m_watches,
		[&](const watch_entry& entry) {
			return entry.path == path;
		}
	);

	if (std::filesystem::is_directory(path)) {
		std::erase_if(
			m_directory_files,
			[&](const auto& pair) {
				auto [file_path, _] = pair;
				return file_path.native_encoded_string().starts_with(path.native_encoded_string());
			}
		);
	}
}

auto gse::file_watcher::poll() -> std::size_t {
	std::lock_guard lock(m_mutex);

	if (!m_poll_timer.tick()) {
		return 0;
	}
	return poll_locked();
}

auto gse::file_watcher::poll_now() -> std::size_t {
	std::lock_guard lock(m_mutex);
	m_poll_timer.reset();
	return poll_locked();
}

auto gse::file_watcher::poll_locked() -> std::size_t {
	std::size_t changes = 0;

	for (auto& [path, last_modified, on_change, is_directory, recursive, extensions, recurse_into] : m_watches) {
		if (is_directory) {
			const auto current_files = scan_directory(path, extensions, recursive, recurse_into);
			for (const auto& [file_path, mod_time] : current_files) {
				if (auto it = m_directory_files.find(file_path); it == m_directory_files.end()) {
					m_directory_files[file_path] = mod_time;
					on_change(file_path);
					++changes;
				}
				else if (it->second != mod_time) {
					it->second = mod_time;
					on_change(file_path);
					++changes;
				}
			}
			std::vector<std::filesystem::path> removed;
			for (const auto& file_path : m_directory_files | std::views::keys) {
				const std::filesystem::path relative = file_path.lexically_relative(path);
				const bool belongs = !relative.empty() && *relative.begin() != ".." && (recursive || file_path.parent_path() == path);
				if (belongs && !current_files.contains(file_path)) {
					removed.push_back(file_path);
				}
			}
			for (const std::filesystem::path& file_path : removed) {
				m_directory_files.erase(file_path);
				on_change(file_path);
				++changes;
			}
		}
		else {
			if (!std::filesystem::exists(path)) {
				continue;
			}

			auto current_time = std::filesystem::last_write_time(path);
			if (last_modified != current_time) {
				last_modified = current_time;
				on_change(path);
				++changes;
			}
		}
	}

	return changes;
}

auto gse::file_watcher::clear() -> void {
	std::lock_guard lock(m_mutex);
	m_watches.clear();
	m_directory_files.clear();
}

auto gse::file_watcher::matches_extensions(const std::filesystem::path& path, std::span<const std::string> extensions) -> bool {
	if (extensions.empty()) {
		return true;
	}

	const auto ext = path.extension().native_encoded_string();
	return std::ranges::find(extensions, ext) != extensions.end();
}

auto gse::file_watcher::scan_directory(const std::filesystem::path& directory, const std::span<const std::string> extensions, const bool recursive, const directory_filter& recurse_into) -> std::unordered_map<std::filesystem::path, std::filesystem::file_time_type> {
	std::unordered_map<std::filesystem::path, std::filesystem::file_time_type> result;

	auto process_entry = [&](const std::filesystem::directory_entry& entry) {
		std::error_code type_ec;
		if (!entry.is_regular_file(type_ec)) {
			return;
		}

		if (matches_extensions(entry.path(), extensions)) {
			std::error_code time_ec;
			const std::filesystem::file_time_type modified = entry.last_write_time(time_ec);
			if (!time_ec) {
				result[entry.path()] = modified;
			}
		}
	};

	if (recursive) {
		std::error_code ec;
		for (std::filesystem::recursive_directory_iterator it(
			directory,
			std::filesystem::directory_options::skip_permission_denied,
			ec
		), end; it != end; it.increment(ec)) {
			if (ec) {
				ec.clear();
				continue;
			}
			std::error_code directory_ec;
			if (it->is_directory(directory_ec)) {
				if (!directory_ec && recurse_into && !recurse_into(it->path())) {
					it.disable_recursion_pending();
				}
				continue;
			}
			process_entry(*it);
		}
	}
	else {
		std::error_code ec;
		for (std::filesystem::directory_iterator it(
			directory,
			std::filesystem::directory_options::skip_permission_denied,
			ec
		), end; it != end; it.increment(ec)) {
			if (ec) {
				ec.clear();
				continue;
			}
			process_entry(*it);
		}
	}

	return result;
}
