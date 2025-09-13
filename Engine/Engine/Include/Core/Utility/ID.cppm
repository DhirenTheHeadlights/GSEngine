export module gse.utility:id;

import std;

import gse.assert;
import gse.physics.math;

export namespace gse {
	class id;
	using uuid = std::uint64_t;

	auto generate_id(std::string_view tag) -> id;
	auto generate_id(std::uint64_t number) -> id;
	auto generate_temp_id(uuid number, std::string_view tag) -> id;
	auto find(uuid number) -> id;
	auto find(std::string_view tag) -> id;
	auto exists(uuid number) -> bool;
	auto exists(std::string_view tag) -> bool;
}

export namespace gse {
	class id {
	public:
		id() = default;
		auto operator==(const id& other) const -> bool;

		auto number() const -> uuid;
		auto tag() const -> const std::string&;
		auto exists() const -> bool;
	private:
		explicit id(uuid id, std::string tag);

		uuid m_number = std::numeric_limits<uuid>::max();
		std::string m_tag;

		friend auto generate_id(std::string_view tag) -> id;
		friend auto generate_id(std::uint64_t number) -> id;
		friend auto generate_temp_id(uuid number, std::string_view tag) -> id;
	};
}

template <>
struct std::formatter<gse::id> {
	static constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
	static auto format(const gse::id& value, std::format_context& ctx) {
		return std::format_to(ctx.out(), "[{}: {}]", value.number(), value.tag());
	}
};

auto gse::id::operator==(const id& other) const -> bool {
	if (!exists() || !other.exists()) {
		return false;
	}
	return m_number == other.m_number;
}

auto gse::id::number() const -> uuid {
	return m_number;
}

auto gse::id::tag() const -> const std::string& {
	return m_tag;
}

auto gse::id::exists() const -> bool {
	return m_number != std::numeric_limits<uuid>::max();
}

gse::id::id(const uuid id, std::string tag): m_number(id), m_tag(std::move(tag)) {}

export namespace gse {
	class identifiable {
	public:
		explicit identifiable(const std::string& tag);
		explicit identifiable(const std::filesystem::path& path);

		auto id() const -> const id&;
		auto operator==(const identifiable& other) const -> bool = default;
	private:
		gse::id m_id;

		static auto filename(const std::filesystem::path& path) -> std::string ;
	};
}

gse::identifiable::identifiable(const std::string& tag) : m_id(generate_id(tag)) {}

gse::identifiable::identifiable(const std::filesystem::path& path): m_id(generate_id(filename(path))) {}

auto gse::identifiable::id() const -> const gse::id& {
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
		identifiable_owned() = default;
		explicit identifiable_owned(const id& owner_id);

		auto owner_id() const -> const id&;
		auto operator==(const identifiable_owned& other) const -> bool = default;

		auto swap(const id& new_parent_id) -> void;
		auto swap(const identifiable& new_parent) -> void;
	private:
		id m_owner_id;
	};
}

gse::identifiable_owned::identifiable_owned(const id& owner_id) : m_owner_id(owner_id) {}

auto gse::identifiable_owned::owner_id() const -> const id& {
	return m_owner_id;
}

auto gse::identifiable_owned::swap(const id& new_parent_id) -> void {
	m_owner_id = new_parent_id;
}

auto gse::identifiable_owned::swap(const identifiable& new_parent) -> void {
	swap(new_parent.id());
}

export namespace gse {
	class identifiable_owned_only_uuid {
	public:
		identifiable_owned_only_uuid() = default;
		explicit identifiable_owned_only_uuid(const id& owner_id);

		auto owner_id() const -> id;
		auto operator==(const identifiable_owned_only_uuid& other) const -> bool = default;

		auto swap(const id& new_parent_id) -> void;
		auto swap(const identifiable& new_parent) -> void;
	private:
		uuid m_owner_id;
	};
}

gse::identifiable_owned_only_uuid::identifiable_owned_only_uuid(const id& owner_id) : m_owner_id(owner_id.number()) {}

auto gse::identifiable_owned_only_uuid::owner_id() const -> id {
	return find(m_owner_id);
}

auto gse::identifiable_owned_only_uuid::swap(const id& new_parent_id) -> void {
	assert(
		new_parent_id.exists(),
		std::format("Cannot reassign identifiable owned to invalid id {}: {}", new_parent_id.tag(), new_parent_id.number())
	);
	m_owner_id = new_parent_id.number();
}

auto gse::identifiable_owned_only_uuid::swap(const identifiable& new_parent) -> void {
	swap(new_parent.id());
}

export namespace gse {
	struct entity;

	template <typename T>
	concept is_identifiable = std::derived_from<T, identifiable> || std::same_as<T, entity>;

	template <typename T, typename PrimaryIdType = id>
	class id_mapped_collection {
	public:
		auto add(const PrimaryIdType& id, T object) -> T*;

		auto remove(const PrimaryIdType& id) -> void;

		auto pop(const PrimaryIdType& id) -> std::optional<T>;

		auto try_get(const PrimaryIdType& id) -> T*;

		auto try_get(const PrimaryIdType& id) const -> const T*;

		auto items() -> std::span<T>;

		auto contains(const PrimaryIdType& id) const -> bool;

		auto size() const -> size_t;

		auto clear() noexcept -> void;
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
auto gse::id_mapped_collection<T, PrimaryIdType>::items() -> std::span<T> {
	return m_items;
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

	const uuid new_uuid = registry.by_uuid.size();

	std::string new_tag(tag);
	if (registry.tag_to_uuid.contains(new_tag)) {
		new_tag += std::to_string(new_uuid);
	}

	id new_id(new_uuid, std::move(new_tag));

	registry.by_uuid.add(new_uuid, new_id);
	registry.tag_to_uuid[new_id.tag()] = new_uuid;

	return new_id;
}

auto gse::generate_id(const std::uint64_t number) -> id {
	const auto& [mutex, registry] = id_registry();
	std::lock_guard lock(mutex);

	assert(!registry.by_uuid.contains(number), std::format("ID number {} already exists", number));

	id new_id(number, std::to_string(number));

	registry.by_uuid.add(number, new_id);
	registry.tag_to_uuid[new_id.tag()] = number;

	return new_id;
}

auto gse::generate_temp_id(const uuid number, const std::string_view tag) -> id {
	return id(number, std::string(tag));
}

auto gse::find(const uuid number) -> id {
	const auto& [mutex, registry] = id_registry();
	std::shared_lock lock(mutex);

	id* found_id = registry.by_uuid.try_get(number);
	assert(found_id, std::format("ID {} not found", number));
	return *found_id;
}

auto gse::find(const std::string_view tag) -> id {
	const auto& [mutex, registry] = id_registry();
	std::shared_lock lock(mutex);

	const auto it = registry.tag_to_uuid.find(tag);
	assert(it != registry.tag_to_uuid.end(), std::format("Tag '{}' not found", tag));

	id* found_id = registry.by_uuid.try_get(it->second);
	assert(found_id, "Inconsistent registry state!");
	return *found_id;
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