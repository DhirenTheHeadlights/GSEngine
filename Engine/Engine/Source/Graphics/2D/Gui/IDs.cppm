export module gse.graphics:ids;

import std;

import gse.core;
import gse.meta;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;

export namespace gse::gui::ids {
	auto current_seed() -> std::uint64_t;

	auto make(
		std::string_view s
	) -> id;

	auto make_from_key(
		std::uint64_t key
	) -> id;

	struct scope : non_copyable {
		bool active = false;

		explicit scope(
			std::uint64_t v
		);

		explicit scope(
			std::string_view s
		);

		~scope();
	};
}

namespace gse::gui::ids {
	inline thread_local std::vector<std::uint64_t> id_stack;
	constexpr auto root_seed = 0xD00DCAFE12345678ull;

	auto push(
		std::uint64_t v
	) -> void;

	auto push(
		std::string_view s
	) -> void;

	auto pop() -> void;
}

auto gse::gui::ids::current_seed() -> std::uint64_t {
	return id_stack.empty() ? root_seed : id_stack.back();
}

auto gse::gui::ids::make(const std::string_view s) -> id {
	return generate_temp_id(hash_combine(current_seed(), stable_id(s)));
}

auto gse::gui::ids::make_from_key(const std::uint64_t key) -> id {
	return generate_temp_id(hash_combine(current_seed(), key));
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

auto gse::gui::ids::push(const std::uint64_t v) -> void {
	id_stack.push_back(hash_combine(current_seed(), v));
}

auto gse::gui::ids::push(const std::string_view s) -> void {
	id_stack.push_back(hash_combine(current_seed(), stable_id(s)));
}

auto gse::gui::ids::pop() -> void {
	if (!id_stack.empty()) {
		id_stack.pop_back();
	}
}
