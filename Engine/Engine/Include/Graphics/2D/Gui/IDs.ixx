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
}

namespace gse::gui::ids {
	thread_local std::vector<std::uint64_t> id_stack;
	constexpr auto k_root_seed = 0xD00DCAFE12345678ull;

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
	return id_stack.empty() ? k_root_seed : id_stack.back();
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

export namespace gse::gui::ids {
	struct scope : non_copyable {
		bool active = false;

		scope(
			std::uint64_t v
		);

		scope(
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
	if (!exists(hash)) {
		return generate_id(hash);
	}
	return find(hash);
}
