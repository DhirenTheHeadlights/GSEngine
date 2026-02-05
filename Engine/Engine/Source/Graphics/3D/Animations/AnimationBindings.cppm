export module gse.graphics:animation_bindings;

import std;

import gse.utility;

import :controller_component;
import :animation_dsl;

export namespace gse::animation {
	class bindings {
		struct param_binding {
			std::string name;
			std::function<void(controller_component&)> sync;
		};

		struct trigger_binding {
			std::string name;
			std::function<bool()> condition;
		};

		std::vector<param_binding> m_params;
		std::vector<trigger_binding> m_triggers;
		controller_component* m_ctrl = nullptr;

	public:
		bindings() = default;

		explicit bindings(controller_component& ctrl) : m_ctrl(&ctrl) {}

		template <typename F>
			requires std::invocable<F> && std::convertible_to<std::invoke_result_t<F>, float>
		auto bind(const param_handle<float>& p, F&& func) -> bindings& {
			m_params.push_back({
				.name = p.name,
				.sync = [name = p.name, f = std::forward<F>(func)](controller_component& c) {
					c.parameters[name] = animation_parameter{
						.value = static_cast<float>(f()),
						.is_trigger = false
					};
				}
			});
			return *this;
		}

		template <typename F>
			requires std::invocable<F> && std::same_as<std::invoke_result_t<F>, bool>
		auto bind(const param_handle<bool>& p, F&& func) -> bindings& {
			m_params.push_back({
				.name = p.name,
				.sync = [name = p.name, f = std::forward<F>(func)](controller_component& c) {
					c.parameters[name] = animation_parameter{
						.value = f(),
						.is_trigger = false
					};
				}
			});
			return *this;
		}

		auto bind(const param_handle<bool>& p, const bool& ref) -> bindings& {
			m_params.push_back({
				.name = p.name,
				.sync = [name = p.name, &ref](controller_component& c) {
					c.parameters[name] = animation_parameter{
						.value = ref,
						.is_trigger = false
					};
				}
			});
			return *this;
		}

		auto bind(const param_handle<float>& p, const float& ref) -> bindings& {
			m_params.push_back({
				.name = p.name,
				.sync = [name = p.name, &ref](controller_component& c) {
					c.parameters[name] = animation_parameter{
						.value = ref,
						.is_trigger = false
					};
				}
			});
			return *this;
		}

		template <typename F>
			requires std::invocable<F> && std::same_as<std::invoke_result_t<F>, bool>
		auto on_trigger(const trigger_handle& t, F&& condition) -> bindings& {
			m_triggers.push_back({
				.name = t.name,
				.condition = std::forward<F>(condition)
			});
			return *this;
		}

		auto update() -> void {
			if (!m_ctrl) return;

			for (auto& p : m_params) {
				p.sync(*m_ctrl);
			}

			for (auto& t : m_triggers) {
				if (t.condition()) {
					m_ctrl->parameters[t.name] = animation_parameter{
						.value = true,
						.is_trigger = true
					};
				}
			}
		}
	};

	auto bind(controller_component& ctrl) -> bindings {
		return bindings{ ctrl };
	}
}
