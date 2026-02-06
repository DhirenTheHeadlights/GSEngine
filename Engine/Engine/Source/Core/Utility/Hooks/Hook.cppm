export module gse.utility:hook;

import std;

import :id;

import :hook_base;

export namespace gse {
	template <typename T>
	class hook : public hook_base<T> {
	public:
		explicit hook(
			T* owner
		) : hook_base<T>(owner) {}

		virtual auto initialize(
		) -> void {}

		virtual auto update(
		) -> void {}

		virtual auto render(
		) -> void {}

		virtual auto shutdown(
		) -> void {}
	};
}
