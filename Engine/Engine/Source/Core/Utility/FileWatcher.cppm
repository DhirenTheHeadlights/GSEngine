export module gse.utility:file_watcher;

import std;

export namespace gse {
    class file_watcher {
    public:
        using callback = std::function<void(const std::filesystem::path&)>;

        auto watch(
            const std::filesystem::path& path,
            callback on_change
        ) -> void;

        auto watch_directory(
            const std::filesystem::path& directory,
            callback on_change,
            std::span<const std::string> extensions = {},
            bool recursive = true
        ) -> void;

        auto unwatch(
            const std::filesystem::path& path
        ) -> void;

        auto poll(
        ) -> std::size_t;

        auto clear(
        ) -> void;

    private:
        struct watch_entry {
            std::filesystem::path path;
            std::filesystem::file_time_type last_modified;
            callback on_change;
            bool is_directory;
            bool recursive;
            std::vector<std::string> extensions;
        };

        std::vector<watch_entry> m_watches;
        std::unordered_map<std::filesystem::path, std::filesystem::file_time_type> m_directory_files;
        mutable std::mutex m_mutex;

        auto matches_extensions(
            const std::filesystem::path& path,
            std::span<const std::string> extensions
        ) const -> bool;

        auto scan_directory(
            const std::filesystem::path& directory,
            std::span<const std::string> extensions,
            bool recursive
        ) -> std::unordered_map<std::filesystem::path, std::filesystem::file_time_type>;
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
        .extensions = {}
    });
}

auto gse::file_watcher::watch_directory(
    const std::filesystem::path& directory,
    callback on_change,
    std::span<const std::string> extensions,
    const bool recursive
) -> void {
    std::lock_guard lock(m_mutex);

    if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
        return;
    }

    std::vector<std::string> ext_vec(extensions.begin(), extensions.end());

    auto files = scan_directory(directory, extensions, recursive);
    for (const auto& [file_path, mod_time] : files) {
        m_directory_files[file_path] = mod_time;
    }

    m_watches.push_back({
        .path = directory,
        .last_modified = {},
        .on_change = std::move(on_change),
        .is_directory = true,
        .recursive = recursive,
        .extensions = std::move(ext_vec)
    });
}

auto gse::file_watcher::unwatch(const std::filesystem::path& path) -> void {
    std::lock_guard lock(m_mutex);

    std::erase_if(m_watches, [&](const watch_entry& entry) {
        return entry.path == path;
    });

    if (std::filesystem::is_directory(path)) {
        std::erase_if(m_directory_files, [&](const auto& pair) {
            auto [file_path, _] = pair;
            return file_path.string().starts_with(path.string());
        });
    }
}

auto gse::file_watcher::poll() -> std::size_t {
    std::lock_guard lock(m_mutex);
    std::size_t changes = 0;

    for (auto& entry : m_watches) {
        if (entry.is_directory) {
            auto current_files = scan_directory(entry.path, entry.extensions, entry.recursive);

            for (const auto& [file_path, mod_time] : current_files) {
                auto it = m_directory_files.find(file_path);

                if (it == m_directory_files.end()) {
                    m_directory_files[file_path] = mod_time;
                    entry.on_change(file_path);
                    ++changes;
                } else if (it->second < mod_time) {
                    it->second = mod_time;
                    entry.on_change(file_path);
                    ++changes;
                }
            }
        } else {
            if (!std::filesystem::exists(entry.path)) {
                continue;
            }

            auto current_time = std::filesystem::last_write_time(entry.path);
            if (entry.last_modified < current_time) {
                entry.last_modified = current_time;
                entry.on_change(entry.path);
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

auto gse::file_watcher::matches_extensions(
    const std::filesystem::path& path,
    std::span<const std::string> extensions
) const -> bool {
    if (extensions.empty()) {
        return true;
    }

    const auto ext = path.extension().string();
    return std::ranges::find(extensions, ext) != extensions.end();
}

auto gse::file_watcher::scan_directory(
    const std::filesystem::path& directory,
    std::span<const std::string> extensions,
    const bool recursive
) -> std::unordered_map<std::filesystem::path, std::filesystem::file_time_type> {
    std::unordered_map<std::filesystem::path, std::filesystem::file_time_type> result;

    auto process_entry = [&](const std::filesystem::directory_entry& entry) {
        if (!entry.is_regular_file()) {
            return;
        }

        if (matches_extensions(entry.path(), extensions)) {
            result[entry.path()] = entry.last_write_time();
        }
    };

    if (recursive) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
            process_entry(entry);
        }
    } else {
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            process_entry(entry);
        }
    }

    return result;
}
