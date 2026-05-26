export module gse.vulkan:aftermath;

import std;
import vulkan;

import gse.core;
import gse.math;

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

		struct data {
			settings cfg;
			bool enabled = false;
			vk::DeviceDiagnosticsConfigCreateInfoNV diag_config{};
			std::uint64_t dump_counter = 0;
			std::string last_dump_stem;
		};

		~aftermath();

		aftermath(
			aftermath&&
		) noexcept = default;

		auto operator=(
			aftermath&&
		) noexcept -> aftermath& = default;

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
		aftermath();

		std::unique_ptr<data> m_state;
	};
}
