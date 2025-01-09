#pragma once
#include <memory>

namespace gse {
	struct object;
	class scene;
	class id;

	template <typename T>
	struct hook_base {
		virtual ~hook_base() = default;
		hook_base(T* owner) : m_owner(owner) {}
	protected:
		T* m_owner;
	};

	template <typename T>
	struct hook : hook_base<T> {
		hook(T* owner, id* owner_id) : hook_base<T>(owner), m_owner_id(owner_id) {}

		virtual auto initialize() -> void {}
		virtual auto update() -> void {}
		virtual auto render() -> void {}
	protected:
		id* m_owner_id;
	};

	template <>
	struct hook<object> {
		virtual ~hook() = default;
		hook() = default;

		virtual auto initialize() -> void {}
		virtual auto update() -> void {}
		virtual auto render() -> void {}

		std::uint32_t owner_id = 0;
	};

	template <typename T>
	class hookable {
	public:
		auto add_hook(std::unique_ptr<hook<T>> hook) -> void {
			m_hooks.push_back(std::move(hook));
		}

	private:
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

		std::vector<std::unique_ptr<hook<T>>> m_hooks;

		friend class scene;
	};
}
