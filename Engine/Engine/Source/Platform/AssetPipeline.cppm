export module gse.platform:asset_pipeline;

import std;

import gse.utility;

import :asset_compiler;
import :resource_loader;

export namespace gse {
    class asset_pipeline {
    public:
        struct compile_result {
            std::set<std::filesystem::path> compiled_paths;
            std::size_t success_count = 0;
            std::size_t failure_count = 0;
            std::size_t skipped_count = 0;
        };

        explicit asset_pipeline(
            std::filesystem::path resource_root,
            std::filesystem::path baked_root
        );

        template <typename Resource, typename Context>
            requires has_asset_compiler<Resource>
        auto register_type(
            resource::loader<Resource, Context>* loader
        ) -> void;

        template <typename Resource>
            requires has_asset_compiler<Resource>
        auto register_compiler_only() -> void {
            std::lock_guard lock(m_mutex);

            compiler_entry entry{
                .type = typeid(Resource),
                .extensions = asset_compiler<Resource>::source_extensions(),
                .source_dir = asset_compiler<Resource>::source_directory(),
                .baked_dir = asset_compiler<Resource>::baked_directory(),
                .baked_ext = asset_compiler<Resource>::baked_extension(),
                .compile_fn = [](const std::filesystem::path& src, const std::filesystem::path& dst) {
                    return asset_compiler<Resource>::compile_one(src, dst);
                },
                .needs_recompile_fn = [](const std::filesystem::path& src, const std::filesystem::path& dst) {
                    return asset_compiler<Resource>::needs_recompile(src, dst);
                },
                .dependencies_fn = [](const std::filesystem::path& src) {
                    return asset_compiler<Resource>::dependencies(src);
                },
                .reload_fn = nullptr,
                .queue_fn = nullptr
            };

            m_compilers.push_back(std::move(entry));
        }

        auto compile_all(
        ) -> compile_result;

        template<typename Resource>
            requires has_asset_compiler<Resource>
        auto compile(
        ) const -> compile_result;

        auto enable_hot_reload(
        ) -> void;

        auto disable_hot_reload(
        ) -> void;

        auto poll(
        ) -> void;

        [[nodiscard]] auto hot_reload_enabled(
        ) const -> bool;

    private:
        struct compiler_entry {
            std::type_index type;
            std::vector<std::string> extensions;
            std::string source_dir;
            std::string baked_dir;
            std::string baked_ext;
            std::function<bool(const std::filesystem::path&, const std::filesystem::path&)> compile_fn;
            std::function<bool(const std::filesystem::path&, const std::filesystem::path&)> needs_recompile_fn;
            std::function<std::vector<std::filesystem::path>(const std::filesystem::path&)> dependencies_fn;
            std::function<void(const std::filesystem::path&)> reload_fn;
            std::function<void(const std::filesystem::path&)> queue_fn;
        };

        std::filesystem::path m_resource_root;
        std::filesystem::path m_baked_root;
        std::vector<compiler_entry> m_compilers;
        file_watcher m_watcher;
        bool m_hot_reload_enabled = false;
        mutable std::mutex m_mutex;

        auto find_compiler(
            const std::filesystem::path& source
        ) const -> const compiler_entry*;

        auto compute_baked_path(
            const compiler_entry& compiler,
            const std::filesystem::path& source
        ) const -> std::filesystem::path;

        auto compile_single(
            const compiler_entry& compiler,
            const std::filesystem::path& source
        ) const -> bool;
    };
}

gse::asset_pipeline::asset_pipeline(
    std::filesystem::path resource_root,
    std::filesystem::path baked_root
) : m_resource_root(std::move(resource_root))
  , m_baked_root(std::move(baked_root)) {
}

template <typename Resource, typename Context>
    requires gse::has_asset_compiler<Resource>
auto gse::asset_pipeline::register_type(resource::loader<Resource, Context>* loader) -> void {
    std::lock_guard lock(m_mutex);

    compiler_entry entry{
        .type = typeid(Resource),
        .extensions = asset_compiler<Resource>::source_extensions(),
        .source_dir = asset_compiler<Resource>::source_directory(),
        .baked_dir = asset_compiler<Resource>::baked_directory(),
        .baked_ext = asset_compiler<Resource>::baked_extension(),
        .compile_fn = [](const std::filesystem::path& src, const std::filesystem::path& dst) {
            return asset_compiler<Resource>::compile_one(src, dst);
        },
        .needs_recompile_fn = [](const std::filesystem::path& src, const std::filesystem::path& dst) {
            return asset_compiler<Resource>::needs_recompile(src, dst);
        },
        .dependencies_fn = [](const std::filesystem::path& src) {
            return asset_compiler<Resource>::dependencies(src);
        },
        .reload_fn = [loader](const std::filesystem::path& baked_path) {
            loader->queue_reload_by_path(baked_path);
        },
        .queue_fn = [loader](const std::filesystem::path& baked_path) {
            loader->queue_by_path(baked_path);
        }
    };

    m_compilers.push_back(std::move(entry));
}

