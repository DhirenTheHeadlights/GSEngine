export module gse.core.id;

import std;
import gse.platform.assert;
import gse.physics.math;

export namespace gse {
    class id;
	using uuid = std::int32_t;

    auto generate_id(const std::string& tag) -> std::unique_ptr<id>;
	auto remove_id(const id* id) -> void;
    auto get_id(uuid number) -> id*;
    auto get_id(std::string_view tag) -> id*;
	auto does_id_exist(uuid number) -> bool;
	auto does_id_exist(std::string_view tag) -> bool;

    class id {
    public:
        ~id();

		auto operator==(const id& other) const -> bool;

		auto number() const -> uuid;
		auto tag() const -> std::string_view;
    private:
        explicit id(int id, const std::string& tag);
        id(const std::string& tag) : m_number(-1), m_tag(tag) {}

        uuid m_number;
		std::string m_tag;

		friend auto generate_id(const std::string& tag) -> std::unique_ptr<id>;
		friend auto remove_id(const id* id) -> void;
    };

    class identifiable {
	public:
		identifiable(const std::string& tag);

		auto get_id() const -> id*;
		auto operator==(const identifiable& other) const -> bool;
	private:
		std::unique_ptr<id> m_id;
    };
}

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

std::vector<gse::id*> g_ids;
std::unordered_map<std::int32_t, gse::id*> g_id_map;
std::unordered_map<std::string, gse::id*, transparent_hash, transparent_equal> g_tag_map;

auto register_object(gse::id* obj, const std::string& tag) -> void {
	g_id_map[obj->number()] = obj;
	g_tag_map[tag] = obj;
}

auto gse::generate_id(const std::string& tag) -> std::unique_ptr<id> {
	const int new_id = static_cast<int>(g_ids.size());

	std::string new_tag = tag;
	if (const auto it = g_tag_map.find(tag); it != g_tag_map.end()) {
		new_tag += std::to_string(new_id);
	}

	auto id_ptr = std::unique_ptr<id>(new id(new_id, new_tag));
	g_ids.push_back(id_ptr.get());
	register_object(id_ptr.get(), new_tag);

	return id_ptr;
}

auto gse::remove_id(const id* id) -> void {
	const int index = id->number();
	if (index < 0 || index >= static_cast<int>(g_ids.size())) {
		assert_comment(false, "Object not found in registry when trying to remove its id");
		return;
	}

	g_id_map.erase(index);
	g_tag_map.erase(id->tag());

	if (index != static_cast<int>(g_ids.size()) - 1) {
		std::swap(g_ids[index], g_ids.back());
		g_ids[index]->m_number = index;
		g_id_map[index] = g_ids[index];
	}

	g_ids.pop_back();
}

auto gse::get_id(const std::int32_t number) -> id* {
	if (const auto it = g_id_map.find(number); it != g_id_map.end()) {
		return it->second;
	}
	return nullptr;
}

auto gse::get_id(const std::string_view tag) -> id* {
	if (const auto it = g_tag_map.find(tag); it != g_tag_map.end()) {
		return it->second;
	}
	return nullptr;
}

auto gse::does_id_exist(const uuid number) -> bool {
	return g_id_map.contains(number);
}

auto gse::does_id_exist(const std::string_view tag) -> bool {
	return g_tag_map.contains(tag);
}

/// ID

gse::id::id(const uuid id, const std::string& tag) : m_number(id), m_tag(tag) {}

gse::id::~id() {
	if (m_number != -1) {
		remove_id(this);
	}
}

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

auto gse::id::tag() const -> std::string_view {
	return m_tag;
}

/// Identifiable

gse::identifiable::identifiable(const std::string& tag) : m_id(generate_id(tag)) {}

auto gse::identifiable::get_id() const -> id* {
	return m_id.get();
}

auto gse::identifiable::operator==(const identifiable& other) const -> bool {
	return *m_id == *other.m_id;
}