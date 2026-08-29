export module gse.json:value;

import std;

export namespace gse::json {
	class value {
	public:
		enum struct kind : std::uint8_t {
			null,
			boolean,
			number,
			string,
			array,
			object
		};

		struct number_storage {
			double real = 0.0;
			std::int64_t integer = 0;
			bool integral = false;
		};

		struct array_storage {
			std::vector<value> items;

			array_storage();

			~array_storage();

			array_storage(
				const array_storage& other
			);

			array_storage(
				array_storage&& other
			) noexcept;

			auto operator=(
				const array_storage& other
			) -> array_storage&;

			auto operator=(
				array_storage&& other
			) noexcept -> array_storage&;
		};

		struct object_storage {
			std::vector<std::string> keys;
			std::vector<value> values;

			object_storage();

			~object_storage();

			object_storage(
				const object_storage& other
			);

			object_storage(
				object_storage&& other
			) noexcept;

			auto operator=(
				const object_storage& other
			) -> object_storage&;

			auto operator=(
				object_storage&& other
			) noexcept -> object_storage&;
		};

		using storage_type = std::variant<std::monostate, bool, number_storage, std::string, array_storage, object_storage>;

		value() = default;

		explicit value(
			bool boolean
		);

		explicit value(
			double real
		);

		explicit value(
			std::int64_t integer
		);

		explicit value(
			std::string text
		);

		static auto make_array() -> value;

		static auto make_object() -> value;

		[[nodiscard]] auto type() const -> kind;

		[[nodiscard]] auto storage() const -> const storage_type&;

		[[nodiscard]] auto is_null() const -> bool;

		[[nodiscard]] auto is_boolean() const -> bool;

		[[nodiscard]] auto is_number() const -> bool;

		[[nodiscard]] auto is_integer() const -> bool;

		[[nodiscard]] auto is_string() const -> bool;

		[[nodiscard]] auto is_array() const -> bool;

		[[nodiscard]] auto is_object() const -> bool;

		[[nodiscard]] auto boolean(
			bool fallback = false
		) const -> bool;

		[[nodiscard]] auto number(
			double fallback = 0.0
		) const -> double;

		[[nodiscard]] auto integer(
			std::int64_t fallback = 0
		) const -> std::int64_t;

		[[nodiscard]] auto text(
			std::string_view fallback = {}
		) const -> std::string_view;

		[[nodiscard]] auto size() const -> std::size_t;

		[[nodiscard]] auto empty() const -> bool;

		[[nodiscard]] auto elements() const -> std::span<const value>;

		[[nodiscard]] auto keys() const -> std::span<const std::string>;

		[[nodiscard]] auto at(
			std::size_t index
		) const -> const value*;

		[[nodiscard]] auto find(
			std::string_view key
		) const -> const value*;

		[[nodiscard]] auto contains(
			std::string_view key
		) const -> bool;

		auto push_back(
			value element
		) -> value&;

		auto insert(
			std::string key,
			value member
		) -> value&;

		auto reserve(
			std::size_t count
		) -> void;

	private:
		storage_type m_storage;
	};
}

namespace gse::json {
	template <value::kind K>
	using alternative_at = std::variant_alternative_t<static_cast<std::size_t>(K), value::storage_type>;

	static_assert(std::is_same_v<alternative_at<value::kind::null>, std::monostate>);
	static_assert(std::is_same_v<alternative_at<value::kind::boolean>, bool>);
	static_assert(std::is_same_v<alternative_at<value::kind::number>, value::number_storage>);
	static_assert(std::is_same_v<alternative_at<value::kind::string>, std::string>);
	static_assert(std::is_same_v<alternative_at<value::kind::array>, value::array_storage>);
	static_assert(std::is_same_v<alternative_at<value::kind::object>, value::object_storage>);
}

gse::json::value::array_storage::array_storage() = default;

gse::json::value::array_storage::~array_storage() = default;

gse::json::value::array_storage::array_storage(const array_storage& other) = default;

gse::json::value::array_storage::array_storage(array_storage&& other) noexcept = default;

auto gse::json::value::array_storage::operator=(const array_storage& other) -> array_storage& = default;

auto gse::json::value::array_storage::operator=(array_storage&& other) noexcept -> array_storage& = default;

gse::json::value::object_storage::object_storage() = default;

gse::json::value::object_storage::~object_storage() = default;

gse::json::value::object_storage::object_storage(const object_storage& other) = default;

gse::json::value::object_storage::object_storage(object_storage&& other) noexcept = default;

auto gse::json::value::object_storage::operator=(const object_storage& other) -> object_storage& = default;

auto gse::json::value::object_storage::operator=(object_storage&& other) noexcept -> object_storage& = default;

gse::json::value::value(const bool boolean) : m_storage(boolean) {}

gse::json::value::value(const double real) : m_storage(number_storage{
	.real = real,
}) {}

