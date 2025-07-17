export module gse.utility:id;

import std;

import gse.assert;
import gse.physics.math;

export namespace gse {
    class id;
	using uuid = std::uint64_t;

    auto generate_id(std::string_view tag) -> id;
	auto find(uuid number) -> id;
	auto find(std::string_view tag) -> id;
	auto exists(uuid number) -> bool;
	auto exists(std::string_view tag) -> bool;

    class id {
    public:
		id() = default;
		auto operator==(const id& other) const -> bool;

		auto number() const -> uuid;
		auto tag() const -> std::string;
		auto exists() const -> bool { return m_number < std::numeric_limits<uuid>::max(); }
    private:
        explicit id(uuid id, const std::string& tag);

		uuid m_number = std::numeric_limits<uuid>::max();
		std::string m_tag;

		friend auto generate_id(std::string_view tag) -> id;
    };

    class identifiable {
	public:
	    explicit identifiable(const std::string& tag) : m_id(generate_id(tag)) {}

		auto id() const -> id { return m_id; }
		auto operator==(const identifiable& other) const -> bool = default;
	private:
		gse::id m_id;
    };

	class identifiable_owned {
	public:
		explicit identifiable_owned(const id& tag) : m_owner_id(tag) {}

		auto owner_id() const -> id { return m_owner_id; }
		auto operator==(const identifiable_owned& other) const -> bool = default;
	private:
		id m_owner_id;
	};

	template <typename T>
	concept is_identifiable = std::derived_from<T, identifiable>;
}

template <>
struct std::formatter<gse::id> {
	constexpr auto parse(std::format_parse_context& ctx) -> std::format_parse_context::iterator {
		return ctx.begin();
	}

	auto format(const gse::id& value, std::format_context& ctx) const -> std::format_context::iterator {
		return std::format_to(ctx.out(), "[{}: {}]", value.number(), value.tag());
	}
};

export template <>
struct std::hash<gse::id> {
	auto operator()(const gse::id& id) const noexcept -> std::size_t {
		return std::hash<gse::uuid>{}(id.number());
	}
};

struct transparent_hash {
	using is_transparent = void; // Indicates support for heterogeneous lookup

	auto operator()(const std::string& s) const noexcept -> std::size_t {
		return std::hash<std::string_view>{}(s);
	}

	auto operator()(const std::string_view sv) const noexcept -> std::size_t {
		return std::hash<std::string_view>{}(sv);
	}
};

struct transparent_equal {
	using is_transparent = void; // Indicates support for heterogeneous lookup

	auto operator()(const std::string& lhs, const std::string& rhs) const noexcept -> bool {
		return lhs == rhs;
	}

	auto operator()(const std::string_view lhs, const std::string_view rhs) const noexcept -> bool {
		return lhs == rhs;
	}

	auto operator()(const std::string& lhs, const std::string_view rhs) const noexcept -> bool {
		return lhs == rhs;
	}

	auto operator()(const std::string_view lhs, const std::string& rhs) const noexcept -> bool {
		return lhs == rhs;
	}
};

namespace gse {
	std::vector<id> ids;
	std::unordered_map<uuid, id> id_map;
	std::unordered_map<std::string, id, transparent_hash, transparent_equal> tag_map;

	auto register_object(const id& obj, const std::string& tag) -> void;
}

auto gse::register_object(const id& obj, const std::string& tag) -> void {
	id_map.insert_or_assign(obj.number(), obj);
	tag_map.insert_or_assign(tag, obj);
}

auto gse::generate_id(const std::string_view tag) -> id {
	const uuid new_id = ids.size();

	std::string new_tag(tag);
	if (tag_map.contains(new_tag)) {
		new_tag += std::to_string(new_id);
	}

	const id id(new_id, new_tag);
	ids.push_back(id);
	register_object(id, new_tag);

	return id;
}

auto gse::find(const uuid number) -> id {
	const auto it = id_map.find(number);
	assert(it != id_map.end(), std::format("ID {} not found", number));
	return it->second;
}

auto gse::find(const std::string_view tag) -> id {
	const auto it = tag_map.find(tag);
	assert(it != tag_map.end(), std::format("Tag {} not found", tag));
	return it->second;
}

auto gse::exists(const uuid number) -> bool {
	return id_map.contains(number);
}

auto gse::exists(const std::string_view tag) -> bool {
	return tag_map.contains(tag);
}

/// ID

gse::id::id(const uuid id, const std::string& tag) : m_number(id), m_tag(tag) {}

auto gse::id::operator==(const id& other) const -> bool {
	if (m_number == -1 || other.m_number == -1) {
		std::cerr << "You're comparing something without an id.\n";
		return false;
	}
	return m_number == other.m_number;
}

auto gse::id::number() const -> uuid {
	return m_number;
}

auto gse::id::tag() const -> std::string {
	return m_tag;
}
