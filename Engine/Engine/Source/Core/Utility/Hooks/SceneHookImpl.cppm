export module gse.utility:scene_hook_impl;

import std;

import :id;
import :registry;
import :scene;
import :scene_hook;

gse::hook<gse::scene>::hook(scene* owner) : identifiable_owned(owner->id()), m_owner(owner) {
	assert(m_owner != nullptr, std::source_location::current(), "Scene owner cannot be null for hook with owner id {}.", owner->id());
}

auto gse::hook<gse::scene>::build(const std::string& name) const -> builder {
	const auto id = m_owner->add_entity(name);
	return builder(id, m_owner, &m_owner->registry());
}

auto gse::hook<gse::scene>::builder::push_init(init_fn fn) -> void {
	m_scene->push_init(m_entity_id, std::move(fn));
}

auto gse::hook<gse::scene>::builder::initialize(gse::move_only_function<void(scene_init_context&)> fn) -> builder& {
	push_init([fn = std::move(fn)](const id self, registry& reg) mutable {
		scene_init_context ctx(self, &reg);
		fn(ctx);
	});
	return *this;
}
