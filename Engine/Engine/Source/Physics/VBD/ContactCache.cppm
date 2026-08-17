export module gse.physics:vbd_contact_cache;

import std;

import gse.core;
import gse.math;
import gse.meta;
import :contact_manifold;

export namespace gse::vbd {
	struct cached_lambda {
		vec3<force> lambda;
		vec3<stiffness> penalty;
		vec3f normal = {};
		vec3f tangent_u = {};
		vec3f tangent_v = {};
		vec3<lever_arm> local_anchor_a = {};
		vec3<lever_arm> local_anchor_b = {};
		bool sticking = false;
	};

	class contact_cache {
	public:
		static constexpr std::uint32_t max_age = 3;

		auto lookup(
			id body_a,
			id body_b,
			const feature_id& fid
		) const -> std::optional<cached_lambda>;

		auto store(
			id body_a,
			id body_b,
			const feature_id& fid,
			const cached_lambda& data
		) -> void;

		auto touch(
			id body_a,
			id body_b,
			const feature_id& fid
		) -> bool;

		template <std::invocable<std::uint64_t, const cached_lambda&> Fn>
		auto for_each_pair(
			id body_a,
			id body_b,
			Fn&& fn
		) -> std::size_t;

		auto advance() -> void;

		auto reserve(
			std::size_t contact_count
		) -> void;

		auto clear() -> void;

	private:
		struct slot_key {
			std::uint64_t key_lo = 0;
			std::uint64_t key_hi = 0;
			std::uint64_t feature = 0;
			std::uint32_t last_seen = 0;
		};

		static constexpr std::uint32_t empty_slot = std::numeric_limits<std::uint32_t>::max();

		auto find_handle(
			std::uint64_t key_lo,
			std::uint64_t key_hi,
			std::uint64_t feature
		) const -> std::uint32_t;

		auto stale(
			std::uint32_t handle
		) const -> bool;

		auto rebuild(
			std::size_t index_capacity
		) -> void;

		std::vector<slot_key> m_keys;
		std::vector<cached_lambda> m_payloads;
		std::vector<std::uint32_t> m_index;
		std::size_t m_sorted_count = 0;
		std::uint32_t m_stamp = 1;
		bool m_stored_since_advance = false;
	};
}

namespace gse::vbd {
	auto swapped_sides(
		const feature_id& fid
	) -> feature_id;

	auto swapped_payload(
		const cached_lambda& data
	) -> cached_lambda;

	auto key_hash(
		std::uint64_t key_lo,
		std::uint64_t key_hi,
		std::uint64_t feature
	) -> std::size_t;
}

auto gse::vbd::swapped_sides(const feature_id& fid) -> feature_id {
	return {
		.type_a = fid.type_b,
		.type_b = fid.type_a,
		.index_a = fid.index_b,
		.index_b = fid.index_a,
		.side_a0 = fid.side_b0,
		.side_a1 = fid.side_b1,
		.side_b0 = fid.side_a0,
		.side_b1 = fid.side_a1
	};
}

auto gse::vbd::swapped_payload(const cached_lambda& data) -> cached_lambda {
	auto result = data;
	result.normal = -result.normal;
	result.tangent_u = -result.tangent_u;
	result.tangent_v = -result.tangent_v;
	std::swap(result.local_anchor_a, result.local_anchor_b);
	return result;
}

auto gse::vbd::key_hash(const std::uint64_t key_lo, const std::uint64_t key_hi, const std::uint64_t feature) -> std::size_t {
	return static_cast<std::size_t>(hash_combine(hash_combine(key_lo, key_hi), feature));
}

