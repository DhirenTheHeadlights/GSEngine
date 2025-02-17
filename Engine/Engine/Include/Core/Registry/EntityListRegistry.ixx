export module gse.core.registry.entity_list;

import std;

import gse.core.id;
import gse.core.clock;

export namespace gse::registry {
	auto is_entity_id_in_list(id* list_id, std::uint32_t id) -> bool;
	auto add_new_entity_list(id* list_id, const std::vector<std::uint32_t>& ids = {}) -> void;
	auto add_id_to_list(id* list_id, std::uint32_t id) -> void;
	auto remove_id_from_list(id* list_id, std::uint32_t id) -> void;
	auto periodically_clean_up_registry(const time& clean_up_interval = seconds(60.f)) -> void;
}

std::unordered_map<gse::id*, std::vector<std::uint32_t>> g_entity_lists;

gse::clock g_clean_up_clock;

auto gse::registry::is_entity_id_in_list(id* list_id, const std::uint32_t id) -> bool {
	return std::ranges::find(g_entity_lists[list_id], id) != g_entity_lists[list_id].end();
}

auto gse::registry::add_new_entity_list(id* list_id, const std::vector<std::uint32_t>& ids) -> void {
	g_entity_lists[list_id] = ids;
}

auto gse::registry::add_id_to_list(id* list_id, const std::uint32_t id) -> void {
	g_entity_lists[list_id].push_back(id);
}

auto gse::registry::remove_id_from_list(id* list_id, const std::uint32_t id) -> void {
	std::erase_if(g_entity_lists[list_id], [&](const std::uint32_t obj) {
		return obj == id;
		});
}

auto gse::registry::periodically_clean_up_registry(const time& clean_up_interval) -> void {
	if (g_clean_up_clock.get_elapsed_time() < clean_up_interval) {
		return;
	}

	/// TODO: This causes a crash sometimes - not sure how to fix just yet

	/*for (auto it = g_entity_lists.begin(); it != g_entity_lists.end();) {
		if (does_id_exist(it->first->number())) {
			it = g_entity_lists.erase(it);
		}
		else {
			++it;
		}
	}*/

	g_clean_up_clock.reset();
}