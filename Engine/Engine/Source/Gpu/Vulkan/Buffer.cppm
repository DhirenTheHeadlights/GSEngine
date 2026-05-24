export module gse.gpu:vulkan_buffer;

import std;

import :handles;
import :types;
import :vulkan_allocation;

import gse.assert;
import gse.math;
import gse.core;
import gse.log;

export namespace gse::vulkan {
	class device;

	template <typename Device>
	class basic_buffer final : public non_copyable {
	public:
		basic_buffer() = default;

		[[nodiscard]]
		static auto create(
			Device& dev,
			const gpu::buffer_desc& desc,
			std::string_view tag = "",
			const std::source_location& loc = std::source_location::current()
		) -> basic_buffer;

		basic_buffer(
			gpu::handle<basic_buffer<device>> buffer,
			basic_allocation<Device> allocation,
			gpu::device_size size
		);

		~basic_buffer() override;

		basic_buffer(
			basic_buffer&& other
		) noexcept;

		auto operator=(
			basic_buffer&& other
		) noexcept -> basic_buffer&;

		[[nodiscard]] auto handle() const -> gpu::handle<basic_buffer<device>>;

		[[nodiscard]] auto size_bytes() const -> gpu::device_size;

		[[nodiscard]] auto size() const -> gpu::device_size;

		auto host_write(
			const void* data,
			std::size_t bytes,
			std::size_t offset = 0
		) const -> void;

		template <typename T>
		requires(!std::is_pointer_v<T>)
		auto host_write(
			const T& src,
			std::size_t offset = 0
		) const -> void;

		auto host_zero() const -> void;

		[[nodiscard]] auto host_read() const -> std::span<const std::byte>;

		[[nodiscard]] auto host_dirty() const noexcept -> bool;

		auto clear_host_dirty() const noexcept -> void;

		[[nodiscard]] auto debug_info() const -> const allocation_debug_info&;

		[[nodiscard]] auto valid() const -> bool;

	private:
		gpu::handle<basic_buffer<device>> m_buffer;
		gpu::device_size m_size = 0;
		basic_allocation<Device> m_allocation;
		mutable std::atomic<bool> m_host_dirty{ false };
	};

	using buffer = basic_buffer<device>;

	auto register_dirty_buffer(
		const buffer* buf
	) -> void;

	auto unregister_dirty_buffer(
		const buffer* buf
	) -> void;

	template <typename T>
	auto register_dirty_buffer([[maybe_unused]] const T* buf) noexcept -> void {
	}

	template <typename T>
	auto unregister_dirty_buffer([[maybe_unused]] const T* buf) noexcept -> void {
	}

	auto drain_dirty_buffers() -> void;
}

namespace gse::vulkan {
	struct dirty_buffer_registry_state {
		std::mutex mutex;
		std::unordered_set<const buffer*> set;
	};

	auto dirty_buffer_registry() -> dirty_buffer_registry_state& {
		static dirty_buffer_registry_state state;
		return state;
	}
}

auto gse::vulkan::register_dirty_buffer(const buffer* buf) -> void {
	auto& r = dirty_buffer_registry();
	std::lock_guard lock(r.mutex);
	r.set.insert(buf);
}

auto gse::vulkan::unregister_dirty_buffer(const buffer* buf) -> void {
	auto& r = dirty_buffer_registry();
	std::lock_guard lock(r.mutex);
	r.set.erase(buf);
}

auto gse::vulkan::drain_dirty_buffers() -> void {
	std::unordered_set<const buffer*> stale;
	{
		auto& r = dirty_buffer_registry();
		std::lock_guard lock(r.mutex);
		if (r.set.empty()) {
			return;
		}
		stale = std::move(r.set);
		r.set.clear();
	}
	std::string detail;
	for (const auto* b : stale) {
		const auto& di = b->debug_info();
		const auto file = std::filesystem::path(di.creation_location.file_name()).filename().string();
		detail += std::format(
			"\n  #{} [{}] {} bytes @ {}:{}",
			di.allocation_id,
			di.tag,
			b->size_bytes(),
			file,
			di.creation_location.line()
		);
	}
	log::println(
		log::level::warning,
		log::category::vulkan,
		"render_graph: {} buffer(s) host_write'd but not consumed by any pass this frame; clearing dirty flags:{}",
		stale.size(),
		detail
	);
	for (const auto* b : stale) {
		b->clear_host_dirty();
	}
}

template <typename Device>
gse::vulkan::basic_buffer<Device>::basic_buffer(const gpu::handle<basic_buffer<device>> buffer, basic_allocation<Device> allocation, const gpu::device_size size)
	: m_buffer(buffer), m_size(size), m_allocation(std::move(allocation)) {
}

template <typename Device>
auto gse::vulkan::basic_buffer<Device>::create(Device& dev, const gpu::buffer_desc& desc, const std::string_view tag, const std::source_location& loc) -> basic_buffer {
	return dev.create_buffer(
		gpu::buffer_create_info{
			.size = desc.size,
			.usage = desc.usage,
		},
		desc.data,
		tag,
		loc
	);
}

