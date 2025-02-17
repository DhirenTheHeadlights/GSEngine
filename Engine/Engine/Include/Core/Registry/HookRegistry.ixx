export module gse.core.registry.hook;

import std;

import gse.core.registry.entity;
import gse.core.object.hook;
import gse.core.component;
import gse.platform.assert;

export namespace gse::registry {
	auto add_entity_hook(std::uint32_t parent_id, std::unique_ptr<hook<entity>> hook) -> void;
	auto remove_object_hook(const hook<entity>& hook) -> void;

	auto initialize_hooks() -> void;
	auto update_hooks() -> void;
	auto render_hooks() -> void;
}

std::vector<std::unique_ptr<gse::hook<gse::entity>>> g_hooks;
std::vector<std::unique_ptr<gse::hook<gse::entity>>> g_registered_inactive_hooks;

auto gse::registry::add_entity_hook(const std::uint32_t parent_id, std::unique_ptr<hook<entity>> hook) -> void {
	const auto entities = get_active_entities();
	const auto inactive_entities = get_inactive_entities();

	const auto it = std::ranges::find_if(entities, [parent_id](const std::uint32_t index) {
		return index == parent_id;
		});

	if (it != entities.end()) {
		hook->owner_id = parent_id;
		g_hooks.push_back(std::move(hook));
		return;
	}

	const auto it2 = std::ranges::find_if(inactive_entities, [parent_id](const std::uint32_t index) {
		return index == parent_id;
		});

	if (it2 != inactive_entities.end()) {
		hook->owner_id = parent_id;
		g_registered_inactive_hooks.push_back(std::move(hook));
		return;
	}

	assert_comment(false, "Object not found in registry when trying to add a hook to it");
}

auto gse::registry::remove_object_hook(const hook<entity>& hook) -> void {
	/// TODO: Implement
}

auto gse::registry::initialize_hooks() -> void {
	for (const auto& hook : g_hooks) {
		hook->initialize();
	}
}

auto gse::registry::update_hooks() -> void {
	for (const auto& hook : g_hooks) {
		hook->update();
	}
}

auto gse::registry::render_hooks() -> void {
	for (const auto& hook : g_hooks) {
		hook->render();
	}
}