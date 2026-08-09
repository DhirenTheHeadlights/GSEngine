export module gse.ecs:registries;

import std;

import gse.assert;
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

		[[nodiscard]] auto contains(
			id type
		) const -> bool;

		auto state_ptr(
			this auto& self,
			id type
		) -> decltype(auto);

		auto state_snapshot_ptr(
			id type
		) const -> const void*;

		auto clear() -> void;

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

		auto clear() -> void;

	private:
		std::unordered_map<id, void*> m_slots;
	};

	class channel_registry;

	class channel_writer {
	public:
		explicit channel_writer(
			channel_registry* registry
		);

		template <typename T>
		auto push(
			T item
		) -> void;

		template <promiseable T>
		auto push(
			T item
		) -> channel_future<typename T::result_type>;

	private:
		channel_registry* m_registry = nullptr;
	};

	class channel_registry {
	public:
		template <typename T>
		auto ensure_typed() -> typed_channel<T>&;

		template <typename T>
		auto ensure_same_frame() -> same_frame_typed_channel<T>&;

		auto flip_all() -> void;

		auto make_writer() -> channel_writer;

		auto clear() -> void;

		template <typename T>
		requires is_same_frame_channel_v<T>
		auto drain() -> std::vector<T>;

	private:
		std::unordered_map<id, std::unique_ptr<channel_base>> m_channels;
		mutable std::mutex m_mutex;
	};

	template <typename T, typename... Us>
	constexpr bool channel_pack_contains_v = (std::is_same_v<T, Us> || ...);

	template <typename... Ts>
	class channel_read {
	public:
		channel_read() = default;

		explicit channel_read(
			channel_registry& channels
		);

		template <typename... Us>
		requires (channel_pack_contains_v<Ts, Us...> && ...)
		channel_read(
			const channel_read<Us...>& wider
		);

		template <typename T>
		auto of() const -> channel_read_guard<T>;

	private:
		channel_registry* m_channels = nullptr;

		template <typename... Us>
		friend class channel_read;
	};

	template <typename... Ts>
	class channel_write {
	public:
		channel_write() = default;

		explicit channel_write(
			channel_writer& writer
		);

		template <typename... Us>
		requires (channel_pack_contains_v<Ts, Us...> && ...)
		channel_write(
			const channel_write<Us...>& wider
		);

		[[nodiscard]] auto bound() const -> bool;

		template <typename T>
		auto push(
			T item
		) const -> void;

		template <promiseable T>
		auto push(
			T item
		) const -> channel_future<typename T::result_type>;

	private:
		channel_writer* m_writer = nullptr;

		template <typename... Us>
		friend class channel_write;
	};

	template <typename T>
	constexpr bool is_channel_read_v = false;

	template <typename... Ts>
	constexpr bool is_channel_read_v<channel_read<Ts...>> = true;

	template <typename T>
	constexpr bool is_channel_write_v = false;

	template <typename... Ts>
	constexpr bool is_channel_write_v<channel_write<Ts...>> = true;

	template <typename T>
	struct channel_pack_ids;

	template <typename... Ts>
	struct channel_pack_ids<channel_read<Ts...>> {
		static auto append(
			std::vector<id>& out
		) -> void;
	};

	template <typename... Ts>
	struct channel_pack_ids<channel_write<Ts...>> {
		static auto append(
			std::vector<id>& out
		) -> void;
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
	using result_t = std::conditional_t<std::is_const_v<std::remove_reference_t<decltype(self)>>, const void*, void*>;
	const auto it = self.m_slots.find(type);
	if (it == self.m_slots.end()) {
		return static_cast<result_t>(nullptr);
	}
	return static_cast<result_t>(it->second.live);
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
	using result_t = std::conditional_t<std::is_const_v<std::remove_reference_t<decltype(self)>>, const void*, void*>;
	const auto it = self.m_slots.find(type);
	if (it == self.m_slots.end()) {
		return static_cast<result_t>(nullptr);
	}
	return static_cast<result_t>(it->second);
}

auto gse::resource_registry::clear() -> void {
	m_slots.clear();
}

gse::channel_writer::channel_writer(channel_registry* registry) : m_registry(registry) {
}

template <typename T>
auto gse::channel_writer::push(T item) -> void {
	if constexpr (is_same_frame_channel_v<T>) {
		m_registry->ensure_same_frame<T>().push(std::move(item));
	}
	else {
		m_registry->ensure_typed<T>().data.push(std::move(item));
	}
}

template <gse::promiseable T>
auto gse::channel_writer::push(T item) -> channel_future<typename T::result_type> {
	auto [future, promise] = make_promise<typename T::result_type>();
	item.promise = std::move(promise);
	if constexpr (is_same_frame_channel_v<T>) {
		m_registry->ensure_same_frame<T>().push(std::move(item));
	}
	else {
		m_registry->ensure_typed<T>().data.push(std::move(item));
	}
	return future;
}

