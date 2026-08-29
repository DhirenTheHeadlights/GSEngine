export module gse.assets:catalog;

import std;
import gse.config;
import gse.fs;

import :asset_format;

export namespace gse::asset {
	template <typename T>
	concept has_baked_asset = requires { typename T::baked; } && has_asset_format<typename T::baked>;

	struct catalog_record {
		std::filesystem::path baked_path;
		std::optional<std::filesystem::path> source_path;
		std::string tag;
		bool baked_exists = false;
	};

	template <has_baked_asset T>
	auto catalog_for() -> std::expected<std::vector<catalog_record>, asset_error>;

	template <has_baked_asset T>
	auto record_for(
		const std::filesystem::path& baked_path
	) -> std::expected<std::optional<catalog_record>, asset_error>;
}

template <gse::asset::has_baked_asset T>
auto gse::asset::catalog_for() -> std::expected<std::vector<catalog_record>, asset_error> {
	constexpr auto fmt = format_of<typename T::baked>();
	std::unordered_map<std::filesystem::path, catalog_record> records;
	try {
		for (const config::content_root& root : config::content_roots()) {
			if (!fmt.source_dir.empty() && !fmt.source_exts.empty()) {
				const auto source_root = root.source / fmt.source_dir;
				if (std::filesystem::exists(source_root)) {
					for (const auto& entry : std::filesystem::recursive_directory_iterator(source_root)) {
						if (!entry.is_regular_file()) {
							continue;
						}
						const auto extension = entry.path().extension().native_encoded_string();
						if (std::ranges::find(fmt.source_exts, extension) == fmt.source_exts.end()) {
							continue;
						}

						auto relative = std::filesystem::relative(entry.path(), source_root);
						relative.replace_extension(std::string(fmt.baked_ext));
						const auto baked_path = root.baked / fmt.baked_dir / relative;
						auto [it, inserted] = records.emplace(
							baked_path,
							catalog_record{
								.baked_path = baked_path,
								.source_path = entry.path(),
								.tag = config::asset_tag(baked_path),
								.baked_exists = std::filesystem::exists(baked_path)
							}
						);
						if (!inserted && it->second.source_path && it->second.source_path != entry.path()) {
							return std::unexpected(asset_error{
								.code = asset_error_code::ambiguous_source,
								.path = baked_path,
								.detail = std::format(
									"Both '{}' and '{}' map to this baked asset",
									it->second.source_path->generic_display_string(),
									entry.path().generic_display_string()
								)
							});
						}
						if (!inserted && !it->second.source_path) {
							it->second.source_path = entry.path();
						}
					}
				}
			}

			const auto baked_root = root.baked / fmt.baked_dir;
			if (!std::filesystem::exists(baked_root)) {
				continue;
			}
			for (const auto& entry : std::filesystem::recursive_directory_iterator(baked_root)) {
				if (!entry.is_regular_file() || entry.path().extension().native_encoded_string() != fmt.baked_ext) {
					continue;
				}
				auto record = records.try_emplace(
					entry.path(),
					catalog_record{
						.baked_path = entry.path(),
						.source_path = std::nullopt,
						.tag = config::asset_tag(entry.path()),
						.baked_exists = true
					}
				).first;
				record->second.baked_exists = true;
			}
		}

		std::vector<catalog_record> result;
		result.reserve(records.size());
		std::unordered_map<std::string, std::filesystem::path> paths_by_tag;
		for (auto& record : std::views::values(records)) {
			auto [existing, inserted] = paths_by_tag.emplace(record.tag, record.baked_path);
			if (!inserted && existing->second != record.baked_path) {
				return std::unexpected(asset_error{
					.code = asset_error_code::ambiguous_source,
					.path = record.baked_path,
					.detail = std::format(
						"Both '{}' and '{}' publish the asset tag '{}'",
						existing->second.generic_display_string(),
						record.baked_path.generic_display_string(),
						record.tag
					)
				});
			}
			result.push_back(std::move(record));
		}
		std::ranges::sort(
			result,
			{},
			[](const catalog_record& record) {
				return record.baked_path.generic_native_encoded_string();
			}
		);
		return result;
	}
	catch (const std::filesystem::filesystem_error& error) {
		return std::unexpected(asset_error{
			.code = asset_error_code::io_failure,
			.path = error.path1(),
			.detail = error.what()
		});
	}
	catch (const std::exception& error) {
		return std::unexpected(asset_error{
			.code = asset_error_code::io_failure,
			.path = {},
			.detail = error.what()
		});
	}
	catch (...) {
		return std::unexpected(asset_error{
			.code = asset_error_code::io_failure,
			.path = {},
			.detail = "Asset catalog construction failed"
		});
	}
}

template <gse::asset::has_baked_asset T>
auto gse::asset::record_for(const std::filesystem::path& baked_path) -> std::expected<std::optional<catalog_record>, asset_error> {
	auto catalog = catalog_for<T>();
	if (!catalog) {
		return std::unexpected(std::move(catalog.error()));
	}
	const auto found = std::ranges::find(*catalog, baked_path, &catalog_record::baked_path);
	if (found == catalog->end()) {
		return std::optional<catalog_record>{};
	}
	return std::optional<catalog_record>{ std::move(*found) };
}
