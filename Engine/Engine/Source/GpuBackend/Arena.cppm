export module gse.gpu_backend:arena;

import std;
import gse.meta;

namespace gse::gpu {
	struct handle_key_probe {
		std::uint64_t value = 0;
	};
}

export namespace gse::gpu {
	using manifest_row = std::pair<std::meta::info, std::meta::info>;

	struct retired_resource {
		std::uint64_t value = 0;
		std::uint64_t retire_after = 0;
	};

	template <typename Backend>
	struct retiring_pool {
		std::unordered_map<std::uint64_t, Backend> live;
		std::vector<retired_resource> retired;

		auto retire(
			std::uint64_t key,
			std::uint64_t retire_after
		) -> void;

		auto collect(
			std::uint64_t frame
		) -> void;
	};

	consteval auto arena_pools_type(
		std::vector<manifest_row> manifest
	) -> std::meta::info;

	consteval auto arena_backend_for(
		std::vector<manifest_row> manifest,
		std::meta::info frontend
	) -> std::meta::info;

	consteval auto arena_keys_are_uint64(
		std::vector<manifest_row> manifest
	) -> bool;

	template <typename Manifest, typename Frontend>
	using arena_backend_of = [:arena_backend_for(Manifest::entries(), ^^Frontend):];

	template <typename Manifest>
	class resource_arena {
	public:
		static_assert(
			arena_keys_are_uint64(Manifest::entries()),
			"resource_arena handles must each be a single uint64 value"
		);

		template <typename Backend>
		auto store(
			std::uint64_t key,
			Backend&& object
		) -> void;

		template <typename Frontend>
		auto retire(
			Frontend handle,
			std::uint64_t retire_after
		) -> void;

		template <typename Frontend>
		[[nodiscard]] auto find(Frontend handle) const -> const arena_backend_of<Manifest, Frontend>*;

		auto collect(
			std::uint64_t frame
		) -> void;

		auto clear() -> void;

	private:
		using storage = [:arena_pools_type(Manifest::entries()):];
		storage m_pools;
	};
}

template <typename Backend>
auto gse::gpu::retiring_pool<Backend>::retire(const std::uint64_t key, const std::uint64_t retire_after) -> void {
	retired.push_back({ key, retire_after });
}

template <typename Backend>
auto gse::gpu::retiring_pool<Backend>::collect(const std::uint64_t frame) -> void {
	std::erase_if(retired, [&](const retired_resource& r) {
		if (frame >= r.retire_after) {
			live.erase(r.value);
			return true;
		}
		return false;
	});
}

consteval auto gse::gpu::arena_pools_type(std::vector<manifest_row> manifest) -> std::meta::info {
	std::vector<std::meta::info> pools;
	for (const auto& backend : std::views::values(manifest)) {
		pools.push_back(std::meta::substitute(^^retiring_pool, { backend }));
	}
	return std::meta::substitute(^^std::tuple, pools);
}

consteval auto gse::gpu::arena_backend_for(std::vector<manifest_row> manifest, const std::meta::info frontend) -> std::meta::info {
	for (const auto& [front, back] : manifest) {
		if (std::meta::dealias(front) == std::meta::dealias(frontend)) {
			return back;
		}
	}
	return ^^void;
}

consteval auto gse::gpu::arena_keys_are_uint64(std::vector<manifest_row> manifest) -> bool {
	const auto key_type = std::meta::dealias(std::meta::type_of(std::meta::nonstatic_data_members_of(^^handle_key_probe, std::meta::access_context::unchecked())[0]));
	for (const auto& frontend : std::views::keys(manifest)) {
		const auto members = std::meta::nonstatic_data_members_of(frontend, std::meta::access_context::unchecked());
		if (members.size() != 1) {
			return false;
		}
		if (std::meta::dealias(std::meta::type_of(members[0])) != key_type) {
			return false;
		}
	}
	return true;
}

template <typename Manifest>
template <typename Backend>
auto gse::gpu::resource_arena<Manifest>::store(const std::uint64_t key, Backend&& object) -> void {
	std::get<retiring_pool<std::remove_cvref_t<Backend>>>(m_pools).live.emplace(key, std::forward<Backend>(object));
}

template <typename Manifest>
template <typename Frontend>
auto gse::gpu::resource_arena<Manifest>::retire(const Frontend handle, const std::uint64_t retire_after) -> void {
	std::get<retiring_pool<arena_backend_of<Manifest, Frontend>>>(m_pools).retire(handle.value, retire_after);
}

template <typename Manifest>
template <typename Frontend>
auto gse::gpu::resource_arena<Manifest>::find(const Frontend handle) const -> const arena_backend_of<Manifest, Frontend>* {
	const auto& live = std::get<retiring_pool<arena_backend_of<Manifest, Frontend>>>(m_pools).live;
	const auto it = live.find(handle.value);
	return it == live.end() ? nullptr : &it->second;
}

template <typename Manifest>
auto gse::gpu::resource_arena<Manifest>::collect(const std::uint64_t frame) -> void {
	std::apply([&](auto&... pool) { (pool.collect(frame), ...); }, m_pools);
}

template <typename Manifest>
auto gse::gpu::resource_arena<Manifest>::clear() -> void {
	std::apply([](auto&... pool) { (pool.live.clear(), ...); }, m_pools);
}