template <typename T>
auto gse::channel_registry::ensure_typed() -> typed_channel<T>& {
	std::lock_guard lock(m_mutex);
	const auto type_id = id_of<T>();
	auto it = m_channels.find(type_id);
	if (it == m_channels.end()) {
		it = m_channels.emplace(type_id, std::make_unique<typed_channel<T>>()).first;
	}
	return static_cast<typed_channel<T>&>(*it->second);
}

template <typename T>
auto gse::channel_registry::ensure_same_frame() -> same_frame_typed_channel<T>& {
	std::lock_guard lock(m_mutex);
	const auto type_id = id_of<T>();
	auto it = m_channels.find(type_id);
	if (it == m_channels.end()) {
		it = m_channels.emplace(type_id, std::make_unique<same_frame_typed_channel<T>>()).first;
	}
	return static_cast<same_frame_typed_channel<T>&>(*it->second);
}

template <typename T>
requires gse::is_same_frame_channel_v<T>
auto gse::channel_registry::drain() -> std::vector<T> {
	return ensure_same_frame<T>().drain();
}

auto gse::channel_registry::flip_all() -> void {
	std::lock_guard lock(m_mutex);
	for (const auto& ch : std::views::values(m_channels)) {
		ch->flip();
	}
}

auto gse::channel_registry::make_writer() -> channel_writer {
	return channel_writer{ this };
}

auto gse::channel_registry::clear() -> void {
	std::lock_guard lock(m_mutex);
	m_channels.clear();
}

template <typename... Ts>
gse::channel_read<Ts...>::channel_read(channel_registry& channels) : m_channels(&channels) {
}

template <typename... Ts>
template <typename... Us>
requires (gse::channel_pack_contains_v<Ts, Us...> && ...)
gse::channel_read<Ts...>::channel_read(const channel_read<Us...>& wider) : m_channels(wider.m_channels) {
}

template <typename... Ts>
template <typename T>
auto gse::channel_read<Ts...>::of() const -> channel_read_guard<T> {
	static_assert(
		channel_pack_contains_v<T, Ts...>,
		"channel type is not declared in this channel_read<...> pack; add it to the parameter so the scheduler sees the edge"
	);
	static_assert(
		!is_same_frame_channel_v<T>,
		"same_frame_channel<T> cannot be consumed via channel_read — the writer routes through "
		"ensure_same_frame<T>() while channel_read goes through ensure_typed<T>(), which collide on id_of<T>() "
		"and produce undefined behavior via a wrong-type static_cast. "
		"Either drop the [[= same_frame_channel]] annotation (and accept normal next-tick double-buffered delivery), "
		"or consume via scheduler::drain_channel<T>() instead."
	);
	assert(m_channels != nullptr, "channel_read used before it was bound to a channel registry");
	return channel_read_guard<T>(m_channels->ensure_typed<T>().data.read_raw());
}

template <typename... Ts>
gse::channel_write<Ts...>::channel_write(channel_writer& writer) : m_writer(&writer) {
}

template <typename... Ts>
auto gse::channel_write<Ts...>::bound() const -> bool {
	return m_writer != nullptr;
}

template <typename... Ts>
template <typename... Us>
requires (gse::channel_pack_contains_v<Ts, Us...> && ...)
gse::channel_write<Ts...>::channel_write(const channel_write<Us...>& wider) : m_writer(wider.m_writer) {
}

template <typename... Ts>
template <typename T>
auto gse::channel_write<Ts...>::push(T item) const -> void {
	static_assert(
		channel_pack_contains_v<T, Ts...>,
		"channel type is not declared in this channel_write<...> pack; add it to the parameter so the scheduler sees the edge"
	);
	assert(m_writer != nullptr, "channel_write used before it was bound to a channel writer");
	m_writer->push<T>(std::move(item));
}

template <typename... Ts>
template <gse::promiseable T>
auto gse::channel_write<Ts...>::push(T item) const -> channel_future<typename T::result_type> {
	static_assert(
		channel_pack_contains_v<T, Ts...>,
		"channel type is not declared in this channel_write<...> pack; add it to the parameter so the scheduler sees the edge"
	);
	assert(m_writer != nullptr, "channel_write used before it was bound to a channel writer");
	return m_writer->push<T>(std::move(item));
}

template <typename... Ts>
auto gse::channel_pack_ids<gse::channel_read<Ts...>>::append(std::vector<id>& out) -> void {
	(out.push_back(trace_id<Ts>()), ...);
}

template <typename... Ts>
auto gse::channel_pack_ids<gse::channel_write<Ts...>>::append(std::vector<id>& out) -> void {
	(out.push_back(trace_id<Ts>()), ...);
}