template <typename Device>
gse::vulkan::basic_buffer<Device>::~basic_buffer() {
	if (m_host_dirty.load(std::memory_order_acquire)) {
		unregister_dirty_buffer(this);
	}
	if (m_allocation.device() && m_buffer) {
		m_allocation.device()->destroy_buffer(m_buffer);
	}
}

template <typename Device>
gse::vulkan::basic_buffer<Device>::basic_buffer(basic_buffer&& other) noexcept
	: m_buffer(other.m_buffer), m_size(other.m_size), m_allocation(std::move(other.m_allocation)), m_host_dirty(other.m_host_dirty.load(std::memory_order_relaxed)) {
	other.m_buffer = {};
	other.m_size = 0;
	const bool was_dirty = other.m_host_dirty.exchange(false, std::memory_order_acq_rel);
	if (was_dirty) {
		unregister_dirty_buffer(&other);
		register_dirty_buffer(this);
	}
}

template <typename Device>
auto gse::vulkan::basic_buffer<Device>::operator=(basic_buffer&& other) noexcept -> basic_buffer& {
	if (this != &other) {
		if (m_host_dirty.load(std::memory_order_acquire)) {
			unregister_dirty_buffer(this);
		}
		if (m_allocation.device() && m_buffer) {
			m_allocation.device()->destroy_buffer(m_buffer);
		}

		m_buffer = other.m_buffer;
		m_size = other.m_size;
		m_allocation = std::move(other.m_allocation);
		m_host_dirty.store(
			other.m_host_dirty.load(std::memory_order_relaxed),
			std::memory_order_relaxed
		);
		other.m_buffer = {};
		other.m_size = 0;
		const bool was_dirty = other.m_host_dirty.exchange(false, std::memory_order_acq_rel);
		if (was_dirty) {
			unregister_dirty_buffer(&other);
			register_dirty_buffer(this);
		}
	}
	return *this;
}

template <typename Device>
auto gse::vulkan::basic_buffer<Device>::handle() const -> gpu::handle<basic_buffer<device>> {
	return m_buffer;
}

template <typename Device>
auto gse::vulkan::basic_buffer<Device>::size_bytes() const -> gpu::device_size {
	return m_size;
}

template <typename Device>
auto gse::vulkan::basic_buffer<Device>::size() const -> gpu::device_size {
	return m_size;
}

template <typename Device>
auto gse::vulkan::basic_buffer<Device>::valid() const -> bool {
	return static_cast<bool>(m_buffer);
}

template <typename Device>
auto gse::vulkan::basic_buffer<Device>::debug_info() const -> const allocation_debug_info& {
	return m_allocation.debug_info();
}

template <typename Device>
auto gse::vulkan::basic_buffer<Device>::host_write(const void* data, const std::size_t bytes, const std::size_t offset) const -> void {
	assert(m_allocation.mapped(), "Buffer must be persistently mapped to host_write");
	assert(offset + bytes <= m_size, "host_write extends past buffer size");

	gse::memcpy(m_allocation.mapped() + offset, data, bytes);
	const bool was_dirty = m_host_dirty.exchange(true, std::memory_order_acq_rel);
	if (!was_dirty) {
		register_dirty_buffer(this);
	}
}

template <typename Device>
template <typename T>
requires(!std::is_pointer_v<T>)
auto gse::vulkan::basic_buffer<Device>::host_write(const T& src, const std::size_t offset) const -> void {
	if constexpr (std::ranges::contiguous_range<T>) {
		host_write(
			std::ranges::data(src),
			std::ranges::size(src) * sizeof(std::ranges::range_value_t<T>),
			offset
		);
	}
	else {
		static_assert(std::is_trivially_copyable_v<T>, "host_write requires a trivially copyable type");
		host_write(std::addressof(src), sizeof(T), offset);
	}
}

template <typename Device>
auto gse::vulkan::basic_buffer<Device>::host_zero() const -> void {
	assert(m_allocation.mapped(), "Buffer must be persistently mapped to host_zero");
	std::memset(m_allocation.mapped(), 0, m_size);
	const bool was_dirty = m_host_dirty.exchange(true, std::memory_order_acq_rel);
	if (!was_dirty) {
		register_dirty_buffer(this);
	}
}

template <typename Device>
auto gse::vulkan::basic_buffer<Device>::host_read() const -> std::span<const std::byte> {
	assert(m_allocation.mapped(), "Buffer must be persistently mapped to host_read");
	return std::span<const std::byte>(m_allocation.mapped(), m_size);
}

template <typename Device>
auto gse::vulkan::basic_buffer<Device>::host_dirty() const noexcept -> bool {
	return m_host_dirty.load(std::memory_order_acquire);
}

template <typename Device>
auto gse::vulkan::basic_buffer<Device>::clear_host_dirty() const noexcept -> void {
	const bool was_dirty = m_host_dirty.exchange(false, std::memory_order_acq_rel);
	if (was_dirty) {
		unregister_dirty_buffer(this);
	}
}