auto gse::vbd::contact_cache::find_handle(const std::uint64_t key_lo, const std::uint64_t key_hi, const std::uint64_t feature) const -> std::uint32_t {
	if (m_index.empty()) {
		return empty_slot;
	}
	const std::size_t mask = m_index.size() - 1;
	std::size_t slot = key_hash(key_lo, key_hi, feature) & mask;
	for (std::size_t probes = 0; probes < m_index.size(); ++probes) {
		const std::uint32_t handle = m_index[slot];
		if (handle == empty_slot) {
			return empty_slot;
		}
		const slot_key& k = m_keys[handle];
		if (k.key_lo == key_lo && k.key_hi == key_hi && k.feature == feature) {
			return handle;
		}
		slot = (slot + 1) & mask;
	}
	return empty_slot;
}

auto gse::vbd::contact_cache::lookup(const id body_a, const id body_b, const feature_id& fid) const -> std::optional<cached_lambda> {
	const bool forward = body_a.number() <= body_b.number();
	const std::uint64_t key_lo = forward ? body_a.number() : body_b.number();
	const std::uint64_t key_hi = forward ? body_b.number() : body_a.number();
	const std::uint64_t feature = pack_feature(forward ? fid : swapped_sides(fid));

	const std::uint32_t handle = find_handle(key_lo, key_hi, feature);
	if (handle == empty_slot || stale(handle)) {
		return std::nullopt;
	}
	return forward ? m_payloads[handle] : swapped_payload(m_payloads[handle]);
}

auto gse::vbd::contact_cache::store(const id body_a, const id body_b, const feature_id& fid, const cached_lambda& data) -> void {
	if (m_index.empty()) {
		rebuild(1024);
	}

	const bool forward = body_a.number() <= body_b.number();
	const std::uint64_t key_lo = forward ? body_a.number() : body_b.number();
	const std::uint64_t key_hi = forward ? body_b.number() : body_a.number();
	const std::uint64_t feature = pack_feature(forward ? fid : swapped_sides(fid));

	const std::size_t mask = m_index.size() - 1;
	std::size_t slot = key_hash(key_lo, key_hi, feature) & mask;
	for (std::size_t probes = 0; probes < m_index.size(); ++probes) {
		const std::uint32_t handle = m_index[slot];
		if (handle == empty_slot) {
			m_index[slot] = static_cast<std::uint32_t>(m_keys.size());
			m_keys.push_back({
				.key_lo = key_lo,
				.key_hi = key_hi,
				.feature = feature,
				.last_seen = m_stamp
			});
			m_payloads.push_back(forward ? data : swapped_payload(data));
			m_stored_since_advance = true;
			return;
		}
		slot_key& k = m_keys[handle];
		if (k.key_lo == key_lo && k.key_hi == key_hi && k.feature == feature) {
			k.last_seen = m_stamp;
			m_payloads[handle] = forward ? data : swapped_payload(data);
			return;
		}
		slot = (slot + 1) & mask;
	}

	rebuild(m_index.size() * 2);
	store(body_a, body_b, fid, data);
}

auto gse::vbd::contact_cache::touch(const id body_a, const id body_b, const feature_id& fid) -> bool {
	const bool forward = body_a.number() <= body_b.number();
	const std::uint64_t key_lo = forward ? body_a.number() : body_b.number();
	const std::uint64_t key_hi = forward ? body_b.number() : body_a.number();
	const std::uint64_t feature = pack_feature(forward ? fid : swapped_sides(fid));

	const std::uint32_t handle = find_handle(key_lo, key_hi, feature);
	if (handle == empty_slot) {
		return false;
	}
	m_keys[handle].last_seen = m_stamp;
	return true;
}

auto gse::vbd::contact_cache::advance() -> void {
	++m_stamp;
	if (m_stamp == 0) {
		m_stamp = 1;
	}
	if (m_keys.size() * 2 > m_index.size()) {
		rebuild(m_index.size());
		if (m_keys.size() * 2 > m_index.size()) {
			rebuild(m_index.size() * 2);
		}
	}
	if (!m_stored_since_advance && m_keys.size() - m_sorted_count > 64) {
		rebuild(m_index.size());
	}
	m_stored_since_advance = false;
}