auto gse::asset_pipeline::compile_all() -> compile_result {
    compile_result total_result;

    for (const auto& compiler : m_compilers) {
        const auto source_path = m_resource_root / compiler.source_dir;
        const auto baked_path = m_baked_root / compiler.baked_dir;

        if (!std::filesystem::exists(source_path)) {
            continue;
        }

        if (!std::filesystem::exists(baked_path)) {
            std::filesystem::create_directories(baked_path);
        }

        for (const auto& entry : std::filesystem::recursive_directory_iterator(source_path)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            const auto& file_path = entry.path();
            const auto ext = file_path.extension().string();

            if (std::ranges::find(compiler.extensions, ext) == compiler.extensions.end()) {
                continue;
            }

            const auto dest = compute_baked_path(compiler, file_path);
            total_result.compiled_paths.insert(dest);

            bool needs_compile = compiler.needs_recompile_fn(file_path, dest);

            if (!needs_compile) {
                for (const auto& dep : compiler.dependencies_fn(file_path)) {
                    if (std::filesystem::exists(dep) &&
                        std::filesystem::last_write_time(dep) > std::filesystem::last_write_time(dest)) {
                        needs_compile = true;
                        break;
                    }
                }
            }

            if (!needs_compile) {
                ++total_result.skipped_count;
                if (compiler.queue_fn && std::filesystem::exists(dest)) {
                    compiler.queue_fn(dest);
                }
                continue;
            }

            if (!std::filesystem::exists(dest.parent_path())) {
                std::filesystem::create_directories(dest.parent_path());
            }

            if (compile_single(compiler, file_path)) {
                ++total_result.success_count;
                if (compiler.queue_fn && std::filesystem::exists(dest)) {
                    compiler.queue_fn(dest);
                }
            } else {
                ++total_result.failure_count;
            }
        }
    }

    return total_result;
}

template <typename Resource>
    requires gse::has_asset_compiler<Resource>
auto gse::asset_pipeline::compile() const -> compile_result {
    compile_result result;
    const auto target_type = std::type_index(typeid(Resource));

    for (const auto& compiler : m_compilers) {
        if (compiler.type != target_type) {
            continue;
        }

        const auto source_path = m_resource_root / compiler.source_dir;
        const auto baked_path = m_baked_root / compiler.baked_dir;

        if (!std::filesystem::exists(source_path)) {
            continue;
        }

        if (!std::filesystem::exists(baked_path)) {
            std::filesystem::create_directories(baked_path);
        }

        for (const auto& entry : std::filesystem::recursive_directory_iterator(source_path)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            const auto& file_path = entry.path();
            const auto ext = file_path.extension().string();

            if (std::ranges::find(compiler.extensions, ext) == compiler.extensions.end()) {
                continue;
            }

            const auto dest = compute_baked_path(compiler, file_path);
            result.compiled_paths.insert(dest);

            bool needs_compile = compiler.needs_recompile_fn(file_path, dest);

            if (!needs_compile) {
                for (const auto& dep : compiler.dependencies_fn(file_path)) {
                    if (std::filesystem::exists(dep) &&
                        std::filesystem::last_write_time(dep) > std::filesystem::last_write_time(dest)) {
                        needs_compile = true;
                        break;
                    }
                }
            }

            if (!needs_compile) {
                ++result.skipped_count;
                if (compiler.queue_fn && std::filesystem::exists(dest)) {
                    compiler.queue_fn(dest);
                }
                continue;
            }

            if (!std::filesystem::exists(dest.parent_path())) {
                std::filesystem::create_directories(dest.parent_path());
            }

            if (compile_single(compiler, file_path)) {
                ++result.success_count;
                if (compiler.queue_fn && std::filesystem::exists(dest)) {
                    compiler.queue_fn(dest);
                }
            } else {
                ++result.failure_count;
            }
        }
    }

    return result;
}

auto gse::asset_pipeline::enable_hot_reload() -> void {
    std::lock_guard lock(m_mutex);

    if (m_hot_reload_enabled) {
        return;
    }

    for (const auto& compiler : m_compilers) {
        const auto source_path = m_resource_root / compiler.source_dir;

        if (!std::filesystem::exists(source_path)) {
            continue;
        }

        m_watcher.watch_directory(
            source_path,
            [this, &compiler](const std::filesystem::path& changed_file) {
                const auto ext = changed_file.extension().string();
                if (std::ranges::find(compiler.extensions, ext) == compiler.extensions.end()) {
                    return;
                }

                const auto dest = compute_baked_path(compiler, changed_file);

                if (!std::filesystem::exists(dest.parent_path())) {
                    std::filesystem::create_directories(dest.parent_path());
                }

                if (compile_single(compiler, changed_file)) {
                    std::println("[Hot Reload] Recompiled: {}", changed_file.filename().string());
                    if (compiler.reload_fn) {
                        compiler.reload_fn(dest);
                    }
                } else {
                    std::println("[Hot Reload] Failed to recompile: {}", changed_file.filename().string());
                }
            },
            compiler.extensions,
            true
        );
    }

    m_hot_reload_enabled = true;
}

auto gse::asset_pipeline::disable_hot_reload() -> void {
    std::lock_guard lock(m_mutex);

    m_watcher.clear();
    m_hot_reload_enabled = false;
}

auto gse::asset_pipeline::poll() -> void {
    if (!m_hot_reload_enabled) {
        return;
    }

    m_watcher.poll();
}

auto gse::asset_pipeline::hot_reload_enabled() const -> bool {
    return m_hot_reload_enabled;
}

auto gse::asset_pipeline::find_compiler(const std::filesystem::path& source) const -> const compiler_entry* {
    const auto ext = source.extension().string();

    for (const auto& compiler : m_compilers) {
        if (std::ranges::find(compiler.extensions, ext) != compiler.extensions.end()) {
            return &compiler;
        }
    }

    return nullptr;
}

auto gse::asset_pipeline::compute_baked_path(const compiler_entry& compiler, const std::filesystem::path& source) const -> std::filesystem::path {
    const auto source_dir = m_resource_root / compiler.source_dir;
    auto relative = source.lexically_relative(source_dir);
    relative.replace_extension(compiler.baked_ext);
    return m_baked_root / compiler.baked_dir / relative;
}

auto gse::asset_pipeline::compile_single(const compiler_entry& compiler, const std::filesystem::path& source) const -> bool {
    const auto dest = compute_baked_path(compiler, source);
    return compiler.compile_fn(source, dest);
}
