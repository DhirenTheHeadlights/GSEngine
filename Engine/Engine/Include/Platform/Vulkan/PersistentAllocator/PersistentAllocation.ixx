export module gse.platform.vulkan:persistent_allocation;

import std;
import vulkan_hpp;

import gse.assert;
import gse.utility;

export namespace gse::vulkan {
	struct sub_allocation {
		vk::DeviceSize offset;
		vk::DeviceSize size;
		bool in_use = false;
	};

	class persistent_allocation final : non_copyable {
	public:
		persistent_allocation() = default;

		persistent_allocation(
			vk::DeviceMemory memory,
			vk::DeviceSize size,
			vk::DeviceSize offset, 
			void* mapped, 
			sub_allocation* owner
		);

		~persistent_allocation() override;

		persistent_allocation(
			persistent_allocation&& other
		) noexcept;

		auto operator=(persistent_allocation&& other) noexcept -> persistent_allocation&;

		[[nodiscard]] auto memory() const -> vk::DeviceMemory { return m_memory; }
		[[nodiscard]] auto size() const -> vk::DeviceSize { return m_size; }
		[[nodiscard]] auto offset() const -> vk::DeviceSize { return m_offset; }
		[[nodiscard]] auto mapped() const -> void* { return m_mapped; }
		[[nodiscard]] auto owner() const -> sub_allocation* { return m_owner; }
	private:
		vk::DeviceMemory m_memory = nullptr;
		vk::DeviceSize m_size = 0, m_offset = 0;
		void* m_mapped = nullptr;
		sub_allocation* m_owner = nullptr;
	};
}

gse::vulkan::persistent_allocation::persistent_allocation(const vk::DeviceMemory memory, const vk::DeviceSize size, const vk::DeviceSize offset, void* mapped, sub_allocation* owner):
	m_memory(memory),
	m_size(size),
	m_offset(offset),
	m_mapped(mapped),
	m_owner(owner) {}

gse::vulkan::persistent_allocation::~persistent_allocation() {
	
}

gse::vulkan::persistent_allocation::persistent_allocation(persistent_allocation&& other) noexcept:
	m_memory(other.m_memory),
	m_size(other.m_size),
	m_offset(other.m_offset),
	m_mapped(other.m_mapped),
	m_owner(other.m_owner) {
	other.m_owner = nullptr;
}

auto gse::vulkan::persistent_allocation::operator=(persistent_allocation&& other) noexcept -> persistent_allocation& {
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
