export module gse.graphics:texture_compiler;

import std;

import gse.platform;
import gse.assert;
import gse.log;
import gse.utility;

import :texture;

export template<>
struct gse::asset_compiler<gse::texture> {
    static auto source_extensions() -> std::vector<std::string> {
        return { ".png", ".jpg", ".jpeg", ".tga", ".bmp" };
    }

    static auto baked_extension() -> std::string {
        return ".gtx";
    }

    static auto source_directory() -> std::string {
        return "";
    }

    static auto baked_directory() -> std::string {
        return "Textures";
    }

    static auto compile_one(
        const std::filesystem::path& source,
        const std::filesystem::path& destination
    ) -> bool {
        auto image_data = image::load(source);
        if (image_data.pixels.empty()) {
            log::println(log::level::warning, log::category::assets, "Failed to load texture '{}', skipping", source.string());
            return false;
        }

        auto texture_profile = texture::profile::generic_repeat;
        const auto meta_path = source.parent_path() / (source.stem().string() + ".meta");

        if (std::filesystem::exists(meta_path)) {
            std::ifstream meta_file(meta_path);
            std::string line;
            if (std::getline(meta_file, line) && line.starts_with("profile:")) {
                std::string profile_str = line.substr(8);
                profile_str.erase(0, profile_str.find_first_not_of(" \t\r\n"));
                profile_str.erase(profile_str.find_last_not_of(" \t\r\n") + 1);

                if (profile_str == "msdf") {
                    texture_profile = texture::profile::msdf;
                } else if (profile_str == "pixel_art") {
                    texture_profile = texture::profile::pixel_art;
                } else if (profile_str == "clamp_to_edge") {
                    texture_profile = texture::profile::generic_clamp_to_edge;
                }
            }
        }

        std::filesystem::create_directories(destination.parent_path());
        std::ofstream out_file(destination, std::ios::binary);
        if (!out_file.is_open()) {
            return false;
        }

        const std::uint32_t width = image_data.size.x();
        const std::uint32_t height = image_data.size.y();
        const std::uint32_t channels = image_data.channels;

        binary_writer ar(out_file, 0x47544558, 1);
        ar & width & height & channels & texture_profile;
        ar & raw_blob(image_data.pixels);

        log::println(log::category::assets, "Texture compiled: {}", destination.filename().string());
        return true;
    }

    static auto needs_recompile(
        const std::filesystem::path& source,
        const std::filesystem::path& destination
    ) -> bool {
        if (!std::filesystem::exists(destination)) {
            return true;
        }

        const auto dst_time = std::filesystem::last_write_time(destination);

        if (std::filesystem::last_write_time(source) > dst_time) {
            return true;
        }

        const auto meta_path = source.parent_path() / (source.stem().string() + ".meta");
        if (std::filesystem::exists(meta_path) && std::filesystem::last_write_time(meta_path) > dst_time) {
            return true;
        }

        return false;
    }

    static auto dependencies(
        const std::filesystem::path& source
    ) -> std::vector<std::filesystem::path> {
        std::vector<std::filesystem::path> deps;
        const auto meta_path = source.parent_path() / (source.stem().string() + ".meta");
        if (std::filesystem::exists(meta_path)) {
            deps.push_back(meta_path);
        }
        return deps;
    }
};
