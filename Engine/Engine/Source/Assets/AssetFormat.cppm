export module gse.assets:asset_format;

import std;
import gse.meta;
import gse.containers;

export namespace gse::asset_format {
	template <fixed_string V>
	struct baked_ext {
		static constexpr std::string_view value = V;
	};

	template <fixed_string V>
	struct baked_dir {
		static constexpr std::string_view value = V;
	};

	template <fixed_string V>
	struct source_dir {
		static constexpr std::string_view value = V;
	};

	template <fixed_string... V>
	struct source_exts {
		static constexpr std::array<std::string_view, sizeof...(V)> value{ std::string_view(V)... };
	};

	template <std::uint32_t V>
	struct magic {
		static constexpr std::uint32_t value = V;
	};

	template <std::uint32_t V>
	struct version {
		static constexpr std::uint32_t value = V;
	};

	struct meta_sidecar {
		static constexpr bool value = true;
	};
}

export namespace gse {
	struct asset_format_data {
		std::span<const std::string_view> source_exts;
		std::string_view source_dir;
		std::string_view baked_ext;
		std::string_view baked_dir;
		std::uint32_t magic;
		std::uint32_t version;
		bool meta_sidecar;
	};

	template <typename T>
	consteval auto find_asset_format() -> std::optional<asset_format_data>;

	template <typename T>
	concept has_asset_format = find_asset_format<T>().has_value();

	template <has_asset_format T>
	consteval auto format_of() -> asset_format_data;

	template <has_asset_format T>
	auto load_baked(
		const std::filesystem::path& path,
		T& out
	) -> bool;
}

template <typename T>
consteval auto gse::find_asset_format() -> std::optional<asset_format_data> {
	return apply_annotations<asset_format_data, T>();
}

template <gse::has_asset_format T>
consteval auto gse::format_of() -> asset_format_data {
	return *find_asset_format<T>();
}

template <gse::has_asset_format T>
auto gse::load_baked(const std::filesystem::path& path, T& out) -> bool {
	std::ifstream in(path, std::ios::binary);
	if (!in.is_open()) {
		return false;
	}

	constexpr auto fmt = format_of<T>();
	binary_reader ar(in, fmt.magic, fmt.version, path.string());
	if (!ar.valid()) {
		return false;
	}

	ar & out;
	return true;
}
