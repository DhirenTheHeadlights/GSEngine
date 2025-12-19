export module gse.utility:hook_base;

import std;

import :id;

export namespace gse {
	template <is_identifiable T>
	class hook_base {
	public:
		explicit hook_base(
			T* owner
		);

		virtual ~hook_base(
		) = default;

		auto owner_id(
		) const -> id;
	protected:
		T* m_owner;
	};
}

template <gse::is_identifiable T>
gse::hook_base<T>::hook_base(T* owner) : m_owner(owner) {
}

template <gse::is_identifiable T>
auto gse::hook_base<T>::owner_id() const -> id {
	return m_owner->id();
}
