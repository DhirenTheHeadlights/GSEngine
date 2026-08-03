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
import :resource_handle;
import :resource_loader;
import :registry;

export namespace gse::asset {
	template <typename T>
	concept loadable = requires(T t, load_ctx& ctx) { t.load(ctx); };

	template <typename T>
	concept has_compile_path = requires { typename T::baked; } && has_asset_format<typename T::baked> &&
		requires(const std::filesystem::path& src, typename T::baked& b) {
			{ bake(src, b) } -> std::convertible_to<bool>;
		};

	template <has_compile_path T>
	auto bake_to_disk(
		const std::filesystem::path& src,
		const std::filesystem::path& dst
	) -> bool;

	template <has_compile_path T>
	auto needs_recompile(
		const std::filesystem::path& src,
		const std::filesystem::path& dst
	) -> bool;

	template <typename T>
	auto enumerate_resources() -> std::vector<std::string>;

	template <typename T>
	auto recompile_if_stale(
		const std::filesystem::path& baked_path
	) -> bool;

	template <typename T>
	auto setup_hot_reload_for(
		data& d
	) -> void;

	struct compile_result {
		std::size_t success_count = 0;
		std::size_t failure_count = 0;
		std::size_t skipped_count = 0;

		auto operator+=(
			const compile_result& other
		) -> compile_result&;
	};

	struct compile_progress {
		std::function<void(std::uint32_t done, std::uint32_t total)> on_tick;
		std::uint32_t done = 0;
		std::uint32_t total = 0;
	};

	template <typename T>
	auto count_compile_work() -> std::uint32_t;

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
		auto compile(
			compile_progress* progress = nullptr
		) -> compile_result;

		auto compile_all(
			compile_progress* progress = nullptr
		) -> compile_result;

		auto compile_boot_critical(
			compile_progress* progress = nullptr
		) -> compile_result;

		auto compile_non_boot_critical(
			compile_progress* progress = nullptr
		) -> compile_result;

