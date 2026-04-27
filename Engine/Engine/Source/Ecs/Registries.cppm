export module gse.ecs:registries;

import std;

import gse.core;
import gse.concurrency;

export namespace gse {
	class state_registry {
	public:
		struct slot {
			void* live = nullptr;
			const void* snapshot = nullptr;
		};

		auto register_state(
			id type,
			void* live_ptr,
			const void* snapshot_ptr
		) -> void;

		auto contains(
			id type
		) const -> bool;

		auto state_ptr(
			this auto& self,
			id type
		) -> decltype(auto);

		auto state_snapshot_ptr(
			id type
		) const -> const void*;

		auto clear(
		) -> void;

	private:
		std::unordered_map<id, slot> m_slots;
	};

	class resource_registry {
	public:
		auto register_resource(
			id type,
			void* ptr
		) -> void;

		auto contains(
			id type
		) const -> bool;

		auto resources_ptr(
			this auto& self,
			id type
		) -> decltype(auto);

		auto clear(
		) -> void;

	private:
		std::unordered_map<id, void*> m_slots;
	};

	class channel_writer {
	public:
		template <typename F>
		explicit channel_writer(
			F&& fn
		);

		channel_writer(
			channel_writer&&
		) noexcept;

		auto operator=(
			channel_writer&&
		) noexcept -> channel_writer&;

		~channel_writer(
		);

		channel_writer(
			const channel_writer&
		) = delete;

		auto operator=(
			const channel_writer&
		) -> channel_writer& = delete;

		template <typename T>
		auto push(
			T item
		) -> void;

		template <promiseable T>
		auto push(
			T item
		) -> channel_future<typename T::result_type>;

	private:
		auto invoke_push(
			id idx,
			std::any value,
			channel_factory_fn factory
		) -> void;

		void* m_ctx = nullptr;
		void(*m_invoke)(void*, id, std::any, channel_factory_fn) = nullptr;
		void(*m_destroy)(void*) noexcept = nullptr;
	};

	class channel_registry {
	public:
		auto ensure(
			id type,
			channel_factory_fn factory
		) -> channel_base&;

		auto find(
			this auto& self,
			id type
		) -> decltype(auto);

		auto snapshot_data(
			id type
		) const -> const void*;

		auto take_snapshot_all(
		) -> void;

		auto make_writer(
		) -> channel_writer;

		auto clear(
		) -> void;

	private:
		std::unordered_map<id, std::unique_ptr<channel_base>> m_channels;
		mutable std::mutex m_mutex;
	};
}

auto gse::state_registry::register_state(const id type, void* live_ptr, const void* snapshot_ptr) -> void {
	m_slots[type] = {
		.live = live_ptr,
		.snapshot = snapshot_ptr,
	};
}

auto gse::state_registry::contains(const id type) const -> bool {
	return m_slots.contains(type);
}

auto gse::state_registry::state_ptr(this auto& self, const id type) -> decltype(auto) {
	using ret_t = std::conditional_t<std::is_const_v<std::remove_reference_t<decltype(self)>>, const void*, void*>;
	const auto it = self.m_slots.find(type);
	if (it == self.m_slots.end()) {
		return ret_t{ nullptr };
	}
	return static_cast<ret_t>(it->second.live);
}

auto gse::state_registry::state_snapshot_ptr(const id type) const -> const void* {
	const auto it = m_slots.find(type);
	if (it == m_slots.end()) {
		return nullptr;
	}
	return it->second.snapshot;
}

auto gse::state_registry::clear() -> void {
	m_slots.clear();
}

auto gse::resource_registry::register_resource(const id type, void* ptr) -> void {
	m_slots[type] = ptr;
}

auto gse::resource_registry::contains(const id type) const -> bool {
	return m_slots.contains(type);
}

auto gse::resource_registry::resources_ptr(this auto& self, const id type) -> decltype(auto) {
	using ret_t = std::conditional_t<std::is_const_v<std::remove_reference_t<decltype(self)>>, const void*, void*>;
	const auto it = self.m_slots.find(type);
	if (it == self.m_slots.end()) {
		return ret_t{ nullptr };
	}
	return static_cast<ret_t>(it->second);
}

