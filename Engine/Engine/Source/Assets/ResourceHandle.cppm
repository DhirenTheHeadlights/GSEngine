export module gse.assets:resource_handle;

import std;

import gse.core;
import gse.assert;

import :asset_format;

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
	struct generation {
		std::shared_ptr<const T> resource;
		std::uint32_t version = 0;
	};

	template <typename T>
	struct acquired_resource {
		std::shared_ptr<const T> resource;
		std::uint32_t version = 0;
	};

	template <typename T>
	struct resource_slot {
		std::unique_ptr<T> pending_resource;
		std::atomic<std::shared_ptr<const generation<T>>> current;
		std::atomic<std::shared_ptr<const asset_error>> error;
		std::atomic<state> current_state = state::unloaded;
		std::filesystem::path path;
		std::uint32_t next_version = 0;
		std::uint64_t requested_reload = 0;
		std::uint64_t started_reload = 0;
		bool load_in_flight = false;

		resource_slot(
			std::unique_ptr<T>&& resource,
			state initial_state,
			const std::filesystem::path& baked_path
		);
	};

	template <typename T>
	class handle : identifiable_owned {
	public:
		handle() = default;

		explicit handle(
			id resource_id
		);

		handle(
			id resource_id,
			std::shared_ptr<resource_slot<T>> slot
		);

		[[nodiscard]] auto acquire() const -> acquired_resource<T>;

		[[nodiscard]] auto resolve() const -> std::shared_ptr<const T>;

		[[nodiscard]] auto state() const -> resource::state;

		[[nodiscard]] auto error() const -> std::shared_ptr<const asset_error>;

		[[nodiscard]] auto valid() const -> bool;

		[[nodiscard]] auto id() const -> id;

		[[nodiscard]] auto version() const -> std::uint32_t;

		[[nodiscard]] auto operator==(
			const handle& other
		) const -> bool;

		[[nodiscard]] auto operator!=(
			const handle& other
		) const -> bool;

		explicit operator bool() const;

	private:
		std::shared_ptr<resource_slot<T>> m_slot;
	};

	template <typename>
	constexpr bool is_handle_v = false;

	template <typename T>
	constexpr bool is_handle_v<handle<T>> = true;
}

template <typename T>
gse::resource::resource_slot<T>::resource_slot(std::unique_ptr<T>&& resource, const state initial_state, const std::filesystem::path& baked_path) : pending_resource(std::move(resource)), current_state(initial_state), path(baked_path) {
	if (initial_state == state::loaded && pending_resource) {
		auto immutable = std::shared_ptr<const T>(std::move(pending_resource));
		current.store(
			std::make_shared<const generation<T>>(
				generation<T>{
					.resource = std::move(immutable),
					.version = next_version
				}
			),
			std::memory_order_release
		);
		++next_version;
	}
}

template <typename T>
gse::resource::handle<T>::handle(const gse::id resource_id) : identifiable_owned(resource_id) {}

template <typename T>
gse::resource::handle<T>::handle(const gse::id resource_id, std::shared_ptr<resource_slot<T>> slot) : identifiable_owned(resource_id), m_slot(std::move(slot)) {}

template <typename T>
auto gse::resource::handle<T>::acquire() const -> acquired_resource<T> {
	assert(m_slot != nullptr, "acquire() on unresolved resource handle: id {}", owner_id());
	const auto generation = m_slot->current.load(std::memory_order_acquire);
	assert(generation && generation->resource, "acquire() on unavailable resource handle: id {}", owner_id());
	return {
		.resource = generation->resource,
		.version = generation->version
	};
}

template <typename T>
auto gse::resource::handle<T>::resolve() const -> std::shared_ptr<const T> {
	return acquire().resource;
}

template <typename T>
auto gse::resource::handle<T>::state() const -> resource::state {
	return m_slot ? m_slot->current_state.load(std::memory_order_acquire) : state::unloaded;
}

template <typename T>
auto gse::resource::handle<T>::error() const -> std::shared_ptr<const asset_error> {
	return m_slot ? m_slot->error.load(std::memory_order_acquire) : nullptr;
}

template <typename T>
auto gse::resource::handle<T>::valid() const -> bool {
	return m_slot && m_slot->current.load(std::memory_order_acquire) != nullptr;
}

template <typename T>
auto gse::resource::handle<T>::id() const -> gse::id {
	return owner_id();
}

template <typename T>
auto gse::resource::handle<T>::version() const -> std::uint32_t {
	const auto current = m_slot ? m_slot->current.load(std::memory_order_acquire) : nullptr;
	return current ? current->version : 0;
}

template <typename T>
auto gse::resource::handle<T>::operator==(const handle& other) const -> bool {
	return owner_id() == other.owner_id();
}

template <typename T>
auto gse::resource::handle<T>::operator!=(const handle& other) const -> bool {
	return !(*this == other);
}

template <typename T>
gse::resource::handle<T>::operator bool() const {
	return valid();
}