		auto register_loaders() -> void;

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
auto gse::asset::bake_to_disk(const std::filesystem::path& src, const std::filesystem::path& dst) -> bool {
	constexpr auto fmt = format_of<typename T::baked>();
	typename T::baked b{};
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
	log::println(log::category::assets, "Asset compiled: {}", dst.filename().display_string());
	return true;
}

template <gse::asset::has_compile_path T>
auto gse::asset::needs_recompile(const std::filesystem::path& src, const std::filesystem::path& dst) -> bool {
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

template <typename T>
auto gse::asset::count_compile_work() -> std::uint32_t {
	if constexpr (!has_compile_path<T>) {
		log::println(log::category::assets,
					 "count_compile_work<{}>: 0 (no compile path)",
					 std::meta::identifier_of(^^T));
		return 0;
	}
	else {
		constexpr auto fmt = format_of<typename T::baked>();
		std::uint32_t count = 0;

		for (const config::content_root& root : config::content_roots()) {
			const auto source_root = root.source / fmt.source_dir;
			if (!std::filesystem::exists(source_root)) {
				log::println(
					log::category::assets,
					"count_compile_work<{}>: 0 (source_root missing: {})",
					std::meta::identifier_of(^^T),
					source_root.display_string()
				);
				continue;
			}

			for (const auto& entry : std::filesystem::recursive_directory_iterator(source_root)) {
				if (!entry.is_regular_file()) {
					continue;
				}
				const auto ext = entry.path().extension().native_encoded_string();
				if (std::ranges::find(fmt.source_exts, ext) == fmt.source_exts.end()) {
					continue;
				}
				++count;
			}
		}

		log::println(log::category::assets, "count_compile_work<{}>: {}", std::meta::identifier_of(^^T), count);
		return count;
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
		detail += std::format("\n  {} -> {}", entry.tag, entry.resolved.display_string());
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
		constexpr auto fmt = format_of<typename T::baked>();

		for (const config::content_root& root : config::content_roots()) {
			const auto dir_path = root.baked / fmt.baked_dir;
			if (!std::filesystem::exists(dir_path)) {
				continue;
			}

			for (const auto& entry : std::filesystem::directory_iterator(dir_path)) {
				if (!entry.is_regular_file()) {
					continue;
				}
				if (entry.path().extension().native_encoded_string() != fmt.baked_ext) {
					continue;
				}
				result.push_back(config::asset_tag(entry.path()));
			}
		}

		std::ranges::sort(result);
		return result;
	}
}

template <typename T>
auto gse::asset::recompile_if_stale(const std::filesystem::path& baked_path) -> bool {
	if constexpr (!has_compile_path<T>) {
		return false;
	}
	else {
		constexpr auto fmt = format_of<typename T::baked>();

		for (const config::content_root& root : config::content_roots()) {
			auto src_rel = baked_path.lexically_relative(root.baked / fmt.baked_dir);
			if (src_rel.empty() || *src_rel.begin() == "..") {
				continue;
			}
			src_rel.replace_extension(fmt.source_exts.empty() ? "" : std::string(fmt.source_exts.front()));
			const auto src = root.source / fmt.source_dir / src_rel;
			if (!std::filesystem::exists(src)) {
				return false;
			}
			if (!needs_recompile<T>(src, baked_path)) {
				return true;
			}
			return bake_to_disk<T>(src, baked_path);
		}

		return false;
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
					if (bake_to_disk<T>(changed_file, dst)) {
						log::println(log::category::assets, "Hot reload recompiled: {}", changed_file.filename().display_string());
						if constexpr (loadable<T>) {
							if (auto it = d.resource_loaders.find(id_of<T>()); it != d.resource_loaders.end()) {
								auto* loader = static_cast<resource::loader<T>*>(it->second.get());
								loader->queue_reload_by_path(dst);
							}
						}
					}
					else {
						log::println(
							log::level::warning,
							log::category::assets,
							"Hot reload failed to recompile: {}",
							changed_file.filename().display_string()
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
gse::asset::system<Ts...>::system(data& d) : m_data(&d) {
}

template <typename... Ts>
auto gse::asset::system<Ts...>::register_loaders() -> void {
	auto try_register = [this]<typename T>() {
		if constexpr (loadable<T>) {
			auto* loader = add_loader<T>(*m_data);
			loader->set_pre_load_fn([](const std::filesystem::path& baked_path) {
				recompile_if_stale<T>(baked_path);
			});
		}
	};
	(try_register.template operator()<Ts>(), ...);
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
auto gse::asset::system<Ts...>::compile(compile_progress* progress) -> compile_result {
	compile_result result{};

	if constexpr (!has_compile_path<T>) {
		return result;
	}
	else {
		constexpr auto fmt = format_of<typename T::baked>();

		auto tick = [progress] {
			if (progress) {
				++progress->done;
				if (progress->on_tick) {
					progress->on_tick(progress->done, progress->total);
				}
			}
		};

		for (const config::content_root& root : config::content_roots()) {
			const auto source_root = root.source / fmt.source_dir;
			const auto baked_root = root.baked / fmt.baked_dir;

			if (!std::filesystem::exists(source_root)) {
				log::println(
					log::category::assets,
					"compile<{}>: skipped (source_dir '{}' baked_dir '{}' baked_ext '{}' magic {:#x}, missing: {})",
					std::meta::identifier_of(^^T),
					fmt.source_dir,
					fmt.baked_dir,
					fmt.baked_ext,
					fmt.magic,
					source_root.display_string()
				);
				continue;
			}

			std::filesystem::create_directories(baked_root);

			for (const auto& entry : std::filesystem::recursive_directory_iterator(source_root)) {
				if (!entry.is_regular_file()) {
					continue;
				}
				const auto ext = entry.path().extension().native_encoded_string();
				if (std::ranges::find(fmt.source_exts, ext) == fmt.source_exts.end()) {
					continue;
				}

				auto rel = std::filesystem::relative(entry.path(), source_root);
				rel.replace_extension(std::string(fmt.baked_ext));
				const auto dst = baked_root / rel;

				if (!needs_recompile<T>(entry.path(), dst)) {
					++result.skipped_count;
				}
				else if (bake_to_disk<T>(entry.path(), dst)) {
					++result.success_count;
				}
				else {
					++result.failure_count;
					tick();
					continue;
				}

				tick();

				if constexpr (loadable<T>) {
					if (!std::filesystem::exists(dst)) {
						continue;
					}
					if (auto it = m_data->resource_loaders.find(id_of<T>()); it != m_data->resource_loaders.end()) {
						static_cast<resource::loader<T>*>(it->second.get())->queue_by_path(dst);
					}
				}
			}
		}

		return result;
	}
}

template <typename... Ts>
auto gse::asset::system<Ts...>::compile_all(compile_progress* progress) -> compile_result {
	if (progress) {
		progress->done = 0;
		progress->total = 0;
		((progress->total += count_compile_work<Ts>()), ...);
		if (progress->on_tick) {
			progress->on_tick(0, progress->total);
		}
	}

	compile_result total{};
	((total += compile<Ts>(progress)), ...);
	return total;
}

template <typename... Ts>
auto gse::asset::system<Ts...>::compile_boot_critical(compile_progress* progress) -> compile_result {
	if (progress) {
		progress->done = 0;
		progress->total = 0;
		auto count_one = [&]<typename T>() {
			if constexpr (has_annotation<boot_critical>(^^T)) {
				progress->total += count_compile_work<T>();
			}
		};
		(count_one.template operator()<Ts>(), ...);
		log::println(
			log::category::assets,
			"compile_boot_critical: pre-count total={} on_tick_set={}",
			progress->total,
			static_cast<bool>(progress->on_tick)
		);
		if (progress->on_tick) {
			progress->on_tick(0, progress->total);
		}
	}

	compile_result total{};
	auto try_one = [&]<typename T>() {
		if constexpr (has_annotation<boot_critical>(^^T)) {
			total += compile<T>(progress);
		}
	};
	(try_one.template operator()<Ts>(), ...);
	return total;
}

template <typename... Ts>
auto gse::asset::system<Ts...>::compile_non_boot_critical(compile_progress* progress) -> compile_result {
	if (progress) {
		progress->done = 0;
		progress->total = 0;
		auto count_one = [&]<typename T>() {
			if constexpr (!has_annotation<boot_critical>(^^T)) {
				progress->total += count_compile_work<T>();
			}
		};
		(count_one.template operator()<Ts>(), ...);
		log::println(
			log::category::assets,
			"compile_non_boot_critical: pre-count total={} on_tick_set={}",
			progress->total,
			static_cast<bool>(progress->on_tick)
		);
		if (progress->on_tick) {
			progress->on_tick(0, progress->total);
		}
	}

	compile_result total{};
	auto try_one = [&]<typename T>() {
		if constexpr (!has_annotation<boot_critical>(^^T)) {
			total += compile<T>(progress);
		}
	};
	(try_one.template operator()<Ts>(), ...);
	return total;
}
