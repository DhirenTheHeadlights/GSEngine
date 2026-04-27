export module gse.core:id;

import std;
import gse.std_meta;

import gse.assert;
import gse.math;

namespace gse {
	using uuid = std::uint64_t;
}

export namespace gse {
	class id;

	template <std::size_t N>
	struct fixed_string {
		char data[N]{};

		consteval fixed_string(
			const char (&s)[N]
		);

		consteval operator std::string_view(
		) const;
	};

	template <std::size_t N>
	fixed_string(const char (&)[N]) -> fixed_string<N>;

	consteval auto stable_id(
		std::string_view tag
	) -> uuid;

	template <typename T>
	consteval auto id_of(
	) -> id;

	template <fixed_string Tag>
	consteval auto id_of(
	) -> id;

	template <typename T>
	auto trace_id(
	) -> id;

	template <fixed_string Tag>
	auto trace_id(
	) -> id;

	template <typename T>
	consteval auto type_tag(
	) -> std::string_view;

	auto generate_id(
		std::string_view tag
	) -> id;

	auto generate_id(
		std::uint64_t number
	) -> id;

	constexpr auto generate_temp_id(
		uuid number
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

		auto operator<=>(
			id other
		) const -> std::strong_ordering;

		constexpr auto number(
		) const -> uuid;

		auto tag(
		) const -> std::string_view;

		constexpr auto exists(
		) const -> bool;

		auto reset(
		) -> void;
	private:
		explicit constexpr id(
			uuid id
		);

		uuid m_number = std::numeric_limits<uuid>::max();

		friend auto generate_id(std::string_view tag) -> id;
		friend auto generate_id(std::uint64_t number) -> id;
		friend constexpr auto generate_temp_id(uuid number) -> id;
	};
}

template <>
struct std::formatter<gse::id> {
	static constexpr auto parse(std::format_parse_context& ctx) {
		return ctx.begin();
	}

	static auto format(const gse::id value, std::format_context& ctx) {
		if (!value.exists()) {
			return std::format_to(ctx.out(), "[invalid]");
		}
		return std::format_to(ctx.out(), "[{}: {}]", value.number(), value.tag());
	}
};

auto gse::id::operator==(const id other) const -> bool {
	if (!exists() || !other.exists()) {
		return false;
	}

	return m_number == other.m_number;
}

auto gse::id::operator<=>(const id other) const -> std::strong_ordering {
	return m_number <=> other.m_number;
}

constexpr auto gse::id::number() const -> uuid {
	return m_number;
}

auto gse::id::tag() const -> std::string_view {
	return gse::tag(m_number);
}

constexpr auto gse::id::exists() const -> bool {
	return m_number != std::numeric_limits<uuid>::max();
}

auto gse::id::reset() -> void {
	this->m_number = std::numeric_limits<uuid>::max();
}

constexpr gse::id::id(const uuid id) : m_number(id) {}

export namespace gse {
	class identifiable {
	public:
		explicit identifiable(
			const std::string& tag
		);

		explicit identifiable(
			const std::filesystem::path& path
		);

		explicit identifiable(
			const std::filesystem::path& path,
			const std::filesystem::path& base
		);

		auto id(
		) const -> id;

		auto operator==(
			const identifiable& other
		) const -> bool = default;
	private:
		gse::id m_id;

		static auto relative_stem(
			const std::filesystem::path& path,
			const std::filesystem::path& base
		) -> std::string;
	};
}

gse::identifiable::identifiable(const std::string& tag) : m_id(generate_id(tag)) {}

gse::identifiable::identifiable(const std::filesystem::path& path) : m_id(generate_id(relative_stem(path, {}))) {}

gse::identifiable::identifiable(const std::filesystem::path& path, const std::filesystem::path& base) : m_id(generate_id(relative_stem(path, base))) {}

auto gse::identifiable::id() const -> gse::id {
	return m_id;
}

auto gse::identifiable::relative_stem(const std::filesystem::path& path, const std::filesystem::path& base) -> std::string {
	std::filesystem::path relative = path;
	if (!base.empty() && path.generic_string().starts_with(base.generic_string())) {
		relative = path.lexically_relative(base);
	}

	std::string result;
	for (auto it = relative.begin(); it != relative.end(); ++it) {
		if (!result.empty()) {
			result += '/';
		}
		result += it->string();
	}

	if (const std::size_t dot_pos = result.find_last_of('.'); dot_pos != std::string::npos) {
		result = result.substr(0, dot_pos);
	}

	return result;
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
	template <typename T>
	concept is_identifiable = std::derived_from<T, identifiable>;

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
		) const -> std::size_t;

		auto clear(
		) noexcept -> void;

		auto transfer_from(
			id_mapped_collection& other
		) -> void;
	private:
		std::vector<T> m_items;
		std::vector<PrimaryIdType> m_ids;
		std::unordered_map<PrimaryIdType, std::size_t> m_map;
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

	const std::size_t index_to_remove = it->second;

	if (const std::size_t last_index = m_items.size() - 1; index_to_remove != last_index) {
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

	const std::size_t index_to_pop = it->second;
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
auto gse::id_mapped_collection<T, PrimaryIdType>::size() const -> std::size_t {
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

template <>
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

constexpr auto gse::generate_temp_id(const uuid number) -> id {
	return id(number);
}

auto gse::find(const uuid number) -> id {
	const auto found_id = try_find(number);
	assert(found_id.has_value(), std::source_location::current(), "ID {} not found", number);
	return *found_id;
}

auto gse::find(const std::string_view tag) -> id {
	const auto found_id = try_find(tag);
	assert(found_id.has_value(), std::source_location::current(), "ID {} not found", tag);
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

template <std::size_t N>
consteval gse::fixed_string<N>::fixed_string(const char (&s)[N]) {
	std::ranges::copy(s, data);
}

template <std::size_t N>
consteval gse::fixed_string<N>::operator std::string_view() const {
	return { data, N - 1 };
}

consteval auto gse::stable_id(const std::string_view tag) -> uuid {
	uuid h = 0xcbf29ce484222325ull;
	for (const unsigned char c : tag) {
		h ^= c;
		h *= 1099511628211ull;
	}
	return h;
}

template <typename T>
consteval auto gse::id_of() -> id {
	return generate_temp_id(stable_id(std::meta::display_string_of(^^T)));
}

template <gse::fixed_string Tag>
consteval auto gse::id_of() -> id {
	return generate_temp_id(stable_id(Tag));
}

template <typename T>
auto gse::trace_id() -> id {
	static const id cached = find_or_generate_id(std::meta::display_string_of(^^T));
	return cached;
}

template <gse::fixed_string Tag>
auto gse::trace_id() -> id {
	static const id cached = find_or_generate_id(std::string_view(Tag));
	return cached;
}

template <typename T>
consteval auto gse::type_tag() -> std::string_view {
	return std::meta::display_string_of(^^T);
}

