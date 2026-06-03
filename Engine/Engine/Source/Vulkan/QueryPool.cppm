export module gse.vulkan:query_pool;

import std;
import vulkan;

import :handles;
import :types;
import :device;

import gse.core;

export namespace gse::vulkan {
	class query_pool final : public non_copyable {
	public:
		query_pool() {}
		~query_pool() = default;

		query_pool(
			query_pool&&
		) noexcept = default;

		auto operator=(
			query_pool&&
		) noexcept -> query_pool& = default;

		[[nodiscard]]
		static auto create_timestamp(
			const device& dev,
			std::uint32_t capacity,
			std::string_view label = {}
		) -> query_pool;

		[[nodiscard]]
		static auto create_pipeline_stats(
			const device& dev,
			std::uint32_t capacity,
			gpu::pipeline_statistic_flags statistics,
			std::string_view label = {}
		) -> query_pool;

		[[nodiscard]] auto handle(
			this const query_pool& self
		) -> gpu::query_pool_handle;

		[[nodiscard]] auto valid() const -> bool;

		template <typename T>
		[[nodiscard]]
		auto results(
			std::uint32_t first_query,
			std::uint32_t query_count,
			std::uint64_t stride
		) const -> std::pair<gpu::query_status, std::vector<T>>;

	private:
		explicit query_pool(
			vk::raii::QueryPool&& pool
		);

		[[nodiscard]] static auto translate_status(
			vk::Result r
		) -> gpu::query_status;

		vk::raii::QueryPool m_pool = nullptr;
	};
}

gse::vulkan::query_pool::query_pool(vk::raii::QueryPool&& pool) : m_pool(std::move(pool)) {
}

auto gse::vulkan::query_pool::translate_status(const vk::Result r) -> gpu::query_status {
	return r == vk::Result::eSuccess ? gpu::query_status::success : gpu::query_status::error;
}

auto gse::vulkan::query_pool::create_timestamp(const device& dev, const std::uint32_t capacity, const std::string_view label) -> query_pool {
	const vk::QueryPoolCreateInfo info{
		.queryType = vk::QueryType::eTimestamp,
		.queryCount = capacity,
	};
	auto pool = dev.raii_device().createQueryPool(info);
	if (!label.empty()) {
		const std::string name{ label };
		const vk::DebugUtilsObjectNameInfoEXT name_info{
			.objectType = vk::ObjectType::eQueryPool,
			.objectHandle = std::bit_cast<std::uint64_t>(*pool),
			.pObjectName = name.c_str(),
		};
		dev.raii_device().setDebugUtilsObjectNameEXT(name_info);
	}
	return query_pool(std::move(pool));
}

auto gse::vulkan::query_pool::create_pipeline_stats(const device& dev, const std::uint32_t capacity, const gpu::pipeline_statistic_flags statistics, const std::string_view label) -> query_pool {
	const vk::QueryPoolCreateInfo info{
		.queryType = vk::QueryType::ePipelineStatistics,
		.queryCount = capacity,
		.pipelineStatistics = to_vk(statistics),
	};
	auto pool = dev.raii_device().createQueryPool(info);
	if (!label.empty()) {
		const std::string name{ label };
		const vk::DebugUtilsObjectNameInfoEXT name_info{
			.objectType = vk::ObjectType::eQueryPool,
			.objectHandle = std::bit_cast<std::uint64_t>(*pool),
			.pObjectName = name.c_str(),
		};
		dev.raii_device().setDebugUtilsObjectNameEXT(name_info);
	}
	return query_pool(std::move(pool));
}

auto gse::vulkan::query_pool::handle(this const query_pool& self) -> gpu::query_pool_handle {
	return std::bit_cast<gpu::query_pool_handle>(*self.m_pool);
}

auto gse::vulkan::query_pool::valid() const -> bool {
	return *m_pool != nullptr;
}

template <typename T>
auto gse::vulkan::query_pool::results(const std::uint32_t first_query, const std::uint32_t query_count, const std::uint64_t stride) const -> std::pair<gpu::query_status, std::vector<T>> {
	static_assert(sizeof(T) == sizeof(std::uint64_t), "query_pool::results currently expects 64-bit results");
	const std::size_t group_count = stride > 0 ? std::max<std::size_t>(stride / sizeof(T), 1) : 1;
	const std::size_t element_count = static_cast<std::size_t>(query_count) * group_count;
	auto [status, vec] = m_pool.getResults<T>(
		first_query,
		query_count,
		element_count * sizeof(T),
		stride,
		vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait
	);
	return { translate_status(status), std::move(vec) };
}
