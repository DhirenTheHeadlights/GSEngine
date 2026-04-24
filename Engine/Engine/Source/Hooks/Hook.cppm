export module gse.hooks:hook;

import std;

import gse.core;

import :hook_base;

export namespace gse {
	template <typename T>
	class hook : public hook_base<T> {
	public:
		explicit hook(
			T* owner
		);

		virtual auto initialize(
		) -> void;

		virtual auto update(
		) -> void;

		virtual auto render(
		) -> void;

		virtual auto shutdown(
		) -> void;
	};

	template <typename Hook, typename T>
	concept is_hook = std::derived_from<Hook, hook<T>>;
}

template <typename T>
gse::hook<T>::hook(T* owner) : hook_base<T>(owner) {}

template <typename T>
auto gse::hook<T>::initialize() -> void {}

template <typename T>
auto gse::hook<T>::update() -> void {}

template <typename T>
auto gse::hook<T>::render() -> void {}

template <typename T>
auto gse::hook<T>::shutdown() -> void {}
