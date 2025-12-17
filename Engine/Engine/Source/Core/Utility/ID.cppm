export module gse.utility:id;

import std;

import gse.assert;
import gse.physics.math;

import :double_buffer;

namespace gse {
	using uuid = std::uint64_t;
}

export namespace gse {
	class id;

	auto generate_id(
		std::string_view tag
	) -> id;

	auto generate_id(
		std::uint64_t number
	) -> id;

	auto generate_temp_id(
		uuid number, 
		std::string_view tag
	) -> id;

	auto find(
		uuid number
	) -> id;

	auto find(
		std::string_view tag
	) -> id;

	auto try_find(
		std::string_view tag
	) -> std::optional<id>;

	auto try_find(
		uuid number
	) -> std::optional<id>;

	auto find_or_generate_id(
		std::string_view tag
	) -> id;

	auto find_or_generate_id(
		uuid number
	) -> id;

	auto exists(
		uuid number
	) -> bool;

	auto exists(
		std::string_view tag
	) -> bool;

	auto tag(
		uuid number
	) -> std::string_view;

	auto number(
		std::string_view tag
	) -> uuid;
}

export namespace gse {
	class id {
	public:
		id() = default;

		auto operator==(
			id other
		) const -> bool;

		auto number(
		) const -> uuid;

		auto tag(
		) const -> std::string_view;

		auto exists(
		) const -> bool;

		auto reset(
		) -> void;
	private:
		explicit id(
			uuid id
		);

		uuid m_number = std::numeric_limits<uuid>::max();

		friend auto generate_id(std::string_view tag) -> id;
		friend auto generate_id(std::uint64_t number) -> id;
		friend auto generate_temp_id(uuid number, std::string_view tag) -> id;
	};
}

template <>
struct std::formatter<gse::id> {
	static constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
	static auto format(gse::id value, std::format_context& ctx) {
		return std::format_to(ctx.out(), "[{}: {}]", value.number(), value.tag());
	}
};

auto gse::id::operator==(const id other) const -> bool {
	if (!exists() || !other.exists()) {
		return false;
	}

	return m_number == other.m_number;
}

auto gse::id::number() const -> uuid {
	return m_number;
}

auto gse::id::tag() const -> std::string_view {
	return gse::tag(m_number);
}

auto gse::id::exists() const -> bool {
	return m_number != std::numeric_limits<uuid>::max();
}

auto gse::id::reset() -> void {
	this->m_number = std::numeric_limits<uuid>::max();
}

gse::id::id(const uuid id) : m_number(id) {}

export namespace gse {
	class identifiable {
	public:
		explicit identifiable(
			const std::string& tag
		);

		explicit identifiable(
			const std::filesystem::path& path
		);

		auto id(
		) const -> id;

		auto operator==(
			const identifiable& other
		) const -> bool = default;
	private:
		gse::id m_id;

		static auto filename(
			const std::filesystem::path& path
		) -> std::string;
	};
}

gse::identifiable::identifiable(const std::string& tag) : m_id(generate_id(tag)) {}

gse::identifiable::identifiable(const std::filesystem::path& path) : m_id(generate_id(filename(path))) {}

auto gse::identifiable::id() const -> gse::id {
	return m_id;
}

auto gse::identifiable::filename(const std::filesystem::path& path) -> std::string {
	std::string name = path.filename().string();
	if (const size_t dot_pos = name.find_first_of('.'); dot_pos != std::string::npos) {
		return name.substr(0, dot_pos);
	}
	return name;
}

export namespace gse {
	class identifiable_owned {
	public:
		identifiable_owned(
		) = default;

		explicit identifiable_owned(
			id owner_id
		);

		auto owner_id(
		) const -> id;

		auto operator==(
			const identifiable_owned& other
		) const -> bool = default;

		auto swap_parent(
			id new_parent_id
		) -> void;

		auto swap_parent(
			const identifiable& new_parent
		) -> void;
	private:
		id m_owner_id;
	};
}

gse::identifiable_owned::identifiable_owned(const id owner_id) : m_owner_id(owner_id) {}

auto gse::identifiable_owned::owner_id() const -> id {
	return m_owner_id;
}

auto gse::identifiable_owned::swap_parent(const id new_parent_id) -> void {
	m_owner_id = new_parent_id;
}

auto gse::identifiable_owned::swap_parent(const identifiable& new_parent) -> void {
	swap_parent(new_parent.id());
}

export namespace gse {
	struct entity;

	template <typename T>
	concept is_identifiable = std::derived_from<T, identifiable> || std::same_as<T, entity>;

	template <typename T, typename PrimaryIdType = id>
	class id_mapped_collection {
	public:
		auto add(
			const PrimaryIdType& id,
			T object
		) -> T*;

