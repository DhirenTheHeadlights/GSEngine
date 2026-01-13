export module gse.utility:hookable;

import std;

import :id;

import :hook;
import :concepts;

export namespace gse {
	template <typename T>
	class hookable : public identifiable {
	public:
		virtual ~hookable() = default;

		explicit hookable(
			std::string_view name,
			std::initializer_list<std::unique_ptr<hook<T>>> hooks = {}
		);

		template <typename Hook>
			requires is_hook<Hook, T>
		auto add_hook(
		) -> void;

		template <typename Hook, typename... Args>
			requires is_hook<Hook, T>
		auto add_hook(
			Args&&... args
		) -> void;

		virtual auto initialize(
		) -> void;

		virtual auto update(
		) -> void;

		virtual auto render(
		) -> void;

		virtual auto shutdown(
		) -> void;

		auto cycle(
			const std::function<bool()>& condition
		) const -> void;
	protected:
		std::vector<std::unique_ptr<hook<T>>> m_hooks;
		friend hook<T>;
	};
}

template <typename T>
gse::hookable<T>::hookable(const std::string_view name, std::initializer_list<std::unique_ptr<hook<T>>> hooks) : identifiable(name) {
	for (auto&& h : hooks) m_hooks.push_back(std::move(const_cast<std::unique_ptr<hook<T>>&>(h)));
}

template <typename T>
template <typename Hook> requires gse::is_hook<Hook, T>
auto gse::hookable<T>::add_hook() -> void {
	m_hooks.push_back(std::make_unique<Hook>(static_cast<T*>(this)));
}

template <typename T>
template <typename Hook, typename... Args>
	requires gse::is_hook<Hook, T>
auto gse::hookable<T>::add_hook(Args&&... args) -> void {
	m_hooks.push_back(std::make_unique<Hook>(static_cast<T*>(this), std::forward<Args>(args)...));
}

template <typename T>
auto gse::hookable<T>::initialize() -> void {
	for (auto& hook : m_hooks) {
		hook->initialize();
	}
}

template <typename T>
auto gse::hookable<T>::update() -> void {
	for (auto& hook : m_hooks) {
		hook->update();
	}
}

template <typename T>
auto gse::hookable<T>::render() -> void {
	for (auto& hook : m_hooks) {
		hook->render();
	}
}

template <typename T>
auto gse::hookable<T>::shutdown() -> void {
	for (auto& hook : m_hooks) {
		hook->shutdown();
	}
}

template <typename T>
auto gse::hookable<T>::cycle(const std::function<bool()>& condition) const -> void {
	initialize();
	while (condition()) {
		update();
		render();
	}
	shutdown();
}
