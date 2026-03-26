export module gse.platform:asset_compiler;

import std;

export namespace gse {
    template<typename Resource>
    struct asset_compiler {
        static auto source_extensions() -> std::vector<std::string> = delete;
        static auto baked_extension() -> std::string = delete;
        static auto source_directory() -> std::string = delete;
        static auto baked_directory() -> std::string = delete;
        static auto compile_one(
            const std::filesystem::path& source,
            const std::filesystem::path& destination
        ) -> bool = delete;

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

    template<typename Resource>
    concept has_asset_compiler = requires {
        { asset_compiler<Resource>::source_extensions() } -> std::same_as<std::vector<std::string>>;
        { asset_compiler<Resource>::baked_extension() } -> std::same_as<std::string>;
        { asset_compiler<Resource>::source_directory() } -> std::same_as<std::string>;
        { asset_compiler<Resource>::baked_directory() } -> std::same_as<std::string>;
        { asset_compiler<Resource>::compile_one(
            std::declval<std::filesystem::path>(),
            std::declval<std::filesystem::path>()
        ) } -> std::same_as<bool>;
    };
}
