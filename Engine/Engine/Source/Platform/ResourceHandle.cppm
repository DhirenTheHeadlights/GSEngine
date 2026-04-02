export module gse.platform:resource_handle;

import std;

import gse.utility;
import gse.assert;

export namespace gse::resource {
	enum struct state {
		unloaded,
		queued,
		loading,
		loaded,
		reloading,
		failed
	};

	template <typename T>
	struct resource_slot : non_copyable {
		double_buffer<std::unique_ptr<T>> resource;
		std::atomic<state> current_state;
		std::filesystem::path path;
		std::atomic<std::uint32_t> version{0};

		resource_slot(std::unique_ptr<T>&& res, const state s, const std::filesystem::path& p)
			: current_state(s), path(p) {
			resource.write() = std::move(res);
			resource.publish();
		}

		resource_slot(resource_slot&& other) noexcept
			: current_state(other.current_state.load(std::memory_order_relaxed)),
			path(std::move(other.path)),
			version(other.version.load(std::memory_order_relaxed)) {
			resource.write() = std::move(const_cast<std::unique_ptr<T>&>(other.resource.read()));
			resource.publish();
		}

		auto operator=(resource_slot&& other) noexcept -> resource_slot& {
			if (this != &other) {
				resource.write() = std::move(const_cast<std::unique_ptr<T>&>(other.resource.read()));
				resource.publish();
				current_state.store(other.current_state.load(std::memory_order_relaxed));
				path = std::move(other.path);
				version.store(other.version.load(std::memory_order_relaxed));
			}
			return *this;
		}
	};

	template <typename T>
	class handle : identifiable_owned {
	public:
		handle() = default;
		handle(
			id resource_id,
			resource_slot<T>* slot
		);
		handle(
			id resource_id,
			resource_slot<T>* slot,
			std::uint32_t version
		);

		[[nodiscard]] auto resolve(
		) const -> T*;

		[[nodiscard]] auto state(
		) const -> resource::state;

		[[nodiscard]] auto valid(
		) const -> bool;

		[[nodiscard]] auto id(
		) const -> id;

		[[nodiscard]] auto version(
		) const -> std::uint32_t;

		[[nodiscard]] auto is_current(
		) const -> bool;

		[[nodiscard]] auto operator->(
		) const -> T*;

		[[nodiscard]] auto operator*(
		) const -> T&;

		[[nodiscard]] auto operator==(
			const handle& other
		) const -> bool;

		[[nodiscard]] auto operator!=(
			const handle& other
		) const -> bool;

		explicit operator bool(
		) const;
	private:
		resource_slot<T>* m_slot = nullptr;
		mutable std::uint32_t m_version = 0;
	};
}

template <typename T>
gse::resource::handle<T>::handle(const gse::id resource_id, resource_slot<T>* slot) : identifiable_owned(resource_id), m_slot(slot) {
	if (m_slot) {
		m_version = m_slot->version.load(std::memory_order_acquire);
	}
}

template <typename T>
gse::resource::handle<T>::handle(const gse::id resource_id, resource_slot<T>* slot, const std::uint32_t version) : identifiable_owned(resource_id), m_slot(slot), m_version(version) {}

template <typename T>
auto gse::resource::handle<T>::resolve() const -> T* {
	if (!m_slot) return nullptr;
	m_version = m_slot->version.load(std::memory_order_acquire);
	const auto& ptr = m_slot->resource.read();
	return ptr ? ptr.get() : nullptr;
}

template <typename T>
auto gse::resource::handle<T>::state() const -> resource::state {
	if (!m_slot) return state::unloaded;
	return m_slot->current_state.load(std::memory_order_acquire);
}

template <typename T>
auto gse::resource::handle<T>::valid() const -> bool {
	const auto s = state();
	return m_slot && (s == state::loaded || s == state::reloading);
}

template <typename T>
auto gse::resource::handle<T>::id() const -> gse::id {
	return owner_id();
}

template <typename T>
auto gse::resource::handle<T>::version() const -> std::uint32_t {
	return m_version;
}

template <typename T>
auto gse::resource::handle<T>::is_current() const -> bool {
	if (!m_slot) return false;
	return m_version == m_slot->version.load(std::memory_order_acquire);
}

template <typename T>
auto gse::resource::handle<T>::operator->() const -> T* {
	T* r = resolve();
	assert(r, std::source_location::current(), "Attempting to access an unloaded or invalid resource with ID: {}", owner_id());
	return r;
}

template <typename T>
auto gse::resource::handle<T>::operator*() const -> T& {
	T* r = resolve();
	assert(r, std::source_location::current(), "Attempting to dereference an unloaded or invalid resource with ID: {}", owner_id());
	return *r;
}

template <typename T>
auto gse::resource::handle<T>::operator==(const handle& other) const -> bool {
	if (!valid() || !other.valid()) {
		return false;
	}

	return owner_id() == other.owner_id() && m_slot == other.m_slot;
}

template <typename T>
auto gse::resource::handle<T>::operator!=(const handle& other) const -> bool {
	return !(*this == other);
}

template <typename T>
gse::resource::handle<T>::operator bool() const {
	return valid();
}
