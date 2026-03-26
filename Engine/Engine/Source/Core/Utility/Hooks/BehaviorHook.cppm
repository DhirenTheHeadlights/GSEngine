export module gse.utility:behavior_hook;

import std;
import :id;
import :entity;
import :entity_hook;

export namespace gse {
	class behavior_hook final : public hook<entity> {
	public:
		using behavior_func = std::move_only_function<void(hook<entity>&)>;

		behavior_hook() = default;

		auto on_init(behavior_func func) -> void { m_initializers.push_back(std::move(func)); }
		auto on_update(behavior_func func) -> void { m_updaters.push_back(std::move(func)); }
		auto on_render(behavior_func func) -> void { m_renderers.push_back(std::move(func)); }
		auto on_shutdown(behavior_func func) -> void { m_shutdowns.push_back(std::move(func)); }

		auto initialize() -> void override { for (auto& f : m_initializers) f(*this); }
		auto update() -> void override { for (auto& f : m_updaters) f(*this); }
		auto render() -> void override { for (auto& f : m_renderers) f(*this); }
		auto shutdown() -> void override { for (auto& f : m_shutdowns) f(*this); }

	private:
		std::vector<behavior_func> m_initializers;
		std::vector<behavior_func> m_updaters;
		std::vector<behavior_func> m_renderers;
		std::vector<behavior_func> m_shutdowns;
	};
}
