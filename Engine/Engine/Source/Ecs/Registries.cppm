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
		auto ensure_typed(
		) -> typed_channel<T>&;

		template <typename T>
		auto ensure_same_frame(
		) -> same_frame_typed_channel<T>&;

		auto flip_all(
		) -> void;

		auto make_writer(
		) -> channel_writer;

		auto clear(
		) -> void;

		template <typename T>
			requires is_same_frame_channel_v<T>
		auto drain(
		) -> std::vector<T>;

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

gse::channel_writer::channel_writer(channel_registry* registry) : m_registry(registry) {}

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
