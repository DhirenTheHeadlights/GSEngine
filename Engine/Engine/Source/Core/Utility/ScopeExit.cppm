export module gse.utility:scope_exit;

import std;

import :non_copyable;

export namespace gse {
	template <class F>
	[[nodiscard]] auto make_scope_exit(
		F&& f
	) noexcept(std::is_nothrow_constructible_v<std::decay_t<F>, F&&>);
}

namespace gse {
	template <class F>
	class scope_exit : non_copyable {
	public:
		using func_type = F;

		explicit scope_exit(
			F&& f
		) noexcept(std::is_nothrow_move_constructible_v<F>);

		scope_exit(
			scope_exit&& other
		) noexcept(std::is_nothrow_move_constructible_v<F>);

		~scope_exit() noexcept override;

		auto release(
		) noexcept -> void;

		auto active(
		) const noexcept -> bool;
	private:
		F m_func;
		bool m_active = true;
	};
}

template <class F>
auto gse::make_scope_exit(F&& f) noexcept(std::is_nothrow_constructible_v<std::decay_t<F>, F&&>) {
	return scope_exit<std::decay_t<F>>(std::forward<F>(f));
}

template <class F>
gse::scope_exit<F>::scope_exit(F&& f) noexcept(std::is_nothrow_move_constructible_v<F>): m_func(std::move(f)), m_active(true) {}

template <class F>
gse::scope_exit<F>::scope_exit(scope_exit&& other) noexcept(std::is_nothrow_move_constructible_v<F>): m_func(std::move(other.m_func)), m_active(other.m_active) {
	other.m_active = false;
}

template <class F>
gse::scope_exit<F>::~scope_exit() noexcept {
	if (!m_active) {
		return;
	}
	try {
		std::invoke(m_func);
	} catch (...) {
		std::terminate(); 
	}
}

template <class F>
auto gse::scope_exit<F>::release() noexcept -> void {
	m_active = false;
}

template <class F>
auto gse::scope_exit<F>::active() const noexcept -> bool {
	return m_active;
}