		auto remove(
			const PrimaryIdType& id
		) -> void;

		auto pop(
			const PrimaryIdType& id
		) -> std::optional<T>;

		auto try_get(
			const PrimaryIdType& id
		) -> T*;

		auto try_get(
			const PrimaryIdType& id
		) const -> const T*;

		auto items(
			this auto&& self
		) -> decltype(auto);

		auto contains(
			const PrimaryIdType& id
		) const -> bool;

		auto size(
		) const -> size_t;

		auto clear(
		) noexcept -> void;

		auto transfer_from(
			id_mapped_collection& other
		) -> void;
	private:
		std::vector<T> m_items;
		std::vector<PrimaryIdType> m_ids;
		std::unordered_map<PrimaryIdType, size_t> m_map;
	};
}

template <typename T, typename PrimaryIdType>
auto gse::id_mapped_collection<T, PrimaryIdType>::add(const PrimaryIdType& id, T object) -> T* {
	if (m_map.contains(id)) {
		return nullptr;
	}

	const std::size_t new_index = m_items.size();
	m_map[id] = new_index;
	m_ids.push_back(id);
	return &m_items.emplace_back(std::move(object));
}

template <typename T, typename PrimaryIdType>
auto gse::id_mapped_collection<T, PrimaryIdType>::remove(const PrimaryIdType& id) -> void {
	const auto it = m_map.find(id);
	if (it == m_map.end()) {
		return;
	}

	const size_t index_to_remove = it->second;

	if (const size_t last_index = m_items.size() - 1; index_to_remove != last_index) {
		const PrimaryIdType& last_id = m_ids.back();
		m_items[index_to_remove] = std::move(m_items.back());
		m_ids[index_to_remove] = std::move(m_ids.back());
		m_map[last_id] = index_to_remove;
	}

	m_map.erase(id);
	m_items.pop_back();
	m_ids.pop_back();
}

template <typename T, typename PrimaryIdType>
auto gse::id_mapped_collection<T, PrimaryIdType>::pop(const PrimaryIdType& id) -> std::optional<T> {
	const auto it = m_map.find(id);
	if (it == m_map.end()) {
		return std::nullopt;
	}

	const size_t index_to_pop = it->second;
	T popped_object = std::move(m_items[index_to_pop]);

	remove(id);

	return popped_object;
}

template <typename T, typename PrimaryIdType>
auto gse::id_mapped_collection<T, PrimaryIdType>::try_get(const PrimaryIdType& id) -> T* {
	if (const auto it = m_map.find(id); it != m_map.end()) {
		return &m_items[it->second];
	}
	return nullptr;
}

template <typename T, typename PrimaryIdType>
auto gse::id_mapped_collection<T, PrimaryIdType>::try_get(const PrimaryIdType& id) const -> const T* {
	if (const auto it = m_map.find(id); it != m_map.end()) {
		return &m_items[it->second];
	}
	return nullptr;
}

template <typename T, typename PrimaryIdType>
auto gse::id_mapped_collection<T, PrimaryIdType>::items(this auto&& self) -> decltype(auto) {
	return std::span{ self.m_items };
}

template <typename T, typename PrimaryIdType>
auto gse::id_mapped_collection<T, PrimaryIdType>::contains(const PrimaryIdType& id) const -> bool {
	return m_map.contains(id);
}

template <typename T, typename PrimaryIdType>
auto gse::id_mapped_collection<T, PrimaryIdType>::size() const -> size_t {
	return m_items.size();
}

template <typename T, typename PrimaryIdType>
auto gse::id_mapped_collection<T, PrimaryIdType>::clear() noexcept -> void {
	m_items.clear();
	m_ids.clear();
	m_map.clear();
}

template <typename T, typename PrimaryIdType>
auto gse::id_mapped_collection<T, PrimaryIdType>::transfer_from(id_mapped_collection& other) -> void {
	m_items = std::move(other.m_items);
    m_ids   = std::move(other.m_ids);
    m_map   = std::move(other.m_map);
}

export template <>
struct std::hash<gse::id> {
	auto operator()(const gse::id& id) const noexcept -> std::size_t {
		return std::hash<gse::uuid>{}(id.number());
	}
};

namespace gse {
	struct transparent_hash {
		using is_transparent = void;
		auto operator()(const std::string_view sv) const noexcept { return std::hash<std::string_view>{}(sv); }
	};
	struct transparent_equal {
		using is_transparent = void;
		auto operator()(const std::string_view a, const std::string_view b) const noexcept { return a == b; }
	};

