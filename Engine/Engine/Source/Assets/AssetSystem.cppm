export module gse.assets:asset_system;

import std;
import gse.core;
import gse.config;
import gse.log;
import gse.containers;
import gse.concurrency;
import gse.fs;
import gse.meta;
import gse.assert;

import :asset_format;
import :boot_critical;
import :catalog;
import :resource_handle;
import :resource_loader;
import :registry;

export namespace gse::asset {
	template <typename T>
	concept loadable = requires(T t, load_ctx& ctx) {
		{ t.load(ctx) } -> std::same_as<async::task<asset_result>>;
	};

	template <typename T>
	concept has_compile_path = has_baked_asset<T> &&
		requires(const std::filesystem::path& src, typename T::baked& b) {
			{ bake(src, b) } -> std::convertible_to<bool>;
		};

	template <has_compile_path T>
	auto bake_to_disk(
		const std::filesystem::path& src,
		const std::filesystem::path& dst
	) -> asset_result;

	template <has_compile_path T>
	auto needs_recompile(
		const std::filesystem::path& src,
		const std::filesystem::path& dst
	) -> std::expected<bool, asset_error>;

	template <typename T>
	auto enumerate_resources() -> std::vector<std::string>;

	template <typename T>
	auto recompile_if_stale(
		const std::filesystem::path& baked_path
	) -> asset_result;

	template <typename T>
	auto setup_hot_reload_for(
		data& d
	) -> void;

	template <typename T>
	auto discover_baked(
		data& d
	) -> asset_result;

	struct compile_result {
		std::size_t success_count = 0;
		std::size_t failure_count = 0;
		std::size_t skipped_count = 0;

		auto operator+=(
			const compile_result& other
		) -> compile_result&;
	};

	struct missing_built_in {
		std::string tag;
		std::filesystem::path resolved;
	};

	template <typename T>
	auto collect_missing_built_ins(
		std::vector<missing_built_in>& out
	) -> void;

	template <typename... Ts>
	class system : public non_copyable {
	public:
		explicit system(
			data& d
		);

		~system() = default;

		system(
			system&&
		) noexcept = default;

		auto operator=(
			system&&
		) noexcept -> system& = default;

		template <typename T>
		auto compile() -> compile_result;

		auto compile_boot_critical() -> compile_result;

		auto register_loaders() -> void;

		auto discover_baked() -> asset_result;

		auto install_hot_reload_fns() -> void;

		auto verify_built_ins() -> void;

	private:
		data* m_data;
	};

	template <typename Pack>
	using system_for = typename Pack::template apply<system>;
}

auto gse::asset::compile_result::operator+=(const compile_result& other) -> compile_result& {
	success_count += other.success_count;
	failure_count += other.failure_count;
	skipped_count += other.skipped_count;
	return *this;
}

template <gse::asset::has_compile_path T>
auto gse::asset::bake_to_disk(const std::filesystem::path& src, const std::filesystem::path& dst) -> asset_result {
	typename T::baked b{};
	try {
		if (!bake(src, b)) {
			return std::unexpected(asset_error{
				.code = asset_error_code::load_failure,
				.path = src,
				.detail = "Asset compiler rejected the source file"
			});
		}
	}
	catch (const std::exception& error) {
		return std::unexpected(asset_error{
			.code = asset_error_code::load_failure,
			.path = src,
			.detail = error.what()
		});
	}
	catch (...) {
		return std::unexpected(asset_error{
			.code = asset_error_code::load_failure,
			.path = src,
			.detail = "Asset compiler failed"
		});
	}
	auto written = write_baked(dst, b);
	if (!written) {
		return written;
	}
	log::println(log::category::assets, "Asset compiled: {}", dst.filename().generic_display_string());
	return {};
}

template <gse::asset::has_compile_path T>
auto gse::asset::needs_recompile(const std::filesystem::path& src, const std::filesystem::path& dst) -> std::expected<bool, asset_error> {
	try {
		if (!std::filesystem::exists(dst)) {
			return true;
		}
		constexpr auto fmt = format_of<typename T::baked>();
		const auto dst_time = std::filesystem::last_write_time(dst);
		if (std::filesystem::last_write_time(src) > dst_time) {
			return true;
		}
		if constexpr (fmt.meta_sidecar) {
			const auto meta = src.parent_path() / (src.stem().native_encoded_string() + ".meta");
			if (std::filesystem::exists(meta) && std::filesystem::last_write_time(meta) > dst_time) {
				return true;
			}
		}
		std::ifstream in(dst, std::ios::binary);
		if (!in.is_open()) {
			return true;
		}
		std::uint32_t header[3]{};
		in.read(reinterpret_cast<char*>(header), sizeof(header));
		if (!in.good() || header[0] != fmt.magic || header[1] != fmt.version || header[2] != archive_format_epoch) {
			return true;
		}
		return false;
	}
	catch (const std::filesystem::filesystem_error& error) {
		return std::unexpected(asset_error{
			.code = asset_error_code::io_failure,
			.path = error.path1(),
			.detail = error.what()
		});
	}
}

