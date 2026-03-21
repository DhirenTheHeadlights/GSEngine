export module gse.physics:vbd_contact_cache;

import std;

import gse.math;
import :contact_manifold;

export namespace gse::vbd {
	struct cached_lambda {
		vec3<force> lambda;
		vec3<stiffness> penalty;
		unitless::vec3 normal = {};
		unitless::vec3 tangent_u = {};
		unitless::vec3 tangent_v = {};
		vec3<length> local_anchor_a = {};
		vec3<length> local_anchor_b = {};
		bool sticking = false;
		std::uint32_t age = 0;
	};

	class contact_cache {
	public:
		static constexpr std::uint32_t max_age = 3;

		auto lookup(
			std::uint32_t body_a,
			std::uint32_t body_b,
			const feature_id& fid
		) const -> std::optional<cached_lambda>;

		auto store(
			std::uint32_t body_a,
			std::uint32_t body_b,
			const feature_id& fid,
			const cached_lambda& data
		) -> void;

		auto age_and_prune(
		) -> void;

		auto clear(
		) -> void;

		auto remove_body(
			std::uint32_t body_index
		) -> void;
	private:
		struct cache_key {
			std::uint32_t body_a;
			std::uint32_t body_b;
			feature_id feature;

			auto operator==(const cache_key&) const -> bool = default;
		};

		struct cache_key_hash {
			auto operator()(const cache_key& k) const noexcept -> std::size_t {
				std::size_t h = 0;
				h ^= std::hash<std::uint32_t>{}(k.body_a) + 0x9e3779b9 + (h << 6) + (h >> 2);
				h ^= std::hash<std::uint32_t>{}(k.body_b) + 0x9e3779b9 + (h << 6) + (h >> 2);
				h ^= std::hash<std::uint8_t>{}(static_cast<std::uint8_t>(k.feature.type_a)) + 0x9e3779b9 + (h << 6) + (h >> 2);
				h ^= std::hash<std::uint8_t>{}(static_cast<std::uint8_t>(k.feature.type_b)) + 0x9e3779b9 + (h << 6) + (h >> 2);
				h ^= std::hash<std::uint8_t>{}(k.feature.index_a) + 0x9e3779b9 + (h << 6) + (h >> 2);
				h ^= std::hash<std::uint8_t>{}(k.feature.index_b) + 0x9e3779b9 + (h << 6) + (h >> 2);
				h ^= std::hash<std::uint8_t>{}(k.feature.side_a0) + 0x9e3779b9 + (h << 6) + (h >> 2);
				h ^= std::hash<std::uint8_t>{}(k.feature.side_a1) + 0x9e3779b9 + (h << 6) + (h >> 2);
				h ^= std::hash<std::uint8_t>{}(k.feature.side_b0) + 0x9e3779b9 + (h << 6) + (h >> 2);
				h ^= std::hash<std::uint8_t>{}(k.feature.side_b1) + 0x9e3779b9 + (h << 6) + (h >> 2);
				return h;
			}
		};

		std::unordered_map<cache_key, cached_lambda, cache_key_hash> m_cache;
	};
}

auto gse::vbd::contact_cache::lookup(
	const std::uint32_t body_a,
	const std::uint32_t body_b,
	const feature_id& fid
) const -> std::optional<cached_lambda> {
	const cache_key key{
		.body_a = body_a,
		.body_b = body_b,
		.feature = fid
	};

	if (const auto it = m_cache.find(key); it != m_cache.end()) {
		return it->second;
	}

	const cache_key key_swapped{
		.body_a = body_b,
		.body_b = body_a,
		.feature = {
			.type_a = fid.type_b,
			.type_b = fid.type_a,
			.index_a = fid.index_b,
			.index_b = fid.index_a,
			.side_a0 = fid.side_b0,
			.side_a1 = fid.side_b1,
			.side_b0 = fid.side_a0,
			.side_b1 = fid.side_a1
		}
	};

	if (const auto it = m_cache.find(key_swapped); it != m_cache.end()) {
		auto result = it->second;
		result.normal = -result.normal;
		result.tangent_u = -result.tangent_u;
		result.tangent_v = -result.tangent_v;
		std::swap(result.local_anchor_a, result.local_anchor_b);
		return result;
	}

	return std::nullopt;
}

auto gse::vbd::contact_cache::store(const std::uint32_t body_a, const std::uint32_t body_b, const feature_id& fid, const cached_lambda& data) -> void {
	const cache_key key{
		.body_a = body_a,
		.body_b = body_b,
		.feature = fid
	};
	m_cache[key] = data;
	m_cache[key].age = 0;
}

auto gse::vbd::contact_cache::age_and_prune() -> void {
	for (auto it = m_cache.begin(); it != m_cache.end(); ) {
		it->second.age++;
		if (it->second.age > max_age) {
			it = m_cache.erase(it);
		} else {
			++it;
		}
	}
}

auto gse::vbd::contact_cache::clear() -> void {
	m_cache.clear();
}

auto gse::vbd::contact_cache::remove_body(const std::uint32_t body_index) -> void {
	for (auto it = m_cache.begin(); it != m_cache.end(); ) {
		if (it->first.body_a == body_index || it->first.body_b == body_index) {
			it = m_cache.erase(it);
		}
		else {
			++it;
		}
	}
}
