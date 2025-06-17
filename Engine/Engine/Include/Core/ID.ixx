export module gse.core.id;

import std;
import gse.platform.assert;
import gse.physics.math;

export namespace gse {
    class id;
	using uuid = std::uint64_t;

    auto generate_id(std::string_view tag) -> id;
	auto get_id(uuid number) -> id;
	auto get_id(std::string_view tag) -> id;
	auto does_id_exist(uuid number) -> bool;
	auto does_id_exist(std::string_view tag) -> bool;

    class id {
    public:
		id() = default;
		auto operator==(const id& other) const -> bool;

		auto number() const -> uuid;
		auto tag() const -> std::string;
    private:
        explicit id(uuid id, const std::string& tag);

		uuid m_number = std::numeric_limits<uuid>::max();
		std::string m_tag;

		friend auto generate_id(std::string_view tag) -> id;
    };

    class identifiable {
	public:
	    explicit identifiable(const std::string& tag);

		auto get_id() const -> id;
		auto operator==(const identifiable& other) const -> bool;
	private:
		id m_id;
    };
}

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

std::vector<gse::id> g_ids;
std::unordered_map<gse::uuid, gse::id> g_id_map;
std::unordered_map<std::string, gse::id, transparent_hash, transparent_equal> g_tag_map;

auto register_object(const gse::id& obj, const std::string& tag) -> void {
	g_id_map.insert_or_assign(obj.number(), obj);
	g_tag_map.insert_or_assign(tag, obj);
}

auto gse::generate_id(const std::string_view tag) -> id {
	const uuid new_id = g_ids.size();

	std::string new_tag(tag);
	if (g_tag_map.contains(new_tag)) {
		new_tag += std::to_string(new_id);
	}

	const id id(new_id, new_tag);
	g_ids.push_back(id);
	register_object(id, new_tag);

	return id;
}

auto gse::get_id(const uuid number) -> id {
	const auto it = g_id_map.find(number);
	assert(it != g_id_map.end(), std::format("ID {} not found", number));
	return it->second;
}

auto gse::get_id(const std::string_view tag) -> id {
	const auto it = g_tag_map.find(tag);
	assert(it != g_tag_map.end(), std::format("Tag {} not found", tag));
	return it->second;
}

auto gse::does_id_exist(const uuid number) -> bool {
	return g_id_map.contains(number);
}

auto gse::does_id_exist(const std::string_view tag) -> bool {
	return g_tag_map.contains(tag);
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

/// Identifiable

gse::identifiable::identifiable(const std::string& tag) : m_id(generate_id(tag)) {}

auto gse::identifiable::get_id() const -> id {
	return m_id;
}

auto gse::identifiable::operator==(const identifiable& other) const -> bool {
	return m_id == other.m_id;
}