template <typename T>
auto gse::asset::collect_missing_built_ins(std::vector<missing_built_in>& out) -> void {
	if constexpr (requires { typename T::baked; }) {
		constexpr auto fmt = format_of<typename T::baked>();
		if (fmt.built_ins.empty()) {
			return;
		}

		const bool installed = config::mode() == config::run_mode::installed;

		for (const std::string_view file : fmt.built_ins) {
			const std::filesystem::path source_file(file);
			const auto stem = source_file.stem().native_encoded_string();
			const auto baked = config::baked_resource_path() / fmt.baked_dir / (stem + std::string(fmt.baked_ext));
			const auto source = config::resource_path() / fmt.source_dir / source_file;
			const auto& resolved = installed ? baked : source;

			if (std::filesystem::exists(resolved)) {
				continue;
			}

			out.push_back({
				.tag = std::string(config::engine_asset_prefix) + std::string(fmt.baked_dir) + "/" + stem,
				.resolved = resolved
			});
		}
	}
}

template <typename... Ts>
auto gse::asset::system<Ts...>::verify_built_ins() -> void {
	std::vector<missing_built_in> missing;
	(collect_missing_built_ins<Ts>(missing), ...);

	if (missing.empty()) {
		log::println(log::category::assets, "Built-in assets verified");
		return;
	}

	std::string detail;
	for (const missing_built_in& entry : missing) {
		detail += std::format("\n  {} -> {}", entry.tag, entry.resolved.generic_display_string());
	}

	assert(
		false,
		"{} required engine built-in asset(s) missing from this install:{}",
		missing.size(),
		detail
	);
}

template <typename T>
auto gse::asset::enumerate_resources() -> std::vector<std::string> {
	std::vector<std::string> result;
	if constexpr (!requires { typename T::baked; }) {
		return result;
	}
	else {
		auto catalog = catalog_for<T>();
		if (!catalog) {
			return result;
		}
		for (const catalog_record& record : *catalog) {
			if (record.baked_exists) {
				result.push_back(record.tag);
			}
		}
		std::ranges::sort(result);
		return result;
	}
}

template <typename T>
auto gse::asset::recompile_if_stale(const std::filesystem::path& baked_path) -> asset_result {
	if constexpr (!has_compile_path<T>) {
		return {};
	}
	else {
		auto found = record_for<T>(baked_path);
		if (!found) {
			return std::unexpected(std::move(found.error()));
		}
		if (!*found || !(*found)->source_path) {
			return {};
		}
		const auto& source = *(*found)->source_path;
		auto stale = needs_recompile<T>(source, baked_path);
		if (!stale) {
			return std::unexpected(std::move(stale.error()));
		}
		return *stale ? bake_to_disk<T>(source, baked_path) : asset_result{};
	}
}

template <typename T>
auto gse::asset::discover_baked(data& d) -> asset_result {
	if constexpr (!loadable<T> || !has_baked_asset<T>) {
		return {};
	}
	else {
		auto catalog = catalog_for<T>();
		if (!catalog) {
			return std::unexpected(std::move(catalog.error()));
		}
		for (const catalog_record& record : *catalog) {
			if (auto queued = queue_by_path<T>(d, record.baked_path); !queued) {
				return queued;
			}
		}
		return {};
	}
}

