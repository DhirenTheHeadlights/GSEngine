module gse.core;

import std;

import gse.assert;

namespace gse {
	using uuid = std::uint64_t;

	constexpr uuid empty_slot_key = std::numeric_limits<uuid>::max();
	constexpr std::size_t initial_slot_count = 1024;

	struct id_slot {
		std::atomic<uuid> key;
		std::atomic<const std::string*> tag;
	};

	struct id_table {
		id_slot* slots = nullptr;
		std::size_t mask = 0;
	};

	auto allocate_table(
		std::size_t slot_count
	) -> const id_table*;

	auto lookup(
		const id_table& table,
		uuid key
	) -> const std::string*;

	auto lookup_tag(
		const id_table& table,
		std::string_view text
	) -> uuid;

	auto place(
		const id_table& table,
		uuid key,
		const std::string* tag
	) -> void;

	struct id_registry_data {
		std::atomic<const id_table*> table = allocate_table(initial_slot_count);
		std::deque<std::string> tags;
		std::vector<const id_table*> retired;
		std::size_t used = 0;
		std::mutex write_mutex;
	};

	auto id_registry() -> id_registry_data&;

	auto current_table() -> const id_table&;

	auto grow_if_needed(
		id_registry_data& registry
	) -> void;

	auto intern(
		uuid number,
		std::string tag,
		bool require_unique
	) -> id;
}

auto gse::allocate_table(const std::size_t slot_count) -> const id_table* {
	auto* slots = new id_slot[slot_count];
	for (std::size_t i = 0; i < slot_count; ++i) {
		slots[i].key.store(empty_slot_key, std::memory_order_relaxed);
		slots[i].tag.store(nullptr, std::memory_order_relaxed);
	}
	return new id_table{
		.slots = slots,
		.mask = slot_count - 1
	};
}

auto gse::lookup(const id_table& table, const uuid key) -> const std::string* {
	std::size_t index = static_cast<std::size_t>(key) & table.mask;
	for (;;) {
		const uuid probed = table.slots[index].key.load(std::memory_order_acquire);
		if (probed == key) {
			return table.slots[index].tag.load(std::memory_order_relaxed);
		}
		if (probed == empty_slot_key) {
			return nullptr;
		}
		index = (index + 1) & table.mask;
	}
}

auto gse::lookup_tag(const id_table& table, const std::string_view text) -> uuid {
	const uuid key = stable_id(text);
	std::size_t index = static_cast<std::size_t>(key) & table.mask;
	for (;;) {
		const uuid probed = table.slots[index].key.load(std::memory_order_acquire);
		if (probed == empty_slot_key) {
			return empty_slot_key;
		}
		if (probed == key && *table.slots[index].tag.load(std::memory_order_relaxed) == text) {
			return key;
		}
		index = (index + 1) & table.mask;
	}
}

auto gse::place(const id_table& table, const uuid key, const std::string* tag) -> void {
	std::size_t index = static_cast<std::size_t>(key) & table.mask;
	while (table.slots[index].key.load(std::memory_order_relaxed) != empty_slot_key) {
		index = (index + 1) & table.mask;
	}
	table.slots[index].tag.store(tag, std::memory_order_relaxed);
	table.slots[index].key.store(key, std::memory_order_release);
}

auto gse::id_registry() -> id_registry_data& {
	static id_registry_data instance;
	return instance;
}

auto gse::current_table() -> const id_table& {
	return *id_registry().table.load(std::memory_order_acquire);
}

auto gse::grow_if_needed(id_registry_data& registry) -> void {
	const id_table* current = registry.table.load(std::memory_order_relaxed);
	const std::size_t capacity = current->mask + 1;

	if ((registry.used + 1) * 2 <= capacity) {
		return;
	}

	const id_table* grown = allocate_table(capacity * 2);
	for (std::size_t i = 0; i <= current->mask; ++i) {
		if (const uuid moved = current->slots[i].key.load(std::memory_order_relaxed); moved != empty_slot_key) {
			place(*grown, moved, current->slots[i].tag.load(std::memory_order_relaxed));
		}
	}

	registry.table.store(grown, std::memory_order_release);
	registry.retired.push_back(current);
}

auto gse::intern(const uuid number, std::string tag, const bool require_unique) -> id {
	auto& registry = id_registry();
	std::lock_guard lock(registry.write_mutex);

	if (const std::string* existing = lookup(current_table(), number)) {
		assert(!require_unique, "ID number {} already exists", number);
		assert(*existing == tag, "ID collision for tag {} vs existing tag {}", tag, *existing);
		return generate_temp_id(number);
	}

	grow_if_needed(registry);
	place(current_table(), number, &registry.tags.emplace_back(std::move(tag)));
	++registry.used;

	return generate_temp_id(number);
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
	const uuid number = stable_id(tag);

	if (const std::string* existing = lookup(current_table(), number)) {
		assert(*existing == tag, "ID collision for tag {} vs existing tag {}", tag, *existing);
		return generate_temp_id(number);
	}

	return intern(number, std::string(tag), false);
}

auto gse::generate_id(const std::uint64_t number) -> id {
	assert(number != empty_slot_key, "ID number {} is reserved", number);
	return intern(number, std::to_string(number), true);
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
	if (const uuid number = lookup_tag(current_table(), tag); number != empty_slot_key) {
		return generate_temp_id(number);
	}
	return std::nullopt;
}

auto gse::try_find(const uuid number) -> std::optional<id> {
	if (lookup(current_table(), number)) {
		return generate_temp_id(number);
	}
	return std::nullopt;
}

auto gse::find_or_generate_id(const std::string_view tag) -> id {
	return generate_id(tag);
}

auto gse::find_or_generate_id(const uuid number) -> id {
	if (lookup(current_table(), number)) {
		return generate_temp_id(number);
	}
	return intern(number, std::to_string(number), false);
}

auto gse::exists(const uuid number) -> bool {
	return lookup(current_table(), number) != nullptr;
}

auto gse::exists(const std::string_view tag) -> bool {
	return lookup_tag(current_table(), tag) != empty_slot_key;
}

auto gse::tag(const uuid number) -> std::string_view {
	const std::string* found = lookup(current_table(), number);
	assert(found != nullptr, "Tag for id {} not found", number);
	return *found;
}

auto gse::number(const std::string_view tag) -> uuid {
	const uuid found = lookup_tag(current_table(), tag);
	assert(found != empty_slot_key, "Tag '{}' not found", tag);
	return found;
}
