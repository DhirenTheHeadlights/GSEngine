export module gse.utility:hook;

import std;

import :id;

namespace gse {
	export template <typename T>
	struct hook_base {
		virtual ~hook_base() = default;
		explicit hook_base(T* owner) : m_owner(owner) {}
	protected:
		T* m_owner;
	};

	export template <typename T>
	struct hook : hook_base<T> {
		hook(T* owner, const id& owner_id) : hook_base<T>(owner), m_owner_id(owner_id) {}

		virtual auto initialize() -> void {}
		virtual auto update() -> void {}
		virtual auto render() -> void {}
	protected:
		id m_owner_id;
	};

	export template <typename T>
	class hookable {
	public:
		auto add_hook(std::unique_ptr<hook<T>> hook) -> void {
			m_hooks.push_back(std::move(hook));
		}

		auto initialize_hooks() const -> void {
			for (auto& hook : m_hooks) {
				hook->initialize();
			}
		}

		auto update_hooks() const -> void {
			for (auto& hook : m_hooks) {
				hook->update();
			}
		}

		auto render_hooks() const -> void {
			for (auto& hook : m_hooks) {
				hook->render();
			}
		}
	private:
		std::vector<std::unique_ptr<hook<T>>> m_hooks;
	};
}
