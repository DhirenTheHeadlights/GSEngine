export module gse.gpu:vulkan_query_pool;

import std;
import vulkan;

import :handles;
import :vulkan_device;

import gse.core;

export namespace gse::gpu {
	enum class query_kind : std::uint8_t {
		timestamp [[= vk::QueryType::eTimestamp]],
		occlusion [[= vk::QueryType::eOcclusion]],
		pipeline_statistics [[= vk::QueryType::ePipelineStatistics]],
	};
}

export namespace gse::vulkan {
	class query_pool final : public non_copyable {
	public:
		query_pool() = default;

		~query_pool() override = default;

		query_pool(
			query_pool&&
		) noexcept = default;

		auto operator=(
			query_pool&&
		) noexcept -> query_pool& = default;

		[[nodiscard]] static auto create(
			const device& dev,
			gpu::query_kind kind,
			std::uint32_t count
		) -> query_pool;

		auto reset(
			std::uint32_t first = 0,
			std::uint32_t count = ~0u
		) -> void;

		[[nodiscard]] auto results(
			std::span<std::uint64_t> out,
			std::uint32_t first
		) const -> bool;

		[[nodiscard]] auto kind(
		) const -> gpu::query_kind;

		[[nodiscard]] auto count(
		) const -> std::uint32_t;

		[[nodiscard]] auto handle(
			this const query_pool& self
		) -> gpu::handle<query_pool>;

		explicit operator bool(
		) const;

	private:
		query_pool(
			vk::raii::QueryPool&& pool,
			gpu::query_kind kind,
			std::uint32_t count
		);

		vk::raii::QueryPool m_pool = nullptr;
		gpu::query_kind m_kind = gpu::query_kind::timestamp;
		std::uint32_t m_count = 0;
	};
}

gse::vulkan::query_pool::query_pool(vk::raii::QueryPool&& pool, const gpu::query_kind kind, const std::uint32_t count)
	: m_pool(std::move(pool)), m_kind(kind), m_count(count) {}

auto gse::vulkan::query_pool::create(const device& dev, const gpu::query_kind kind, const std::uint32_t count) -> query_pool {
	const vk::QueryPoolCreateInfo info{
		.queryType = to_vk(kind),
		.queryCount = count,
	};
	return query_pool(dev.raii_device().createQueryPool(info), kind, count);
}

auto gse::vulkan::query_pool::reset(const std::uint32_t first, std::uint32_t count) -> void {
	if (count == ~0u) {
		count = m_count - first;
	}
	m_pool.reset(first, count);
}

auto gse::vulkan::query_pool::results(const std::span<std::uint64_t> out, const std::uint32_t first) const -> bool {
	const auto result = m_pool.getResults<std::uint64_t>(
		first,
		static_cast<std::uint32_t>(out.size()),
		out.size_bytes(),
		sizeof(std::uint64_t),
		vk::QueryResultFlagBits::e64
	);
	if (result.result != vk::Result::eSuccess) {
		return false;
	}
	std::ranges::copy(result.value, out.begin());
	return true;
}

auto gse::vulkan::query_pool::kind() const -> gpu::query_kind {
	return m_kind;
}

auto gse::vulkan::query_pool::count() const -> std::uint32_t {
	return m_count;
}

auto gse::vulkan::query_pool::handle(this const query_pool& self) -> gpu::handle<query_pool> {
	return std::bit_cast<gpu::handle<query_pool>>(*self.m_pool);
}

gse::vulkan::query_pool::operator bool() const {
	return *m_pool != nullptr;
}