template <typename T>
auto gse::asset::setup_hot_reload_for(data& d) -> void {
	if constexpr (!has_compile_path<T>) {
		return;
	}
	else {
		constexpr auto fmt = format_of<typename T::baked>();
		for (const config::content_root& root : config::content_roots()) {
			const auto source_root = root.source / fmt.source_dir;
			if (!std::filesystem::exists(source_root)) {
				continue;
			}
			std::vector<std::string> exts(fmt.source_exts.begin(), fmt.source_exts.end());
			d.watcher.watch_directory(
				source_root,
				[&d, source_base = root.source, baked_base = root.baked](const std::filesystem::path& changed_file) {
					constexpr auto fmt = format_of<typename T::baked>();
					auto rel = std::filesystem::relative(changed_file, source_base / fmt.source_dir);
					rel.replace_extension(std::string(fmt.baked_ext));
					const auto dst = baked_base / fmt.baked_dir / rel;
					auto record = record_for<T>(dst);
					if (!record) {
						log::println(
							log::level::error,
							log::category::assets,
							"Hot reload rejected source: {}",
							record.error().detail
						);
						return;
					}
					auto baked = bake_to_disk<T>(changed_file, dst);
					if (baked) {
						log::println(log::category::assets, "Hot reload recompiled: {}", changed_file.filename().generic_display_string());
						if constexpr (loadable<T>) {
							if (auto it = d.resource_loaders.find(id_of<T>()); it != d.resource_loaders.end()) {
								if (auto queued = queue_by_path<T>(d, dst); !queued) {
									log::println(
										log::level::warning,
										log::category::assets,
										"Hot reload failed to queue resource: {}",
										queued.error().detail
									);
								}
							}
						}
					}
					else {
						log::println(
							log::level::warning,
							log::category::assets,
							"Hot reload failed to recompile: {}",
							baked.error().detail
						);
					}
				},
				exts,
				true
			);
		}
	}
}

template <typename... Ts>
gse::asset::system<Ts...>::system(data& d) : m_data(&d) {}

template <typename... Ts>
auto gse::asset::system<Ts...>::register_loaders() -> void {
	auto try_register = [this]<typename T>() {
		if constexpr (loadable<T>) {
			add_loader<T>(*m_data);
			set_pre_load_fn<T>(*m_data, [](const std::filesystem::path& baked_path) -> asset_result {
				return recompile_if_stale<T>(baked_path);
			});
		}
	};
	(try_register.template operator()<Ts>(), ...);
}

template <typename... Ts>
auto gse::asset::system<Ts...>::discover_baked() -> asset_result {
	asset_result result{};
	auto discover_one = [this, &result]<typename T>() {
		if (!result) {
			return;
		}
		result = asset::discover_baked<T>(*m_data);
	};
	(discover_one.template operator()<Ts>(), ...);
	return result;
}

template <typename... Ts>
auto gse::asset::system<Ts...>::install_hot_reload_fns() -> void {
	auto* d = m_data;
	d->enable_hot_reload_fn = [d] {
		(setup_hot_reload_for<Ts>(*d), ...);
	};
	d->disable_hot_reload_fn = [d] {
		d->watcher.clear();
	};
}

template <typename... Ts>
template <typename T>
auto gse::asset::system<Ts...>::compile() -> compile_result {
	compile_result result{};

	if constexpr (!has_compile_path<T>) {
		return result;
	}
	else {
		auto catalog = catalog_for<T>();
		if (!catalog) {
			log::println(
				log::level::error,
				log::category::assets,
				"Unable to catalog {} assets: {}",
				std::meta::identifier_of(^^T),
				catalog.error().detail
			);
			++result.failure_count;
			return result;
		}

		for (const catalog_record& record : *catalog) {
			if (!record.source_path) {
				continue;
			}

			bool compiled = false;
			auto stale = needs_recompile<T>(*record.source_path, record.baked_path);
			if (!stale) {
				log::println(
					log::level::error,
					log::category::assets,
					"Unable to inspect {}: {}",
					record.source_path->generic_display_string(),
					stale.error().detail
				);
				++result.failure_count;
				continue;
			}
			if (!*stale) {
				++result.skipped_count;
			}
			else if (auto baked = bake_to_disk<T>(*record.source_path, record.baked_path); baked) {
				++result.success_count;
				compiled = true;
			}
			else {
				log::println(
					log::level::error,
					log::category::assets,
					"Unable to compile {}: {}",
					record.source_path->generic_display_string(),
					baked.error().detail
				);
				++result.failure_count;
				continue;
			}

			if constexpr (loadable<T>) {
				if (compiled) {
					if (auto queued = queue_by_path<T>(*m_data, record.baked_path); !queued) {
						log::println(
							log::level::warning,
							log::category::assets,
							"Compiled asset could not be queued: {}",
							queued.error().detail
						);
					}
				}
			}
		}

		return result;
	}
}

template <typename... Ts>
auto gse::asset::system<Ts...>::compile_boot_critical() -> compile_result {
	compile_result total{};
	auto try_one = [&]<typename T>() {
		if constexpr (has_annotation<boot_critical>(^^T)) {
			total += compile<T>();
		}
	};
	(try_one.template operator()<Ts>(), ...);
	return total;
}
