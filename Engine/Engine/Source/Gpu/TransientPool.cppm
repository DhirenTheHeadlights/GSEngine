export module gse.gpu:transient_pool;

import std;

import :handles;
import :types;
import :vulkan_image;
import :vulkan_buffer;
import :vulkan_device;
import :vulkan_allocation;
import :aliases;
import :device;

import gse.assert;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;
import gse.meta;

export namespace gse::gpu {
	struct transient_image_desc {
		vec2u extent = { 1, 1 };
		image_format format = image_format::r8g8b8a8_unorm;
		image_usage usage = image_flag::sampled;
		image_view_type view = image_view_type::e2d;
		image_layout layout = image_layout::general;
		std::vector<id> used_by;
		std::string tag;
	};

	struct transient_buffer_desc {
		std::size_t size = 0;
		buffer_usage usage = buffer_flag::storage;
		std::vector<id> used_by;
		std::string tag;
	};

	struct transient_image_handle {
		std::uint64_t key = 0;

		constexpr auto operator==(
			const transient_image_handle&
		) const -> bool = default;

		explicit constexpr operator bool() const {
			return key != 0;
		}
	};

	struct transient_buffer_handle {
		std::uint64_t key = 0;

		constexpr auto operator==(
			const transient_buffer_handle&
		) const -> bool = default;

		explicit constexpr operator bool() const {
			return key != 0;
		}
	};

	struct [[= same_frame_channel]] transient_image_request {
		transient_image_handle handle;
		transient_image_desc desc;
	};

	struct [[= same_frame_channel]] transient_buffer_request {
		transient_buffer_handle handle;
		transient_buffer_desc desc;
	};

	[[nodiscard]] auto transient_image(
		const frame_context& ctx,
		transient_image_desc desc
	) -> transient_image_handle;

	[[nodiscard]] auto transient_buffer(
		const frame_context& ctx,
		transient_buffer_desc desc
	) -> transient_buffer_handle;

	struct transient_image_allocation {
		std::unique_ptr<gpu::image> resource;
		image_aspect_flags aspects;
		image_format format = image_format::r8g8b8a8_unorm;
		image_layout layout = image_layout::general;
		std::uint32_t color = 0;
		std::uint32_t first_pass = 0;
		std::uint32_t last_pass = 0;
	};

	struct transient_buffer_allocation {
		std::unique_ptr<gpu::buffer> resource;
		std::uint32_t color = 0;
		std::uint32_t first_pass = 0;
		std::uint32_t last_pass = 0;
	};

	class transient_pool : public non_copyable {
	public:
		explicit transient_pool(
			gpu::device& dev
		);

		~transient_pool() override;

		transient_pool(
			transient_pool&& other
		) noexcept;

		auto operator=(
			transient_pool&& other
		) noexcept -> transient_pool&;

		auto plan(
			std::uint32_t frame_idx,
			std::span<const transient_image_request> image_requests,
			std::span<const transient_buffer_request> buffer_requests,
			std::span<const id> pass_kind_order
		) -> void;

		auto reset() -> void;

		[[nodiscard]] auto resolve_image(
			transient_image_handle h
		) const -> const image*;

		[[nodiscard]] auto resolve_buffer(
			transient_buffer_handle h
		) const -> const buffer*;

		struct transient_image_info {
			const image* resource = nullptr;
			image_aspect_flags aspects;
			image_layout target_layout = image_layout::general;
			image_format format = image_format::r8g8b8a8_unorm;
		};

		[[nodiscard]] auto transient_images() const -> std::vector<transient_image_info>;

	private:
		struct memory_block {
			vulkan::device_memory_handle memory;
			gpu::device_size size = 0;
			std::uint32_t memory_type_index = 0;
		};

		struct frame_state {
			std::unordered_map<std::uint64_t, transient_image_allocation> images;
			std::unordered_map<std::uint64_t, transient_buffer_allocation> buffers;
			std::vector<memory_block> blocks;
		};

		auto ensure_block_for_color(
			frame_state& slot,
			std::uint32_t color,
			gpu::device_size required_size,
			std::uint32_t memory_type_mask
		) -> void;

		auto free_slot_resources(
			frame_state& slot
		) -> void;

		auto free_slot_memory(
			frame_state& slot
		) -> void;

		gpu::device* m_device = nullptr;
		per_frame_resource<frame_state> m_slots;
		std::uint32_t m_current_frame = 0;
	};
}
