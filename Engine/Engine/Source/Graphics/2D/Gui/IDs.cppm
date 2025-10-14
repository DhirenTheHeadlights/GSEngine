export module gse.graphics:ids;

import std;

import gse.utility;

export namespace gse::gui::ids {
	auto current_seed(
	) -> std::uint64_t;

	auto push(
		std::uint64_t v
	) -> void;

	auto push(
		std::string_view s
	) -> void;

	auto pop(
	) -> void;

	auto scoped_key(
		std::string_view s
	) -> std::uint64_t;

	auto scoped_key(
		std::uint64_t v
	) -> std::uint64_t;

	auto stable_key(
		std::string_view s
	) -> std::uint64_t;

	auto stable_key(
		std::uint64_t v
	) -> std::uint64_t;

	class key_builder {
	public:
		key_builder(
			std::uint64_t seed
		);

		auto with(
			std::string_view s
		) const -> std::uint64_t;

		auto with(
			std::uint64_t v
		) const -> std::uint64_t;

		auto seed(
		) const -> std::uint64_t ;

	private:
		std::uint64_t m_seed = 0;
	};

	auto make_key_builder(
		std::string_view scope_label
	) -> key_builder;

	class per_frame_key_cache {
	public:
		auto begin(
			std::uint64_t seed,
			std::uint64_t frame_seq = 0
		) -> void;

		auto key(
			const void* p
		) -> std::uint64_t;
	private:
		std::unordered_map<const void*, std::uint64_t> m_map;
		std::uint64_t m_salt = 0;
		std::uint64_t m_epoch = 0;
	};
}

namespace gse::gui::ids {
	thread_local std::vector<std::uint64_t> id_stack;
	constexpr auto root_seed = 0xD00DCAFE12345678ull;

	auto mix_64(
		std::uint64_t x
	) -> std::uint64_t;

	auto hash_combine_u64(
		std::uint64_t h,
		std::uint64_t v
	) -> std::uint64_t;

	auto hash_combine_string(
		std::uint64_t h,
		std::string_view s
	) -> std::uint64_t;

	auto make(
		std::string_view s
	) -> id;
}

auto gse::gui::ids::current_seed() -> std::uint64_t {
	return id_stack.empty() ? root_seed : id_stack.back();
}

auto gse::gui::ids::push(const std::uint64_t v) -> void {
	id_stack.push_back(hash_combine_u64(current_seed(), v));
}

auto gse::gui::ids::push(const std::string_view s) -> void {
	id_stack.push_back(hash_combine_string(current_seed(), s));
}

auto gse::gui::ids::pop() -> void {
	if (!id_stack.empty()) {
		id_stack.pop_back();
	}
}

auto gse::gui::ids::scoped_key(const std::string_view s) -> std::uint64_t {
	return hash_combine_string(current_seed(), s);
}

auto gse::gui::ids::scoped_key(const std::uint64_t v) -> std::uint64_t {
	return hash_combine_u64(current_seed(), v);
}

auto gse::gui::ids::stable_key(const std::string_view s) -> std::uint64_t {
	return hash_combine_string(root_seed, s);
}

auto gse::gui::ids::stable_key(const std::uint64_t v) -> std::uint64_t {
	return hash_combine_u64(root_seed, v);
}

gse::gui::ids::key_builder::key_builder(const std::uint64_t seed) : m_seed(seed) {}

auto gse::gui::ids::key_builder::with(const std::string_view s) const -> std::uint64_t {
	return hash_combine_string(m_seed, s);
}

auto gse::gui::ids::key_builder::with(const std::uint64_t v) const -> std::uint64_t {
	return hash_combine_u64(m_seed, v);
}

auto gse::gui::ids::key_builder::seed() const -> std::uint64_t {
	return m_seed;
}

auto gse::gui::ids::make_key_builder(const std::string_view scope_label) -> key_builder {
	return hash_combine_string(root_seed, scope_label);
}

auto gse::gui::ids::per_frame_key_cache::begin(const std::uint64_t seed, const std::uint64_t frame_seq) -> void {
	m_map.clear();
	m_salt = seed;
	m_epoch = frame_seq;
}

auto gse::gui::ids::per_frame_key_cache::key(const void* p) -> std::uint64_t {
	if (const auto it = m_map.find(p); it != m_map.end()) {
		return it->second;
	}

	std::uint64_t h = m_salt;

	if (m_epoch) {
		h = hash_combine_u64(h, m_epoch);
	}

	h = hash_combine_u64(h, reinterpret_cast<std::uintptr_t>(p));
	h = mix_64(h);

	return m_map.emplace(p, h).first->second;
}

export namespace gse::gui::ids {
	struct scope : non_copyable {
		bool active = false;

		explicit scope(
			std::uint64_t v
		);

		explicit scope(
			std::string_view s
		);

		~scope() override;
	};
}

gse::gui::ids::scope::scope(const std::uint64_t v) {
	active = true;
	push(v);
}

gse::gui::ids::scope::scope(const std::string_view s) {
	active = true;
	push(s);
}

gse::gui::ids::scope::~scope() {
	if (active) {
		pop();
	}
}

auto gse::gui::ids::mix_64(std::uint64_t x) -> std::uint64_t {
	x += 0x9E3779B97F4A7C15ull;
	x = (x ^ x >> 30) * 0xBF58476D1CE4E5B9ull;
	x = (x ^ x >> 27) * 0x94D049BB133111EBull;
	return x ^ x >> 31;
}

auto gse::gui::ids::hash_combine_u64(const std::uint64_t h, const std::uint64_t v) -> std::uint64_t {
	return mix_64(h ^ v);
}

auto gse::gui::ids::hash_combine_string(const std::uint64_t h, const std::string_view s) -> std::uint64_t {
	std::uint64_t fnv = 1469598103934665603ull;
	for (const unsigned char c : s) {
		fnv ^= c; fnv *= 1099511628211ull;
	}
	return hash_combine_u64(h, fnv);
}

auto gse::gui::ids::make(const std::string_view s) -> id {
	const auto hash = hash_combine_string(current_seed(), s);
	return generate_temp_id(hash, std::string(s));
}