auto gse::vbd::contact_cache::reserve(const std::size_t contact_count) -> void {
	m_keys.reserve(contact_count * 2);
	m_payloads.reserve(contact_count * 2);
	const std::size_t need = std::bit_ceil(std::max<std::size_t>(contact_count * 4, 1024));
	if (need > m_index.size()) {
		rebuild(need);
	}
}

auto gse::vbd::contact_cache::clear() -> void {
	m_keys.clear();
	m_payloads.clear();
	m_index.clear();
	m_sorted_count = 0;
	m_stamp = 1;
}

auto gse::vbd::contact_cache::stale(const std::uint32_t handle) const -> bool {
	return m_stamp - m_keys[handle].last_seen > max_age;
}

auto gse::vbd::contact_cache::rebuild(const std::size_t index_capacity) -> void {
	std::vector<std::uint32_t> order;
	order.reserve(m_keys.size());
	for (std::uint32_t handle = 0; handle < m_keys.size(); ++handle) {
		if (!stale(handle)) {
			order.push_back(handle);
		}
	}
	std::ranges::sort(order, [this](const std::uint32_t a, const std::uint32_t b) {
		const slot_key& ka = m_keys[a];
		const slot_key& kb = m_keys[b];
		if (ka.key_lo != kb.key_lo) {
			return ka.key_lo < kb.key_lo;
		}
		if (ka.key_hi != kb.key_hi) {
			return ka.key_hi < kb.key_hi;
		}
		return ka.feature < kb.feature;
	});

	std::vector<slot_key> new_keys;
	std::vector<cached_lambda> new_payloads;
	new_keys.reserve(order.size());
	new_payloads.reserve(order.size());
	for (const auto handle : order) {
		new_keys.push_back(m_keys[handle]);
		new_payloads.push_back(std::move(m_payloads[handle]));
	}
	m_keys = std::move(new_keys);
	m_payloads = std::move(new_payloads);

	m_index.assign(index_capacity, empty_slot);
	const std::size_t mask = m_index.size() - 1;
	for (std::uint32_t handle = 0; handle < m_keys.size(); ++handle) {
		const slot_key& k = m_keys[handle];
		std::size_t slot = key_hash(k.key_lo, k.key_hi, k.feature) & mask;
		while (m_index[slot] != empty_slot) {
			slot = (slot + 1) & mask;
		}
		m_index[slot] = handle;
	}

	m_sorted_count = m_keys.size();
}

template <std::invocable<std::uint64_t, const gse::vbd::cached_lambda&> Fn>
auto gse::vbd::contact_cache::for_each_pair(const id body_a, const id body_b, Fn&& fn) -> std::size_t {
	if (m_keys.size() - m_sorted_count > 4096) {
		return 0;
	}

	const std::uint64_t key_lo = std::min(body_a.number(), body_b.number());
	const std::uint64_t key_hi = std::max(body_a.number(), body_b.number());

	std::size_t emitted = 0;
	const auto emit = [&](const std::uint32_t handle) {
		if (m_stamp - m_keys[handle].last_seen != 1) {
			return;
		}
		m_keys[handle].last_seen = m_stamp;
		fn(m_keys[handle].feature, m_payloads[handle]);
		++emitted;
	};

	const auto sorted_begin = m_keys.begin();
	const auto sorted_end = sorted_begin + static_cast<std::ptrdiff_t>(m_sorted_count);
	auto it = std::ranges::lower_bound(sorted_begin, sorted_end, std::pair(key_lo, key_hi), std::ranges::less{}, [](const slot_key& k) {
		return std::pair(k.key_lo, k.key_hi);
	});
	for (; it != sorted_end && it->key_lo == key_lo && it->key_hi == key_hi; ++it) {
		emit(static_cast<std::uint32_t>(it - m_keys.begin()));
	}

	for (std::size_t handle = m_sorted_count; handle < m_keys.size(); ++handle) {
		const slot_key& k = m_keys[handle];
		if (k.key_lo == key_lo && k.key_hi == key_hi) {
			emit(static_cast<std::uint32_t>(handle));
		}
	}

	return emitted;
}