	struct id_registry_data {
		id_mapped_collection<id, uuid> by_uuid;
		std::unordered_map<std::string, uuid, transparent_hash, transparent_equal> tag_to_uuid;
		std::unordered_map<uuid, std::string> uuid_to_tag;
	};

	auto id_registry() -> std::pair<std::shared_mutex&, id_registry_data&> {
		static std::shared_mutex m;
		static id_registry_data instance;
		return { m, instance };
	}
}

auto gse::generate_id(const std::string_view tag) -> id {
	const auto& [mutex, registry] = id_registry();
	std::lock_guard lock(mutex);

	uuid stable_id = 0xcbf29ce484222325ull;
	for (unsigned char c : tag) {
		stable_id ^= c;
		stable_id *= 1099511628211ull;
	}

	if (registry.by_uuid.contains(stable_id)) {
		if (const auto it = registry.uuid_to_tag.find(stable_id); it != registry.uuid_to_tag.end()) {
			assert(
				it->second == tag,
				std::source_location::current(),
				"ID collision for tag {} vs existing tag {}",
				tag,
				it->second
			);

			if (auto* existing = registry.by_uuid.try_get(stable_id)) {
				return *existing;
			}
		}
	}

	const id new_id(stable_id);
	registry.by_uuid.add(stable_id, new_id);
	registry.tag_to_uuid[std::string(tag)] = stable_id;
	registry.uuid_to_tag[stable_id] = std::string(tag);
	return new_id;
}

auto gse::generate_id(const std::uint64_t number) -> id {
	const auto& [mutex, registry] = id_registry();
	std::lock_guard lock(mutex);

	assert(!registry.by_uuid.contains(number), std::source_location::current(), "ID number {} already exists", number);

	const id new_id(number);
	auto tag = std::to_string(number);

	registry.by_uuid.add(number, new_id);
	registry.tag_to_uuid[tag] = number;
	registry.uuid_to_tag[number] = std::move(tag);

	return new_id;
}

auto gse::generate_temp_id(const uuid number, const std::string_view tag) -> id {
	return id(number);
}

auto gse::find(const uuid number) -> id {
	const auto found_id = try_find(number);
	assert(found_id.has_value(), std::source_location::current(), "ID {} not found", number);
	return *found_id;
}

auto gse::find(const std::string_view tag) -> id {
	const auto found_id = try_find(tag);
	assert(found_id.has_value(), std::source_location::current(), "Inconsistent registry state!");
	return *found_id;
}

auto gse::try_find(const std::string_view tag) -> std::optional<id> {
	const auto& [mutex, registry] = id_registry();
	std::shared_lock lock(mutex);

	const auto it = registry.tag_to_uuid.find(tag);
	if (it == registry.tag_to_uuid.end()) {
		return std::nullopt;
	}

	if (id* found_id = registry.by_uuid.try_get(it->second)) {
		return *found_id;
	}

	return std::nullopt;
}

auto gse::try_find(const uuid number) -> std::optional<id> {
	const auto& [mutex, registry] = id_registry();
	std::shared_lock lock(mutex);

	if (id* found_id = registry.by_uuid.try_get(number)) {
		return *found_id;
	}
	return std::nullopt;
}

auto gse::find_or_generate_id(const std::string_view tag) -> id {
	if (exists(tag)) {
		return find(tag);
	}
	return generate_id(tag);
}

auto gse::find_or_generate_id(const uuid number) -> id {
	if (exists(number)) {
		return find(number);
	}
	return generate_id(number);
}

auto gse::exists(const uuid number) -> bool {
	const auto& [mutex, registry] = id_registry();
	std::shared_lock lock(mutex);
	return registry.by_uuid.contains(number);
}

auto gse::exists(const std::string_view tag) -> bool {
	const auto& [mutex, registry] = id_registry();
	std::shared_lock lock(mutex);
	return registry.tag_to_uuid.contains(tag);
}

auto gse::tag(uuid number) -> std::string_view {
	const auto& [mutex, registry] = id_registry();
	std::shared_lock lock(mutex);
	const auto it = registry.uuid_to_tag.find(number);
	assert(it != registry.uuid_to_tag.end(), std::source_location::current(), "Tag for id {} not found", number);
	return it->second;
}

auto gse::number(const std::string_view tag) -> uuid {
	const auto& [mutex, registry] = id_registry();
	std::shared_lock lock(mutex);
	const auto it = registry.tag_to_uuid.find(tag);
	assert(it != registry.tag_to_uuid.end(), std::source_location::current(), "Tag '{}' not found", tag);
	return it->second;
}

export namespace gse {
	template <typename T, typename IdType>
	class double_buffered_id_mapped_queue {
	public:
		using id_type = IdType;
		using value_type = T;

