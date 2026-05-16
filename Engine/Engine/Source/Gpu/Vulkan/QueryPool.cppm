export module gse.gpu:vulkan_query_pool;

import std;
import vulkan;

import :handles;

import gse.core;

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

		[[nodiscard]] auto handle(
			this const query_pool& self
		) -> gpu::handle<query_pool>;

		explicit operator bool(
		) const;

	private:
		vk::raii::QueryPool m_pool = nullptr;
	};
}

auto gse::vulkan::query_pool::handle(this const query_pool& self) -> gpu::handle<query_pool> {
	return std::bit_cast<gpu::handle<query_pool>>(*self.m_pool);
}

gse::vulkan::query_pool::operator bool() const {
	return *m_pool != nullptr;
}
