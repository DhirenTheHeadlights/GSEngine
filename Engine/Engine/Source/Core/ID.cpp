module gse.core;

import std;

import gse.assert;

namespace gse {
	using uuid = std::uint64_t;

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

auto gse::id::tag() const -> std::string_view {
	return gse::tag(m_number);
}

auto gse::id::reset() -> void {
	this->m_number = std::numeric_limits<uuid>::max();
}

gse::identifiable::identifiable(const std::string& tag) : m_id(generate_id(tag)) {
}

gse::identifiable::identifiable(const std::filesystem::path& path) : m_id(generate_id(relative_stem(path, {}))) {
}

gse::identifiable::identifiable(const std::filesystem::path& path, const std::filesystem::path& base)
	: m_id(generate_id(relative_stem(path, base))) {
}

auto gse::identifiable::id() const -> gse::id {
	return m_id;
}

auto gse::identifiable::relative_stem(const std::filesystem::path& path, const std::filesystem::path& base) -> std::string {
	std::filesystem::path relative = path;
	if (!base.empty() && path.generic_native_encoded_string().starts_with(base.generic_native_encoded_string())) {
		relative = path.lexically_relative(base);
	}

	std::string result;
	for (auto it = relative.begin(); it != relative.end(); ++it) {
		if (!result.empty()) {
			result += '/';
		}
		result += it->native_encoded_string();
	}

	if (const std::size_t dot_pos = result.find_last_of('.'); dot_pos != std::string::npos) {
		result = result.substr(0, dot_pos);
	}

	return result;
}

gse::identifiable_owned::identifiable_owned(const id owner_id) : m_owner_id(owner_id) {
}

auto gse::identifiable_owned::owner_id() const -> id {
	return m_owner_id;
}

auto gse::identifiable_owned::swap_parent(const id new_parent_id) -> void {
	m_owner_id = new_parent_id;
}

auto gse::identifiable_owned::swap_parent(const identifiable& new_parent) -> void {
	swap_parent(new_parent.id());
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
			assert(it->second == tag, "ID collision for tag {} vs existing tag {}", tag, it->second);

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

	assert(!registry.by_uuid.contains(number), "ID number {} already exists", number);

	const id new_id(number);
	auto tag = std::to_string(number);

	registry.by_uuid.add(number, new_id);
	registry.tag_to_uuid[tag] = number;
	registry.uuid_to_tag[number] = std::move(tag);

	return new_id;
}

auto gse::generate_temp_id(const std::filesystem::path& path) -> id {
	std::string key = path.lexically_normal().generic_native_encoded_string();
#ifdef _WIN32
	std::ranges::transform(key, key.begin(), [](const unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
#endif
	return generate_temp_id(stable_id(key));
}

auto gse::find(const uuid number) -> id {
	const auto found_id = try_find(number);
	assert(found_id.has_value(), "ID {} not found", number);
	return *found_id;
}

auto gse::find(const std::string_view tag) -> id {
	const auto found_id = try_find(tag);
	assert(found_id.has_value(), "ID {} not found", tag);
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
	assert(it != registry.uuid_to_tag.end(), "Tag for id {} not found", number);
	return it->second;
}

auto gse::number(const std::string_view tag) -> uuid {
	const auto& [mutex, registry] = id_registry();
	std::shared_lock lock(mutex);
	const auto it = registry.tag_to_uuid.find(tag);
	assert(it != registry.tag_to_uuid.end(), "Tag '{}' not found", tag);
	return it->second;
}
