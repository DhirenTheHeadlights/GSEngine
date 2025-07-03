export module gse.platform.vulkan:resources;

import std;
import vulkan_hpp;

import gse.assert;

export namespace gse::vulkan::persistent_allocator {
	class allocation;
	auto free(allocation& alloc) -> void;

	struct sub_allocation {
		vk::DeviceSize offset;
		vk::DeviceSize size;
		bool in_use = false;
	};

	class allocation {
	public:
		allocation() = default;

		allocation(
			const vk::DeviceMemory memory, 
			const vk::DeviceSize size, 
			const vk::DeviceSize offset, 
			void* mapped, 
			sub_allocation* owner
		) :
			m_memory(memory),
			m_size(size),
			m_offset(offset),
			m_mapped(mapped),
			m_owner(owner) {}

		~allocation() {
			if (m_owner) {
				free(*this);
			}
		}

		allocation(const allocation&) = delete;
		auto operator=(const allocation&) -> allocation & = delete;

		allocation(
			allocation&& other
		) noexcept :
			m_memory(other.m_memory),
			m_size(other.m_size),
			m_offset(other.m_offset),
			m_mapped(other.m_mapped),
			m_owner(other.m_owner) {
			other.m_owner = nullptr;
		}

		auto operator=(allocation&& other) noexcept -> allocation& {
			if (this != &other) {
				if (m_owner) {
					free(*this);
				}

				m_memory = other.m_memory;
				m_size = other.m_size;
				m_offset = other.m_offset;
				m_mapped = other.m_mapped;
				m_owner = other.m_owner;

				other.m_owner = nullptr;
			}
			return *this;
		}

		[[nodiscard]] auto memory() const -> vk::DeviceMemory { return m_memory; }
		[[nodiscard]] auto size() const -> vk::DeviceSize { return m_size; }
		[[nodiscard]] auto offset() const -> vk::DeviceSize { return m_offset; }
		[[nodiscard]] auto mapped() const -> void* { return m_mapped; }
		[[nodiscard]] auto owner() const -> sub_allocation* { return m_owner; }
	private:
		friend auto free(allocation& alloc) -> void;

		vk::DeviceMemory m_memory = nullptr;
		vk::DeviceSize m_size = 0, m_offset = 0;
		void* m_mapped = nullptr;
		sub_allocation* m_owner = nullptr;
	};

	struct image_resource {
		vk::raii::Image image = nullptr;
		vk::raii::ImageView view = nullptr;
		vk::Format format = vk::Format::eUndefined;
		vk::ImageLayout current_layout = vk::ImageLayout::eUndefined;
		allocation allocation;
	};

	struct buffer_resource {
		vk::raii::Buffer buffer = nullptr;
		allocation allocation;
	};

	template <typename T>
	struct mapped_buffer_view {
		void* base = nullptr;
		vk::DeviceSize stride = sizeof(T);

		auto operator[](const size_t index) -> T& {
			assert(base != nullptr, "mapped_buffer_view base is null");
			return *reinterpret_cast<T*>(static_cast<std::byte*>(base) + index * stride);
		}

		auto get_offset(const size_t index) const -> vk::DeviceSize {
			return index * stride;
		}

		auto get_span(const size_t index) const -> std::span<const std::byte> {
			const auto ptr = static_cast<const std::byte*>(base) + index * stride;
			return std::span{ ptr, stride };
		}

		auto get_span(const size_t index) -> std::span<std::byte> {
			const auto ptr = static_cast<std::byte*>(base) + index * stride;
			return std::span{ ptr, stride };
		}
	};
}

export namespace gse::vulkan::transient_allocator {
	struct allocation {
		vk::DeviceMemory memory;
		vk::DeviceSize size = 0, offset = 0;
		void* mapped = nullptr;
	};
}