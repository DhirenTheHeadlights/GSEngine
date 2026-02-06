export module gse.graphics:skeleton_compiler;

import std;

import gse.platform;

import :asset_compiler;
import :skeleton;

export template<>
struct gse::asset_compiler<gse::skeleton> {
    static auto source_extensions() -> std::vector<std::string> {
        return { ".gskel" };
    }

    static auto baked_extension() -> std::string {
        return ".gskel";
    }

    static auto source_directory() -> std::string {
        return "Skeletons";
    }

    static auto baked_directory() -> std::string {
        return "Skeletons";
    }

    static auto compile_one(
        const std::filesystem::path& source,
        const std::filesystem::path& destination
    ) -> bool {
        std::filesystem::create_directories(destination.parent_path());
        std::filesystem::copy_file(source, destination, std::filesystem::copy_options::overwrite_existing);
        std::println("Skeleton compiled: {}", destination.filename().string());
        return true;
    }

    static auto needs_recompile(
        const std::filesystem::path& source,
        const std::filesystem::path& destination
    ) -> bool {
        if (!std::filesystem::exists(destination)) {
            return true;
        }
        return std::filesystem::last_write_time(source) >
               std::filesystem::last_write_time(destination);
    }

    static auto dependencies(
        const std::filesystem::path&
    ) -> std::vector<std::filesystem::path> {
        return {};
    }
};
