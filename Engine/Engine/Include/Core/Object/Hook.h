#pragma once

namespace gse {
	struct base_hook {
		virtual ~base_hook() = default;

		virtual void initialize() {}
		virtual void update() {}
		virtual void render() {}
	};

	class object;
	class id;

	template <typename T>
	concept has_id = requires(T t) {
		{ t.get_id() } -> std::same_as<std::weak_ptr<id>>;
	};

	template <typename owner_type = object>
	struct hook : base_hook {
		hook() = default;
		hook(owner_type* owner) : m_owner(owner), m_id(owner->get_id().lock().get()) {}
		~hook() override = default;
	protected:
		owner_type* m_owner;
		id* m_id;
	};

	class scene;

	class hookable {
	public:
		void add_hook(std::unique_ptr<base_hook> hook) {
			m_hooks.push_back(std::move(hook));
		}

	private:
		void initialize_hooks() const {
			for (auto& hook : m_hooks) {
				hook->initialize();
			}
		}

		void update_hooks() const {
			for (auto& hook : m_hooks) {
				hook->update();
			}
		}

		void render_hooks() const {
			for (auto& hook : m_hooks) {
				hook->render();
			}
		}

		std::vector<std::unique_ptr<base_hook>> m_hooks;

		friend class scene;
	};
}