		struct reader {
			double_buffered_id_mapped_queue* parent;

			auto objects(
			) -> std::span<const T>;

			auto try_get(
				const id_type& id
			) const -> const T*;

			auto emplace_from_writer(
				const id_type& id
			) -> const T*;
		};

		struct writer {
			double_buffered_id_mapped_queue* parent;

			template <typename... Args>
			auto emplace_queued(
				const id_type& id,
				Args&&... args
			) -> T*;

			auto activate(
				const id_type& owner
			) -> bool;

			auto remove(
				const id_type& id
			) -> void;

			auto objects(
			) -> std::span<T>;

			auto try_get(
				const id_type& id
			) -> T*;

			auto reader(
			) -> reader;
		};

		auto bind(
			std::size_t read,
			std::size_t write
		) -> std::pair<reader, writer>;

		auto flip(
		) -> void;

		auto clear(
		) noexcept -> void;
	private:
		struct slot {
			id_mapped_collection<T, IdType> active;
			id_mapped_collection<T, IdType> queued;

			auto clone_from(
				const slot& other
			) -> void;
		};

		double_buffer<slot> m_slots;
	};
}

template <typename T, typename IdType>
auto gse::double_buffered_id_mapped_queue<T, IdType>::reader::objects() -> std::span<const T> {
	return parent->m_slots.read().active.items();
}

template <typename T, typename IdType>
auto gse::double_buffered_id_mapped_queue<T, IdType>::reader::try_get(const id_type& id) const -> const T* {
	return parent->m_slots.read().active.try_get(id);
}

template <typename T, typename IdType>
auto gse::double_buffered_id_mapped_queue<T, IdType>::reader::emplace_from_writer(const id_type& id) -> const T* {
	auto& read_slot = const_cast<slot&>(parent->m_slots.read());
	auto& write_slot = parent->m_slots.write();

	if (auto* existing = read_slot.active.try_get(id)) {
		return existing;
	}

	if (const auto* src = write_slot.active.try_get(id)) {
		return read_slot.active.add(id, T(*src));
	}

	return nullptr;
}

template <typename T, typename IdType>
template <typename... Args>
auto gse::double_buffered_id_mapped_queue<T, IdType>::writer::emplace_queued(const id_type& id, Args&&... args) -> T* {
	return parent->m_slots.write().queued.add(id, T(id, std::forward<Args>(args)...));
}

template <typename T, typename IdType>
auto gse::double_buffered_id_mapped_queue<T, IdType>::writer::activate(const id_type& owner) -> bool {
	if (auto obj = parent->m_slots.write().queued.pop(owner)) {
		parent->m_slots.write().active.add(owner, std::move(*obj));
		return true;
	}
	return false;
}

template <typename T, typename IdType>
auto gse::double_buffered_id_mapped_queue<T, IdType>::writer::remove(const id_type& id) -> void {
	for (auto& slot : parent->m_slots.buffer()) {
		slot.active.remove(id);
		slot.queued.remove(id);
	}
}

template <typename T, typename IdType>
auto gse::double_buffered_id_mapped_queue<T, IdType>::writer::objects() -> std::span<T> {
	return parent->m_slots.write().active.items();
}

template <typename T, typename IdType>
auto gse::double_buffered_id_mapped_queue<T, IdType>::writer::try_get(const id_type& id) -> T* {
	if (auto* p = parent->m_slots.write().active.try_get(id)) {
		return p;
	}
	return parent->m_slots.write().queued.try_get(id);
}

template <typename T, typename IdType>
auto gse::double_buffered_id_mapped_queue<T, IdType>::writer::reader() -> double_buffered_id_mapped_queue::reader {
	return { parent };
}

template <typename T, typename IdType>
auto gse::double_buffered_id_mapped_queue<T, IdType>::bind(const std::size_t read, const std::size_t write) -> std::pair<reader, writer> {
	assert(read < 2 && write < 2 && read != write, std::source_location::current(), "Read and write indices must be 0 or 1 and different");

	return {
		{ this },
		{ this }
	};
}

template <typename T, typename IdType>
auto gse::double_buffered_id_mapped_queue<T, IdType>::flip() -> void {
	m_slots.flip();
	m_slots.write().clone_from(m_slots.read());
}

template <typename T, typename IdType>
auto gse::double_buffered_id_mapped_queue<T, IdType>::clear() noexcept -> void {
	for (auto& slot : m_slots.buffer()) {
		slot.active.clear();
		slot.queued.clear();
	}
}

template <typename T, typename IdType>
auto gse::double_buffered_id_mapped_queue<T, IdType>::slot::clone_from(const slot& other) -> void {
	active = other.active;
	queued.clear();
}