auto gse::resource_registry::clear() -> void {
	m_slots.clear();
}

template <typename F>
gse::channel_writer::channel_writer(F&& fn) {
	using closure = std::decay_t<F>;
	m_ctx = new closure(std::forward<F>(fn));
	m_invoke = +[](void* p, id idx, std::any value, channel_factory_fn factory) {
		(*static_cast<closure*>(p))(idx, std::move(value), std::move(factory));
	};
	m_destroy = +[](void* p) noexcept {
		delete static_cast<closure*>(p);
	};
}

template <typename T>
auto gse::channel_writer::push(T item) -> void {
	invoke_push(
		id_of<T>(),
		std::any(std::move(item)),
		+[]() -> std::unique_ptr<channel_base> {
			return std::make_unique<typed_channel<T>>();
		}
	);
}

template <gse::promiseable T>
auto gse::channel_writer::push(T item) -> channel_future<typename T::result_type> {
	auto [future, promise] = make_promise<typename T::result_type>();
	item.promise = std::move(promise);
	invoke_push(
		id_of<T>(),
		std::any(std::move(item)),
		+[]() -> std::unique_ptr<channel_base> {
			return std::make_unique<typed_channel<T>>();
		}
	);
	return future;
}

gse::channel_writer::channel_writer(channel_writer&& other) noexcept
	: m_ctx(other.m_ctx), m_invoke(other.m_invoke), m_destroy(other.m_destroy) {
	other.m_ctx = nullptr;
	other.m_invoke = nullptr;
	other.m_destroy = nullptr;
}

auto gse::channel_writer::operator=(channel_writer&& other) noexcept -> channel_writer& {
	if (this != &other) {
		if (m_destroy && m_ctx) {
			m_destroy(m_ctx);
		}
		m_ctx = other.m_ctx;
		m_invoke = other.m_invoke;
		m_destroy = other.m_destroy;
		other.m_ctx = nullptr;
		other.m_invoke = nullptr;
		other.m_destroy = nullptr;
	}
	return *this;
}

gse::channel_writer::~channel_writer() {
	if (m_destroy && m_ctx) {
		m_destroy(m_ctx);
	}
}

auto gse::channel_writer::invoke_push(const id idx, std::any value, const channel_factory_fn factory) -> void {
	m_invoke(m_ctx, idx, std::move(value), std::move(factory));
}

auto gse::channel_registry::ensure(const id type, const channel_factory_fn factory) -> channel_base& {
	std::lock_guard lock(m_mutex);
	auto it = m_channels.find(type);
	if (it == m_channels.end()) {
		it = m_channels.emplace(type, factory()).first;
	}
	return *it->second;
}

auto gse::channel_registry::find(this auto& self, const id type) -> decltype(auto) {
	using ret_t = std::conditional_t<std::is_const_v<std::remove_reference_t<decltype(self)>>, const channel_base*, channel_base*>;
	std::lock_guard lock(self.m_mutex);
	const auto it = self.m_channels.find(type);
	if (it == self.m_channels.end()) {
		return ret_t{ nullptr };
	}
	return static_cast<ret_t>(it->second.get());
}

auto gse::channel_registry::snapshot_data(const id type) const -> const void* {
	std::lock_guard lock(m_mutex);
	const auto it = m_channels.find(type);
	if (it == m_channels.end()) {
		return nullptr;
	}
	return it->second->snapshot_data();
}

auto gse::channel_registry::take_snapshot_all() -> void {
	std::lock_guard lock(m_mutex);
	for (const auto& ch : std::views::values(m_channels)) {
		ch->take_snapshot();
	}
}

auto gse::channel_registry::make_writer() -> channel_writer {
	return channel_writer([this](const id type, std::any item, const channel_factory_fn factory) {
		std::lock_guard lock(m_mutex);
		auto it = m_channels.find(type);
		if (it == m_channels.end()) {
			it = m_channels.emplace(type, factory()).first;
		}
		it->second->push_any(std::move(item));
	});
}

auto gse::channel_registry::clear() -> void {
	std::lock_guard lock(m_mutex);
	m_channels.clear();
}
