export module gse.assets:asset_compiler;

import std;
import gse.std_meta;
import gse.containers;
import gse.log;

import :asset_format;

export namespace gse {

	template <typename T>
	concept has_baked_with_format = requires { typename T::baked; } && has_asset_format<typename T::baked>;

	template <typename T>
	concept has_bake_hook = requires (const std::filesystem::path& src, typename T::baked& b) {
		{ bake(src, b) } -> std::convertible_to<bool>;
	};

	template <typename Resource>
	struct asset_compiler {
		static auto source_extensions(
		) -> std::vector<std::string> = delete;

		static auto baked_extension(
		) -> std::string = delete;

		static auto source_directory(
		) -> std::string = delete;

		static auto baked_directory(
		) -> std::string = delete;

		static auto compile_one(
			const std::filesystem::path& source,
			const std::filesystem::path& destination
		) -> bool = delete;

		static auto needs_recompile(
			const std::filesystem::path& source,
			const std::filesystem::path& destination
		) -> bool;

		static auto dependencies(
			const std::filesystem::path& source
		) -> std::vector<std::filesystem::path>;
	};

	template <typename Resource>
		requires has_baked_with_format<Resource> && has_bake_hook<Resource>
	struct asset_compiler<Resource> {
		static constexpr auto fmt = format_of<typename Resource::baked>();

		static auto source_extensions() -> std::vector<std::string> {
			return std::vector<std::string>(fmt.source_exts.begin(), fmt.source_exts.end());
		}

		static auto baked_extension() -> std::string {
			return std::string(fmt.baked_ext);
		}

		static auto source_directory() -> std::string {
			return std::string(fmt.source_dir);
		}

		static auto baked_directory() -> std::string {
			return std::string(fmt.baked_dir);
		}

		static auto compile_one(
			const std::filesystem::path& src,
			const std::filesystem::path& dst
		) -> bool {
			typename Resource::baked b{};
			if (!bake(src, b)) {
				return false;
			}
			std::filesystem::create_directories(dst.parent_path());
			std::ofstream out(dst, std::ios::binary);
			if (!out.is_open()) {
				return false;
			}
			binary_writer ar(out, fmt.magic, fmt.version);
			ar & b;
			log::println(log::category::assets, "Asset compiled: {}", dst.filename().string());
			return true;
		}

		static auto needs_recompile(
			const std::filesystem::path& src,
			const std::filesystem::path& dst
		) -> bool {
			if (!std::filesystem::exists(dst)) {
				return true;
			}
			const auto dst_time = std::filesystem::last_write_time(dst);
			if (std::filesystem::last_write_time(src) > dst_time) {
				return true;
			}
			if constexpr (fmt.meta_sidecar) {
				const auto meta = src.parent_path() / (src.stem().string() + ".meta");
				if (std::filesystem::exists(meta) && std::filesystem::last_write_time(meta) > dst_time) {
					return true;
				}
			}
			return false;
		}

		static auto dependencies(
			const std::filesystem::path& src
		) -> std::vector<std::filesystem::path> {
			if constexpr (fmt.meta_sidecar) {
				const auto meta = src.parent_path() / (src.stem().string() + ".meta");
				if (std::filesystem::exists(meta)) {
					return { meta };
				}
			}
			return {};
		}
	};

	template <typename Resource>
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

template <typename Resource>
auto gse::asset_compiler<Resource>::needs_recompile(const std::filesystem::path& source, const std::filesystem::path& destination) -> bool {
	if (!std::filesystem::exists(destination)) {
		return true;
	}
	return std::filesystem::last_write_time(source) > std::filesystem::last_write_time(destination);
}

template <typename Resource>
auto gse::asset_compiler<Resource>::dependencies(const std::filesystem::path&) -> std::vector<std::filesystem::path> {
	return {};
}
