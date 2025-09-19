export module gse.utility:identifiable_owned;

import std;

import :id;
import :identifiable;

export namespace gse {
	template <typename T>
	class identifiable_owned {
	public:
		identifiable_owned() = default;
		explicit identifiable_owned(const id_t<T>& owner_id);

		auto owner_id() const -> const id_t<T>&;
		auto operator==(const identifiable_owned& other) const -> bool = default;

		auto swap(const id_t<T>& new_parent_id) -> void;
		auto swap(const identifiable& new_parent) -> void;
	private:
		id_t<T> m_owner_id;
	};
}

template <typename T>
gse::identifiable_owned<T>::identifiable_owned(const id_t<T>& owner_id) : m_owner_id(owner_id) {}

template <typename T>
auto gse::identifiable_owned<T>::owner_id() const -> const id_t<T>& {
	return m_owner_id;
}

template <typename T>
auto gse::identifiable_owned<T>::swap(const id_t<T>& new_parent_id) -> void {
	m_owner_id = new_parent_id;
}

template <typename T>
auto gse::identifiable_owned<T>::swap(const identifiable& new_parent) -> void {
	swap(new_parent.id());
}