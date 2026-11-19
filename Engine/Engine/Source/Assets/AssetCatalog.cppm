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

	const auto io_failure = [](const std::filesystem::path& path, const std::error_code& ec) {
		return std::unexpected(asset_error{
			.code = asset_error_code::io_failure,
			.path = path,
			.detail = ec.message()
		});
	};

	std::error_code ec;
	for (const config::content_root& root : config::content_roots()) {
		if (!fmt.source_dir.empty() && !fmt.source_exts.empty()) {
			const auto source_root = root.source / fmt.source_dir;
			const bool source_root_exists = std::filesystem::exists(source_root, ec);
			if (ec) {
				return io_failure(source_root, ec);
			}
			if (source_root_exists) {
				std::filesystem::recursive_directory_iterator it(
					source_root,
					std::filesystem::directory_options::none,
					ec
				);
				if (ec) {
					return io_failure(source_root, ec);
				}
				for (const std::filesystem::recursive_directory_iterator end; it != end; it.increment(ec)) {
					if (ec) {
						return io_failure(source_root, ec);
					}
					if (!it->is_regular_file(ec)) {
						if (ec) {
							return io_failure(it->path(), ec);
						}
						continue;
					}
					const auto extension = it->path().extension().native_encoded_string();
					if (std::ranges::find(fmt.source_exts, extension) == fmt.source_exts.end()) {
						continue;
					}

					auto relative = std::filesystem::relative(it->path(), source_root, ec);
					if (ec) {
						return io_failure(it->path(), ec);
					}
					relative.replace_extension(std::string(fmt.baked_ext));
					const auto baked_path = root.baked / fmt.baked_dir / relative;
					const bool baked_exists = std::filesystem::exists(baked_path, ec);
					if (ec) {
						return io_failure(baked_path, ec);
					}
					auto [entry, inserted] = records.emplace(
						baked_path,
						catalog_record{
							.baked_path = baked_path,
							.source_path = it->path(),
							.tag = config::asset_tag(baked_path),
							.baked_exists = baked_exists
						}
					);
					if (!inserted && entry->second.source_path && entry->second.source_path != it->path()) {
						return std::unexpected(asset_error{
							.code = asset_error_code::ambiguous_source,
							.path = baked_path,
							.detail = std::format(
								"Both '{}' and '{}' map to this baked asset",
								entry->second.source_path->generic_display_string(),
								it->path().generic_display_string()
							)
						});
					}
					if (!inserted && !entry->second.source_path) {
						entry->second.source_path = it->path();
					}
				}
				if (ec) {
					return io_failure(source_root, ec);
				}
			}
		}

		const auto baked_root = root.baked / fmt.baked_dir;
		if (!std::filesystem::exists(baked_root, ec)) {
			if (ec) {
				return io_failure(baked_root, ec);
			}
			continue;
		}
		std::filesystem::recursive_directory_iterator baked_it(
			baked_root,
			std::filesystem::directory_options::none,
			ec
		);
		if (ec) {
			return io_failure(baked_root, ec);
		}
		for (const std::filesystem::recursive_directory_iterator end; baked_it != end; baked_it.increment(ec)) {
			if (ec) {
				return io_failure(baked_root, ec);
			}
			if (!baked_it->is_regular_file(ec)) {
				if (ec) {
					return io_failure(baked_it->path(), ec);
				}
				continue;
			}
			if (baked_it->path().extension().native_encoded_string() != fmt.baked_ext) {
				continue;
			}
			auto record = records.try_emplace(
				baked_it->path(),
				catalog_record{
					.baked_path = baked_it->path(),
					.source_path = std::nullopt,
					.tag = config::asset_tag(baked_it->path()),
					.baked_exists = true
				}
			).first;
			record->second.baked_exists = true;
		}
		if (ec) {
			return io_failure(baked_root, ec);
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
