export module gse.utility:scene_hook_impl;

import :scene;
import :scene_hook;

gse::hook<gse::scene>::hook(scene* owner) : identifiable_owned(owner->id()), m_owner(owner) {
	assert(m_owner != nullptr, std::source_location::current(), "Scene owner cannot be null for hook with owner id {}.", owner->id());
}

auto gse::hook<gse::scene>::build(const std::string& name) const -> builder {
	const auto id = m_owner->add_entity(name);
	return builder(id, &m_owner->registry());
}
