export module gse.utility:hook;

import std;

import :id;

import gse.physics.math;

namespace gse {
	class scene;
}

export namespace gse {
	template <is_identifiable T>
	class hook_base {
	public:
		virtual ~hook_base() = default;
		explicit hook_base(T* owner) : m_owner(owner) {}

		auto owner_id() -> const id& {
			return m_owner->id();
		}
	protected:
		T* m_owner;
	};

	template <typename T>
	class hook : public hook_base<T> {
	public:
		explicit hook(T* owner) : hook_base<T>(owner) {}

		virtual auto initialize() -> void {}
		virtual auto update() -> void {}
		virtual auto render() -> void {}
	};

	struct entity {
		std::uint32_t index = random_value(std::numeric_limits<std::uint32_t>::max());
		std::uint32_t generation = 0;

		auto operator==(const entity& other) const -> bool = default;
	};

	template <>
	class hook<entity> : public identifiable_owned {
	public:
		virtual ~hook() = default;
		explicit hook(const id& owner_id, scene* scene) : identifiable_owned(owner_id), m_scene(scene) {}

		virtual auto initialize() -> void {}
		virtual auto update() -> void {}
		virtual auto render() -> void {}

		template <typename T>
		auto component() -> T&;
	protected:
		scene* m_scene = nullptr;
	};

	template <typename T>
	class hookable : public identifiable {
	public:
		hookable(const std::string& name, std::initializer_list<std::unique_ptr<hook<T>>> hooks = {}) : identifiable(name) {
			for (auto&& h : hooks) m_hooks.push_back(std::move(const_cast<std::unique_ptr<hook<T>>&>(h)));
		}

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

		auto loop() const -> void {
			for (auto& hook : m_hooks) {
				hook->update();
				hook->render();
			}
		}
	private:
		std::vector<std::unique_ptr<hook<T>>> m_hooks;
		friend hook<T>;
	};
}
