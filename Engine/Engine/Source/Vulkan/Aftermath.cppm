export module gse.vulkan:aftermath;

import std;
import vulkan;

import gse.aftermath;
import gse.config;
import gse.core;
import gse.log;
import gse.math;
import gse.time;

export namespace gse::vulkan {
	class aftermath final : public non_copyable {
	public:
		struct settings {
			bool resource_tracking = true;
			bool automatic_checkpoints = true;
			bool shader_debug_info = true;
			bool shader_error_reporting = true;
			std::filesystem::path dump_directory;
			std::filesystem::path shader_directory;
		};

		~aftermath();

		aftermath(
			aftermath&& other
		) noexcept;

		auto operator=(
			aftermath&& other
		) noexcept -> aftermath&;

		[[nodiscard]] static auto create(
			settings cfg
		) -> aftermath;

		[[nodiscard]] auto available() const -> bool;

		[[nodiscard]] auto required_device_extensions() const -> std::span<const char* const>;

		[[nodiscard]] auto device_create_info_pnext(
			void* next
		) -> void*;

		auto wait_for_crash_dump(
			time timeout = seconds(5)
		) -> void;

		static auto register_spirv(
			std::span<const std::uint32_t> spirv
		) -> void;

	private:
		aftermath() = default;

		bool m_owns_session = false;
		vk::DeviceDiagnosticsConfigCreateInfoNV m_diag_config{};
	};
}

namespace gse::vulkan {
	constexpr std::array k_required_device_extensions = {
		vk::NVDeviceDiagnosticCheckpointsExtensionName,
		vk::NVDeviceDiagnosticsConfigExtensionName,
	};

	struct dump_writer {
		std::filesystem::path dump_directory;
		std::filesystem::path shader_directory;
		std::uint64_t dump_counter = 0;
		std::string last_dump_stem;
		bool active = false;
	};

	inline dump_writer dumps;
	inline std::mutex shader_registry_mutex;

	auto default_dump_directory() -> std::filesystem::path;

	auto default_shader_directory() -> std::filesystem::path;

	auto write_dump_to_disk(
		const std::filesystem::path& directory,
		const std::string& stem,
		std::string_view extension,
		const void* data,
		std::size_t size
	) -> std::filesystem::path;

	auto build_diagnostics_flags(
		const aftermath::settings& cfg
	) -> vk::DeviceDiagnosticsConfigFlagsNV;

	auto on_gpu_crash_dump(
		const void* data,
		std::uint32_t size
	) -> void;

	auto on_shader_debug_info(
		const void* data,
		std::uint32_t size
	) -> void;
}

auto gse::vulkan::default_dump_directory() -> std::filesystem::path {
	return config::resource_path / "Misc" / "crash_dumps";
}

auto gse::vulkan::default_shader_directory() -> std::filesystem::path {
	return config::resource_path / "Misc" / "aftermath_shaders";
}

auto gse::vulkan::write_dump_to_disk(const std::filesystem::path& directory, const std::string& stem, std::string_view extension, const void* data, std::size_t size) -> std::filesystem::path {
	std::error_code ec;
	std::filesystem::create_directories(directory, ec);

	const auto path = directory / (stem + std::string(extension));
	std::ofstream out(path, std::ios::binary);
	if (out) {
		out.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
	}
	return path;
}

auto gse::vulkan::build_diagnostics_flags(const aftermath::settings& cfg) -> vk::DeviceDiagnosticsConfigFlagsNV {
	vk::DeviceDiagnosticsConfigFlagsNV flags{};
	if (cfg.resource_tracking) {
		flags |= vk::DeviceDiagnosticsConfigFlagBitsNV::eEnableResourceTracking;
	}
	if (cfg.automatic_checkpoints) {
		flags |= vk::DeviceDiagnosticsConfigFlagBitsNV::eEnableAutomaticCheckpoints;
	}
	if (cfg.shader_debug_info) {
		flags |= vk::DeviceDiagnosticsConfigFlagBitsNV::eEnableShaderDebugInfo;
	}
	if (cfg.shader_error_reporting) {
		flags |= vk::DeviceDiagnosticsConfigFlagBitsNV::eEnableShaderErrorReporting;
	}
	return flags;
}

auto gse::vulkan::on_gpu_crash_dump(const void* data, std::uint32_t size) -> void {
	const auto seq = dumps.dump_counter++;
	const auto stem = std::format("gse_{}_{}", system_clock::timestamp_filename(), seq);
	dumps.last_dump_stem = stem;
	const auto path = write_dump_to_disk(dumps.dump_directory, stem, ".nv-gpudmp", data, size);
	log::println(
		log::level::error,
		log::category::vulkan,
		"Aftermath crash dump written: {}",
		path.string()
	);
}

