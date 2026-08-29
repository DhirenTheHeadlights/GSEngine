export module gse.assets:asset_format;

import std;
import gse.meta;
import gse.containers;
import gse.diag;
import gse.log;

#ifdef _WIN32
import gse.win32;
#endif

export namespace gse {
	namespace asset_format {
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

		template <fixed_string... V>
		struct built_ins {
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

		template <bool V = true>
		struct meta_sidecar {
			static constexpr bool value = V;
		};
	}

	enum struct asset_error_code {
		missing_file,
		invalid_format,
		io_failure,
		load_failure,
		ambiguous_source
	};

	struct asset_error {
		asset_error_code code = asset_error_code::load_failure;
		std::filesystem::path path;
		std::string detail;
	};

	using asset_result = std::expected<void, asset_error>;

	struct asset_format_data {
		std::span<const std::string_view> source_exts;
		std::span<const std::string_view> built_ins;
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
		const std::filesystem::path& path
	) -> std::expected<T, asset_error>;

	template <has_asset_format T>
	auto write_baked(
		const std::filesystem::path& path,
		const T& value
	) -> asset_result;

	template <has_asset_format T>
	auto baked_schema_current(
		const std::filesystem::path& path
	) -> bool;
}

namespace gse {
	inline std::atomic<std::uint64_t> baked_temporary_id{ 0 };

	auto replace_baked_file(
		const std::filesystem::path& temporary,
		const std::filesystem::path& destination
	) -> asset_result;
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
auto gse::load_baked(const std::filesystem::path& path) -> std::expected<T, asset_error> {
	std::ifstream in(path, std::ios::binary);
	if (!in.is_open()) {
		return std::unexpected(asset_error{
			.code = asset_error_code::missing_file,
			.path = path,
			.detail = "Unable to open baked asset"
		});
	}

	constexpr auto fmt = format_of<T>();
	std::expected<binary_reader, archive_mismatch> opened = binary_reader::open(in, fmt.magic, fmt.version);
	if (!opened) {
		return std::unexpected(asset_error{
			.code = asset_error_code::invalid_format,
			.path = path,
			.detail = "Baked asset header does not match its format"
		});
	}

	std::uint64_t stored_fingerprint = 0;
	*opened & stored_fingerprint;
	if (!opened->valid() || stored_fingerprint != schema_fingerprint<T>()) {
		return std::unexpected(asset_error{
			.code = asset_error_code::invalid_format,
			.path = path,
			.detail = "Baked asset schema fingerprint does not match this build; rebake it from source"
		});
	}

	try {
		T out{};
		*opened & out;
		if (!opened->valid()) {
			return std::unexpected(asset_error{
				.code = asset_error_code::invalid_format,
				.path = path,
				.detail = "Baked asset payload is truncated or malformed"
			});
		}
		for (const std::string& skipped : opened->skipped_fields()) {
			log::println(
				log::level::warning,
				log::category::assets,
				"{}: schema drift, stored field skipped and left defaulted: {}",
				path.generic_display_string(),
				skipped
			);
		}
		return std::move(out);
	}
	catch (const std::exception& error) {
		return std::unexpected(asset_error{
			.code = asset_error_code::invalid_format,
			.path = path,
			.detail = error.what()
		});
	}
	catch (...) {
		return std::unexpected(asset_error{
			.code = asset_error_code::invalid_format,
			.path = path,
			.detail = "Baked asset deserialization failed"
		});
	}
}

template <gse::has_asset_format T>
auto gse::write_baked(const std::filesystem::path& path, const T& value) -> asset_result {
	std::error_code ec;
	if (const auto parent = path.parent_path(); !parent.empty()) {
		std::filesystem::create_directories(parent, ec);
	}
	if (ec) {
		return std::unexpected(asset_error{
			.code = asset_error_code::io_failure,
			.path = path,
			.detail = ec.message()
		});
	}

	auto temporary = path;
	temporary += std::format(".{}.tmp", baked_temporary_id.fetch_add(1, std::memory_order_relaxed));

	std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
	if (!out.is_open()) {
		return std::unexpected(asset_error{
			.code = asset_error_code::io_failure,
			.path = temporary,
			.detail = "Unable to open temporary baked asset"
		});
	}

	constexpr auto fmt = format_of<T>();
	bool written = false;
	try {
		binary_writer writer(out, fmt.magic, fmt.version);
		constexpr std::uint64_t fingerprint = schema_fingerprint<T>();
		writer & fingerprint;
		writer & value;
		out.flush();
		written = writer.valid() && out.good();
	}
	catch (const std::exception& error) {
		out.close();
		std::filesystem::remove(temporary, ec);
		return std::unexpected(asset_error{
			.code = asset_error_code::io_failure,
			.path = temporary,
			.detail = error.what()
		});
	}
	catch (...) {
		out.close();
		std::filesystem::remove(temporary, ec);
		return std::unexpected(asset_error{
			.code = asset_error_code::io_failure,
			.path = temporary,
			.detail = "Baked asset serialization failed"
		});
	}
	out.close();
	written = written && !out.fail();
	if (!written) {
		std::filesystem::remove(temporary, ec);
		return std::unexpected(asset_error{
			.code = asset_error_code::io_failure,
			.path = temporary,
			.detail = "Unable to write complete baked asset"
		});
	}

	return replace_baked_file(temporary, path);
}

template <gse::has_asset_format T>
auto gse::baked_schema_current(const std::filesystem::path& path) -> bool {
	std::ifstream in(path, std::ios::binary);
	if (!in.is_open()) {
		return false;
	}

	constexpr auto fmt = format_of<T>();
	std::expected<binary_reader, archive_mismatch> opened = binary_reader::open(in, fmt.magic, fmt.version);
	if (!opened) {
		return false;
	}

	std::uint64_t stored_fingerprint = 0;
	*opened & stored_fingerprint;
	return opened->valid() && stored_fingerprint == schema_fingerprint<T>();
}

auto gse::replace_baked_file(const std::filesystem::path& temporary, const std::filesystem::path& destination) -> asset_result {
#ifdef _WIN32
	if (win32::MoveFileExW(
		temporary.c_str(),
		destination.c_str(),
		win32::movefile_replace_existing | win32::movefile_write_through
		) == 0) {
		const auto code = static_cast<int>(win32::GetLastError());
		std::error_code cleanup;
		std::filesystem::remove(temporary, cleanup);
		return std::unexpected(asset_error{
			.code = asset_error_code::io_failure,
			.path = destination,
			.detail = std::system_category().message(code)
		});
	}
#else
	std::error_code ec;
	std::filesystem::rename(temporary, destination, ec);
	if (ec) {
		std::error_code cleanup;
		std::filesystem::remove(temporary, cleanup);
		return std::unexpected(asset_error{
			.code = asset_error_code::io_failure,
			.path = destination,
			.detail = ec.message()
		});
	}
#endif
	return {};
}