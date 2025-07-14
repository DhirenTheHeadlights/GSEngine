export module gse.utility:hook;

import std;

import :id;

import gse.physics.math;

export namespace gse {
	template <is_identifiable T>
	struct hook_base {
		virtual ~hook_base() = default;
		explicit hook_base(T* owner) : m_owner(owner) {}

		auto owner_id() -> const id& {
			return m_owner->id();
		}
	protected:
		T* m_owner;
	};

	template <typename T>
	struct hook : hook_base<T> {
		explicit hook(T* owner) : hook_base<T>(owner) {}

		virtual auto initialize() -> void {}
		virtual auto update() -> void {}
		virtual auto render() -> void {}
	};

	struct entity {
		std::uint32_t index = random_value(std::numeric_limits<std::uint32_t>::max());
		std::uint32_t generation = 0;
	};

	template <>
	struct hook<entity> : identifiable_owned {
		virtual ~hook() = default;
		explicit hook(const id& owner_id) : identifiable_owned(owner_id) {}

		virtual auto initialize() -> void {}
		virtual auto update() -> void {}
		virtual auto render() -> void {}
	};

	template <typename T>
	class hookable {
	public:
		auto add_hook(std::unique_ptr<hook<T>> hook) -> void {
			m_hooks.push_back(std::move(hook));
		}

		auto initialize() const -> void {
			for (auto& hook : m_hooks) {
				hook->initialize();
			}
		}

		auto update() const -> void {
			for (auto& hook : m_hooks) {
				hook->update();
			}
		}

		auto render() const -> void {
			for (auto& hook : m_hooks) {
				hook->render();
			}
		}
	private:
		std::vector<std::unique_ptr<hook<T>>> m_hooks;
		friend hook<T>;
	};
}