auto gse::vulkan::on_shader_debug_info(const void* data, std::uint32_t size) -> void {
	const auto stem = dumps.last_dump_stem.empty()
		? std::format(
			  "gse_{}_shaderdebug",
			  system_clock::timestamp_filename()
		  )
		: dumps.last_dump_stem + "_shaderdebug";
	write_dump_to_disk(dumps.shader_directory, stem, ".nvdbg", data, size);
}

gse::vulkan::aftermath::~aftermath() {
	if (!m_owns_session) {
		return;
	}
	wait_for_crash_dump();
	gse::aftermath::disable();
	dumps.active = false;
}

gse::vulkan::aftermath::aftermath(aftermath&& other) noexcept : m_owns_session(std::exchange(other.m_owns_session, false)), m_diag_config(other.m_diag_config) {}

auto gse::vulkan::aftermath::operator=(aftermath&& other) noexcept -> aftermath& {
	if (this != &other) {
		if (m_owns_session) {
			wait_for_crash_dump();
			gse::aftermath::disable();
			dumps.active = false;
		}
		m_owns_session = std::exchange(other.m_owns_session, false);
		m_diag_config = other.m_diag_config;
	}
	return *this;
}

auto gse::vulkan::aftermath::create(settings cfg) -> aftermath {
	if (cfg.dump_directory.empty()) {
		cfg.dump_directory = default_dump_directory();
	}
	if (cfg.shader_directory.empty()) {
		cfg.shader_directory = default_shader_directory();
	}

	aftermath tracker;
	tracker.m_diag_config.flags = build_diagnostics_flags(cfg);

	dumps = {
		.dump_directory = std::move(cfg.dump_directory),
		.shader_directory = std::move(cfg.shader_directory),
	};

	tracker.m_owns_session = gse::aftermath::enable(&on_gpu_crash_dump, &on_shader_debug_info, "GSEngine", "0.1.0");
	dumps.active = tracker.m_owns_session;

	if (tracker.m_owns_session) {
		log::println(
			log::category::vulkan,
			"Nsight Aftermath crash dumps enabled (dumps -> {}, shaders -> {})",
			dumps.dump_directory.string(),
			dumps.shader_directory.string()
		);
	}
	else if (gse::aftermath::compiled_in) {
		log::println(log::level::warning, log::category::vulkan, "Nsight Aftermath enable failed");
	}
	else {
		log::println(log::category::vulkan, "Nsight Aftermath SDK not compiled in; crash dumps disabled");
	}

	return tracker;
}

auto gse::vulkan::aftermath::available() const -> bool {
	return m_owns_session;
}

auto gse::vulkan::aftermath::required_device_extensions() const -> std::span<const char* const> {
	if (!available()) {
		return {};
	}
	return std::span<const char* const>(k_required_device_extensions.data(), k_required_device_extensions.size());
}

auto gse::vulkan::aftermath::device_create_info_pnext(void* next) -> void* {
	if (!available()) {
		return next;
	}
	m_diag_config.pNext = next;
	return &m_diag_config;
}

auto gse::vulkan::aftermath::wait_for_crash_dump(time timeout) -> void {
	if (!m_owns_session) {
		return;
	}

	const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(static_cast<std::int64_t>(timeout.as<milliseconds>()));
	while (std::chrono::steady_clock::now() < deadline) {
		const auto status = gse::aftermath::status();
		if (status == gse::aftermath::dump_status::finished || status == gse::aftermath::dump_status::not_started || status == gse::aftermath::dump_status::failed) {
			return;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}

	log::println(
		log::level::warning,
		log::category::vulkan,
		"Aftermath crash dump did not finish within {} ms",
		timeout.as<milliseconds>()
	);
}

auto gse::vulkan::aftermath::register_spirv(std::span<const std::uint32_t> spirv) -> void {
	if (!dumps.active || spirv.empty()) {
		return;
	}

	const auto aftermath_hash = gse::aftermath::shader_hash_spirv(spirv);
	const auto bytes = std::as_bytes(spirv);
	const auto filename = aftermath_hash.has_value()
		? std::format("{:016X}.spv", *aftermath_hash)
		: std::format("{:016x}.spv", std::hash<std::string_view>{}(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size())));

	const std::lock_guard lock(shader_registry_mutex);

	std::error_code ec;
	std::filesystem::create_directories(dumps.shader_directory, ec);
	const auto path = dumps.shader_directory / filename;
	if (std::filesystem::exists(path, ec)) {
		return;
	}

	std::ofstream out(path, std::ios::binary);
	if (out) {
		out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	}

	log::println(
		log::category::vulkan,
		"Aftermath shader registered: {} ({} bytes)",
		filename,
		bytes.size()
	);
}
