export module gse.core:id;

import std;

import gse.meta;

namespace gse {
	using uuid = std::uint64_t;
}

export namespace gse {
	class id;

	constexpr auto stable_id(
		std::string_view tag
	) -> uuid;

	template <typename T>
	consteval auto id_of() -> id;

	template <fixed_string Tag>
	consteval auto id_of() -> id;

	template <typename T>
	auto trace_id() -> id;

	template <fixed_string Tag>
	auto trace_id() -> id;

	template <typename T>
	consteval auto type_tag() -> std::string_view;

	auto generate_id(
		std::string_view tag
	) -> id;

	auto generate_id(
		std::uint64_t number
	) -> id;

	constexpr auto generate_temp_id(
		uuid number
	) -> id;

	auto generate_temp_id(
		const std::filesystem::path& path
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

		auto operator<=>(
			const id&
		) const -> std::strong_ordering = default;

		[[nodiscard]] constexpr auto number() const -> uuid;

		[[nodiscard]] auto tag() const -> std::string_view;

		[[nodiscard]] constexpr auto exists() const -> bool;

		auto reset() -> void;

	private:
		explicit constexpr id(
			uuid id
		);

		uuid m_number = std::numeric_limits<uuid>::max();

		friend auto generate_id(
			std::string_view tag
		) -> id;
		friend auto generate_id(
			std::uint64_t number
		) -> id;
		friend constexpr auto generate_temp_id(
			uuid number
		) -> id;
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
		if (gse::exists(value.number())) {
			return std::format_to(ctx.out(), "[{}: {}]", value.number(), value.tag());
		}
		return std::format_to(ctx.out(), "[{}]", value.number());
	}
};

constexpr auto gse::id::number() const -> uuid {
	return m_number;
}

constexpr auto gse::id::exists() const -> bool {
	return m_number != std::numeric_limits<uuid>::max();
}

constexpr gse::id::id(const uuid id) : m_number(id) {
}

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

		[[nodiscard]] auto id() const -> id;

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

export namespace gse {
	class identifiable_owned {
	public:
		identifiable_owned() = default;

		explicit identifiable_owned(
			id owner_id
		);

		auto owner_id() const -> id;

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

export namespace gse {
	template <typename T>
	concept is_identifiable = std::derived_from<T, identifiable>;

	template <typename T, typename PrimaryIdType = id>
	class id_mapped_collection {
	public:
		id_mapped_collection() = default;
		id_mapped_collection(const id_mapped_collection&) = default;
		auto operator=(const id_mapped_collection&) -> id_mapped_collection& = default;
		id_mapped_collection(id_mapped_collection&& other) noexcept;
		auto operator=(id_mapped_collection&& other) noexcept -> id_mapped_collection&;

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

		[[nodiscard]] auto try_get(
			const PrimaryIdType& id
		) const -> const T*;

		auto items(
			this auto&& self
		) -> decltype(auto);

		[[nodiscard]] auto ids() const -> std::span<const PrimaryIdType>;

		auto contains(
			const PrimaryIdType& id
		) const -> bool;

		[[nodiscard]] auto size() const -> std::size_t;

		auto reserve(
			std::size_t capacity
		) -> void;

		auto clear() noexcept -> void;

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
gse::id_mapped_collection<T, PrimaryIdType>::id_mapped_collection(id_mapped_collection&& other) noexcept {
	transfer_from(other);
}

template <typename T, typename PrimaryIdType>
auto gse::id_mapped_collection<T, PrimaryIdType>::operator=(id_mapped_collection&& other) noexcept -> id_mapped_collection& {
	if (this != &other) {
		transfer_from(other);
	}
	return *this;
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
		PrimaryIdType last_id = std::move(m_ids.back());
		m_items[index_to_remove] = std::move(m_items.back());
		m_ids[index_to_remove] = std::move(last_id);
		m_map[m_ids[index_to_remove]] = index_to_remove;
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
auto gse::id_mapped_collection<T, PrimaryIdType>::ids() const -> std::span<const PrimaryIdType> {
	return m_ids;
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
auto gse::id_mapped_collection<T, PrimaryIdType>::reserve(const std::size_t capacity) -> void {
	m_items.reserve(capacity);
	m_ids.reserve(capacity);
	m_map.reserve(capacity);
}

template <typename T, typename PrimaryIdType>
auto gse::id_mapped_collection<T, PrimaryIdType>::clear() noexcept -> void {
	m_items.clear();
	m_ids.clear();
	m_map.clear();
}

template <typename T, typename PrimaryIdType>
auto gse::id_mapped_collection<T, PrimaryIdType>::transfer_from(id_mapped_collection& other) -> void {
	if (this == &other) {
		return;
	}
	clear();
	m_items.swap(other.m_items);
	m_ids.swap(other.m_ids);
	m_map.swap(other.m_map);
}

export namespace gse {
	struct transparent_hash {
		using is_transparent = void;
		auto operator()(const std::string_view sv) const noexcept {
			return std::hash<std::string_view>{}(sv);
		}
	};
	struct transparent_equal {
		using is_transparent = void;
		auto operator()(const std::string_view a, const std::string_view b) const noexcept {
			return a == b;
		}
	};
}

constexpr auto gse::generate_temp_id(const uuid number) -> id {
	return id(number);
}

constexpr auto gse::stable_id(const std::string_view tag) -> uuid {
	uuid h = 0xcbf29ce484222325ull;
	for (const unsigned char c : tag) {
		h ^= c;
		h *= 1099511628211ull;
	}
	return h;
}

template <typename T>
consteval auto gse::id_of() -> id {
	return generate_temp_id(stable_id(type_tag<T>()));
}

template <gse::fixed_string Tag>
consteval auto gse::id_of() -> id {
	return generate_temp_id(stable_id(Tag));
}

export namespace gse {
	template <typename T>
	const id trace_id_type_cache = find_or_generate_id(type_tag<T>());

	template <fixed_string Tag>
	const id trace_id_tag_cache = find_or_generate_id(std::string_view(Tag));
}

template <typename T>
auto gse::trace_id() -> id {
	return trace_id_type_cache<T>;
}

template <gse::fixed_string Tag>
auto gse::trace_id() -> id {
	return trace_id_tag_cache<Tag>;
}

template <typename T>
consteval auto gse::type_tag() -> std::string_view {
	return gse::meta::qualified_name<T>();
}