gse::json::value::value(const std::int64_t integer) : m_storage(number_storage{
	.real = static_cast<double>(integer),
	.integer = integer,
	.integral = true,
}) {}

gse::json::value::value(std::string text) : m_storage(std::move(text)) {}

auto gse::json::value::make_array() -> value {
	value out;
	out.m_storage.emplace<array_storage>();
	return out;
}

auto gse::json::value::make_object() -> value {
	value out;
	out.m_storage.emplace<object_storage>();
	return out;
}

auto gse::json::value::type() const -> kind {
	return static_cast<kind>(m_storage.index());
}

auto gse::json::value::storage() const -> const storage_type& {
	return m_storage;
}

auto gse::json::value::is_null() const -> bool {
	return std::holds_alternative<std::monostate>(m_storage);
}

auto gse::json::value::is_boolean() const -> bool {
	return std::holds_alternative<bool>(m_storage);
}

auto gse::json::value::is_number() const -> bool {
	return std::holds_alternative<number_storage>(m_storage);
}

auto gse::json::value::is_integer() const -> bool {
	const auto* stored = std::get_if<number_storage>(&m_storage);
	return stored != nullptr && stored->integral;
}

auto gse::json::value::is_string() const -> bool {
	return std::holds_alternative<std::string>(m_storage);
}

auto gse::json::value::is_array() const -> bool {
	return std::holds_alternative<array_storage>(m_storage);
}

auto gse::json::value::is_object() const -> bool {
	return std::holds_alternative<object_storage>(m_storage);
}

auto gse::json::value::boolean(const bool fallback) const -> bool {
	const auto* stored = std::get_if<bool>(&m_storage);
	return stored ? *stored : fallback;
}

auto gse::json::value::number(const double fallback) const -> double {
	const auto* stored = std::get_if<number_storage>(&m_storage);
	return stored ? stored->real : fallback;
}

auto gse::json::value::integer(const std::int64_t fallback) const -> std::int64_t {
	const auto* stored = std::get_if<number_storage>(&m_storage);
	if (!stored) {
		return fallback;
	}
	return stored->integral ? stored->integer : static_cast<std::int64_t>(stored->real);
}

auto gse::json::value::text(const std::string_view fallback) const -> std::string_view {
	const auto* stored = std::get_if<std::string>(&m_storage);
	return stored ? std::string_view(*stored) : fallback;
}

auto gse::json::value::size() const -> std::size_t {
	if (const auto* items = std::get_if<array_storage>(&m_storage)) {
		return items->items.size();
	}
	if (const auto* members = std::get_if<object_storage>(&m_storage)) {
		return members->values.size();
	}
	return 0;
}

auto gse::json::value::empty() const -> bool {
	return size() == 0;
}

auto gse::json::value::elements() const -> std::span<const value> {
	if (const auto* items = std::get_if<array_storage>(&m_storage)) {
		return items->items;
	}
	if (const auto* members = std::get_if<object_storage>(&m_storage)) {
		return members->values;
	}
	return {};
}

auto gse::json::value::keys() const -> std::span<const std::string> {
	if (const auto* members = std::get_if<object_storage>(&m_storage)) {
		return members->keys;
	}
	return {};
}

auto gse::json::value::at(const std::size_t index) const -> const value* {
	const std::span<const value> items = elements();
	return index < items.size() ? &items[index] : nullptr;
}

auto gse::json::value::find(const std::string_view key) const -> const value* {
	const auto* members = std::get_if<object_storage>(&m_storage);
	if (!members) {
		return nullptr;
	}
	for (std::size_t i = 0; i < members->keys.size(); ++i) {
		if (members->keys[i] == key) {
			return &members->values[i];
		}
	}
	return nullptr;
}

auto gse::json::value::contains(const std::string_view key) const -> bool {
	return find(key) != nullptr;
}

auto gse::json::value::push_back(value element) -> value& {
	auto* items = std::get_if<array_storage>(&m_storage);
	if (!items) {
		items = &m_storage.emplace<array_storage>();
	}
	return items->items.emplace_back(std::move(element));
}

auto gse::json::value::insert(std::string key, value member) -> value& {
	auto* members = std::get_if<object_storage>(&m_storage);
	if (!members) {
		members = &m_storage.emplace<object_storage>();
	}

	for (std::size_t i = 0; i < members->keys.size(); ++i) {
		if (members->keys[i] == key) {
			members->values[i] = std::move(member);
			return members->values[i];
		}
	}

	members->keys.push_back(std::move(key));
	return members->values.emplace_back(std::move(member));
}

auto gse::json::value::reserve(const std::size_t count) -> void {
	if (auto* items = std::get_if<array_storage>(&m_storage)) {
		items->items.reserve(count);
		return;
	}
	if (auto* members = std::get_if<object_storage>(&m_storage)) {
		members->keys.reserve(count);
		members->values.reserve(count);
	}
}
