module gse.fs:file_watcher_impl;

import gse.math;
import gse.time;
import std;

import :file_watcher;

gse::file_watcher::~file_watcher() {
	clear();
}

auto gse::file_watcher::matches_extensions(const std::filesystem::path& path, const std::span<const std::string> extensions) -> bool {
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
		if (!matches_extensions(entry.path(), extensions)) {
			return;
		}
		std::error_code time_ec;
		const std::filesystem::file_time_type modified = entry.last_write_time(time_ec);
		if (!time_ec) {
			result[entry.path()] = modified;
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
		return result;
	}

	std::error_code ec;
	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(
		directory,
		std::filesystem::directory_options::skip_permission_denied,
		ec
	)) {
		process_entry(entry);
	}
	return result;
}

auto gse::file_watcher::entry_for_stream(const void* stream) -> watch_entry* {
	const auto at = std::ranges::find(m_watches, stream, &watch_entry::stream);
	return at == m_watches.end() ? nullptr : &*at;
}

auto gse::file_watcher::emit_change(watch_entry& entry, const std::filesystem::path& path) -> std::size_t {
	if (!matches_extensions(path, entry.extensions)) {
		return 0;
	}
	if (const std::filesystem::path parent = path.parent_path(); entry.recurse_into && parent != entry.path && !entry.recurse_into(parent)) {
		return 0;
	}

	std::error_code ec;
	if (std::filesystem::is_regular_file(path, ec)) {
		std::error_code time_ec;
		const std::filesystem::file_time_type modified = std::filesystem::last_write_time(path, time_ec);
		if (time_ec) {
			return 0;
		}
		if (const auto known = m_directory_files.find(path); known != m_directory_files.end() && known->second == modified) {
			return 0;
		}
		m_directory_files[path] = modified;
	}
	else {
		if (m_directory_files.erase(path) == 0) {
			return 0;
		}
	}

	entry.on_change(path);
	return 1;
}

auto gse::file_watcher::rescan_entry(watch_entry& entry) -> std::size_t {
	std::size_t changes = 0;
	const auto current_files = scan_directory(entry.path, entry.extensions, entry.recursive, entry.recurse_into);

	for (const auto& [file_path, mod_time] : current_files) {
		const auto known = m_directory_files.find(file_path);
		if (known == m_directory_files.end() || known->second != mod_time) {
			m_directory_files[file_path] = mod_time;
			entry.on_change(file_path);
			++changes;
		}
	}

	std::vector<std::filesystem::path> removed;
	for (const auto& file_path : m_directory_files | std::views::keys) {
		const std::filesystem::path relative = file_path.lexically_relative(entry.path);
		const bool belongs = !relative.empty() && *relative.begin() != ".." && (entry.recursive || file_path.parent_path() == entry.path);
		if (belongs && !current_files.contains(file_path)) {
			removed.push_back(file_path);
		}
	}
	for (const std::filesystem::path& file_path : removed) {
		m_directory_files.erase(file_path);
		entry.on_change(file_path);
		++changes;
	}

	return changes;
}

auto gse::file_watcher::watch(const std::filesystem::path& path, callback on_change) -> void {
	std::lock_guard _(m_mutex);

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

auto gse::file_watcher::watch_directory(const std::filesystem::path& directory, callback on_change, const std::span<const std::string> extensions, const bool recursive, directory_filter recurse_into) -> void {
	std::lock_guard _(m_mutex);

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

	open_stream(m_watches.back());
}

auto gse::file_watcher::unwatch(const std::filesystem::path& path) -> void {
	std::lock_guard _(m_mutex);

	for (watch_entry& entry : m_watches) {
		if (entry.path == path) {
			close_stream(entry);
		}
	}

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
	std::lock_guard _(m_mutex);

	if (!m_poll_timer.tick()) {
		return 0;
	}

	return poll_locked();
}

auto gse::file_watcher::poll_now() -> std::size_t {
	std::lock_guard _(m_mutex);
	m_poll_timer.reset();
	return poll_locked();
}

auto gse::file_watcher::poll_locked() -> std::size_t {
	std::size_t changes = drain_completions();

	for (watch_entry& entry : m_watches) {
		if (entry.is_directory) {
			if (!entry.stream) {
				changes += rescan_entry(entry);
			}
			continue;
		}
		if (!std::filesystem::exists(entry.path)) {
			continue;
		}
		if (const auto current_time = std::filesystem::last_write_time(entry.path); entry.last_modified != current_time) {
			entry.last_modified = current_time;
			entry.on_change(entry.path);
			++changes;
		}
	}

	return changes;
}

auto gse::file_watcher::clear() -> void {
	std::lock_guard _(m_mutex);

	for (watch_entry& entry : m_watches) {
		close_stream(entry);
	}
	m_watches.clear();
	m_directory_files.clear();

	destroy_completion_port();
}
