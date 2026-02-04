export module gse.utility:registry;

import std;

import :id;
import :non_copyable;
import :misc;
import :frame_sync;
import :task;
import :lambda_traits;

import :entity;
import :concepts;
import :component_link;
import :hook_link;

export namespace gse {
	struct deferred_action_desc {
		id owner;
		std::vector<std::type_index> reads;
		std::vector<std::type_index> writes;
		std::function<bool(registry&)> execute;

		auto conflicts_with(const deferred_action_desc& other) const -> bool {
			if (owner == other.owner) {
				return true;
			}
			for (const auto& w : writes) {
				if (std::ranges::contains(other.writes, w)) {
					return true;
				}
			}
			return false;
		}
	};

	class registry final : public non_copyable {
	public:
		using uuid = std::uint32_t;
		using deferred_action = std::function<bool(registry&)>;

		registry();

		auto create(
			const std::string& name
		) -> id;

		auto activate(
			id id
		) -> void;

		auto remove(
			id id
		) -> void;

		auto update(
		) -> void;

		auto render(
		) -> void;

		auto add_deferred_action(
			id owner_id,
			deferred_action&& action
		) -> void;

		template <typename F>
		auto defer(
			id owner,
			F&& action
		) -> void;

		template <is_component U, typename... Args>
		auto add_component(
			id owner_id,
			Args&&... args
		) -> U*;

		template <is_entity_hook U, typename... Args>
		auto add_hook(
			id owner_id,
			Args&&... args
		) -> U*;

		template <typename U>
		auto remove_link(
			id id
		) -> void;

		template <typename U>
		auto linked_objects_read(
		) const -> std::span<const U>;

		template <typename U>
		auto linked_objects_write(
		) -> std::span<U>;

		auto all_hooks(
		) const -> std::vector<hook<entity>*>;

		template <typename U>
		auto linked_object_read(
			id id
		) -> const U&;

		template <typename U>
		auto linked_object_write(
			id id
		) -> U&;

		template <typename U>
		auto try_linked_object_read(
			id id
		) -> const U*;

		template <typename U>
		auto try_linked_object_write(
			id id
		) -> U*;

		template <typename U>
		auto try_linked_object_by_link_id_read(
			id link_id
		) -> const U*;

		template <typename U>
		auto try_linked_object_by_link_id_write(
			id link_id
		) -> U*;

		template <typename U>
		auto linked_object_by_link_id_read(
			id link_id
		) -> const U&;

		template <typename U>
		auto linked_object_by_link_id_write(
			id link_id
		) -> U&;

		auto exists(
			id id
		) const -> bool;

		auto active(
			id id
		) const -> bool;

		auto ensure_exists(
			id id
		) -> void;

		auto ensure_active(
			id id
		) -> void;

		template <typename U>
		auto drain_component_adds(
		) -> std::vector<id>;

		template <typename U>
		auto drain_component_updates(
		) -> std::vector<id>;

		template <typename U>
		auto drain_component_removes(
		) -> std::vector<id>;

		template <typename U>
		auto mark_component_updated(
			id owner_id
		) -> void;

		template <typename U>
		static auto any_components(
			std::span<const std::reference_wrapper<registry>> registries
		) -> bool;

		template <typename U, typename Fn>
		auto for_each(
			Fn&& fn
		) -> void;
	private:
		id_mapped_collection<entity> m_active_entities;
		std::unordered_set<id> m_inactive_ids;
		std::vector<std::uint32_t> m_free_indices;

		std::unordered_map<std::type_index, std::unique_ptr<component_link_base>> m_component_links;
		std::unordered_map<std::type_index, std::unique_ptr<hook_link_base>> m_hook_links;

		std::unordered_map<id, std::vector<deferred_action>> m_deferred_actions;
		std::vector<deferred_action_desc> m_typed_deferred_actions;
		std::mutex m_deferred_actions_mutex;

		std::size_t m_read_index = 0;
		std::size_t m_write_index = 1;
	};
}

gse::registry::registry() {
	frame_sync::on_end([this] {
		std::swap(m_read_index, m_write_index);

		for (const auto& link : m_component_links | std::views::values) {
			link->flip(m_read_index, m_write_index);
		}
	});
}

auto gse::registry::create(const std::string& name) -> id {
	const auto id = generate_id(name);
	m_inactive_ids.insert(id);
	return id;
}

auto gse::registry::activate(id id) -> void {
	assert(
		m_inactive_ids.contains(id),
		std::source_location::current(),
		"Cannot activate entity with id {}: it is not inactive.",
		id
	);

	m_inactive_ids.erase(id);

	entity object;
	if (!m_free_indices.empty()) {
		object.index = m_free_indices.back();
		m_free_indices.pop_back();
		if (auto* old_entity = m_active_entities.try_get(id)) {
			old_entity->generation++;
			object.generation = old_entity->generation;
		}
	}
	else {
		object.index = static_cast<std::uint32_t>(m_active_entities.size());
	}
	m_active_entities.add(id, object);

	bool work_was_done;
	do {
		work_was_done = false;

		for (const auto& link : m_component_links | std::views::values) {
			if (link->activate(id)) {
				work_was_done = true;
			}
		}

		std::vector<hook_link_base*> current_hook_links;
		current_hook_links.reserve(m_hook_links.size());
		for (const auto& val : m_hook_links | std::views::values) {
			current_hook_links.push_back(val.get());
		}

		for (auto* link : current_hook_links) {
			if (link->activate(id)) {
				link->initialize_hook(id);
				work_was_done = true;
			}
		}
	} while (work_was_done);

	if (const auto it = m_deferred_actions.find(id); it != m_deferred_actions.end()) {
		auto& actions = it->second;

		std::erase_if(
			actions,
			[&](auto& action) {
				return action(*this);
			}
		);

		if (actions.empty()) {
			m_deferred_actions.erase(it);
		}
	}
}

auto gse::registry::remove(const id id) -> void {
	if (active(id)) {
		if (const auto* entity = m_active_entities.try_get(id)) {
			m_free_indices.push_back(entity->index);
		}
		m_active_entities.remove(id);
	}
	else {
		m_inactive_ids.erase(id);
	}

	for (const auto& link : m_component_links | std::views::values) {
		link->remove(id);
	}
	for (const auto& link : m_hook_links | std::views::values) {
		link->remove(id);
	}
}

auto gse::registry::update() -> void {
	std::unordered_map<id, std::vector<deferred_action>> legacy_actions;
	std::vector<deferred_action_desc> typed_actions;

	{
		std::lock_guard lock(m_deferred_actions_mutex);
		legacy_actions = std::move(m_deferred_actions);
		m_deferred_actions.clear();
		typed_actions = std::move(m_typed_deferred_actions);
		m_typed_deferred_actions.clear();
	}

	for (auto& [owner_id, actions] : legacy_actions) {
		for (auto& action : actions) {
			if (!action(*this)) {
				std::lock_guard lock(m_deferred_actions_mutex);
				m_deferred_actions[owner_id].push_back(std::move(action));
			}
		}
	}

	if (typed_actions.empty()) {
		return;
	}

	std::vector<std::vector<deferred_action_desc*>> batches;
	std::vector<bool> scheduled(typed_actions.size(), false);

	while (true) {
		std::vector<deferred_action_desc*> batch;

		for (std::size_t i = 0; i < typed_actions.size(); ++i) {
			if (scheduled[i]) {
				continue;
			}

			bool can_add = true;
			for (const auto* other : batch) {
				if (typed_actions[i].conflicts_with(*other)) {
					can_add = false;
					break;
				}
			}

			if (can_add) {
				batch.push_back(&typed_actions[i]);
				scheduled[i] = true;
			}
		}

		if (batch.empty()) {
			break;
		}
		batches.push_back(std::move(batch));
	}

	std::vector<deferred_action_desc> retries;

	for (auto& batch : batches) {
		if (batch.size() == 1) {
			if (!batch[0]->execute(*this)) {
				retries.push_back(std::move(*batch[0]));
			}
		}
		else {
			std::vector<deferred_action_desc*> batch_retries;
			std::mutex retry_mutex;

			task::parallel_for(0uz, batch.size(), [&](std::size_t i) {
				if (!batch[i]->execute(*this)) {
					std::lock_guard lock(retry_mutex);
					batch_retries.push_back(batch[i]);
				}
			});

			for (auto* r : batch_retries) {
				retries.push_back(std::move(*r));
			}
		}
	}

	if (!retries.empty()) {
		std::lock_guard lock(m_deferred_actions_mutex);
		for (auto& r : retries) {
			m_typed_deferred_actions.push_back(std::move(r));
		}
	}
}

auto gse::registry::render() -> void {
}

auto gse::registry::add_deferred_action(const id owner_id, deferred_action&& action) -> void {
	std::lock_guard lock(m_deferred_actions_mutex);
	m_deferred_actions[owner_id].push_back(std::move(action));
}

template <typename F>
auto gse::registry::defer(const id owner, F&& action) -> void {
	using traits = lambda_traits<std::decay_t<F>>;

	deferred_action_desc desc;
	desc.owner = owner;
	desc.reads = traits::reads();
	desc.writes = traits::writes();

	if constexpr (traits::arity == 0) {
		desc.execute = [action = std::forward<F>(action)](registry&) mutable -> bool {
			return action();
		};
	}
	else {
		desc.execute = [owner, action = std::forward<F>(action)](registry& reg) mutable -> bool {
			using arg_tuple = traits::arg_tuple;

			auto resolve = [&]<std::size_t... Is>(std::index_sequence<Is...>) -> std::optional<std::tuple<std::add_pointer_t<std::remove_reference_t<std::tuple_element_t<Is, arg_tuple>>>...>> {
				using ptr_tuple = std::tuple<std::add_pointer_t<std::remove_reference_t<std::tuple_element_t<Is, arg_tuple>>>...>;
				ptr_tuple result;
				bool all_resolved = true;

				auto try_resolve = [&]<std::size_t I>() {
					using arg_t = std::tuple_element_t<I, arg_tuple>;
					using base_t = param_base_type<arg_t>;

					if constexpr (is_write_param_v<arg_t>) {
						std::get<I>(result) = reg.try_linked_object_write<base_t>(owner);
					}
					else {
						std::get<I>(result) = const_cast<std::remove_reference_t<arg_t>*>(
							reg.try_linked_object_read<base_t>(owner)
						);
					}
					if (!std::get<I>(result)) {
						all_resolved = false;
					}
				};

				(try_resolve.template operator()<Is>(), ...);

				if (!all_resolved) {
					return std::nullopt;
				}
				return result;
			};

			auto resolved = resolve(std::make_index_sequence<traits::arity>{});
			if (!resolved) {
				return false;
			}

			auto invoke = [&]<std::size_t... Is>(std::index_sequence<Is...>) {
				return action(*std::get<Is>(*resolved)...);
			};

			return invoke(std::make_index_sequence<traits::arity>{});
		};
	}

	std::lock_guard lock(m_deferred_actions_mutex);
	m_typed_deferred_actions.push_back(std::move(desc));
}

template <gse::is_component U, typename... Args>
auto gse::registry::add_component(id owner_id, Args&&... args) -> U* {
	assert(exists(owner_id), std::source_location::current(), "Cannot add component to entity with id {}: it does not exist.", owner_id);

	const auto type_idx = std::type_index(typeid(U));
	if (!m_component_links.contains(type_idx)) {
		auto link = std::make_unique<component_link<U>>();
		link->bind(m_read_index, m_write_index);
		m_component_links[type_idx] = std::move(link);
	}

	auto& lnk = static_cast<component_link<U>&>(*m_component_links.at(type_idx));
	auto* comp_ptr = lnk.add(owner_id, this, std::forward<Args>(args)...);

	if (active(owner_id)) {
		lnk.activate(owner_id);
	}

	return comp_ptr;
}

template <gse::is_entity_hook U, typename... Args>
auto gse::registry::add_hook(id owner_id, Args&&... args) -> U* {
	assert(exists(owner_id), std::source_location::current(), "Cannot add hook to entity with id {}: it does not exist.", owner_id);

	const auto type_idx = std::type_index(typeid(U));
	if (!m_hook_links.contains(type_idx)) {
		m_hook_links[type_idx] = std::make_unique<hook_link<U>>(*this);
	}

	auto& lnk = static_cast<hook_link<U>&>(*m_hook_links.at(type_idx));
	auto* hook_ptr = lnk.add(owner_id, std::forward<Args>(args)...);

	if (active(owner_id)) {
		lnk.activate(owner_id);
		lnk.initialize_hook(owner_id);
	}

	return hook_ptr;
}

template <typename U>
auto gse::registry::remove_link(const id id) -> void {
	const auto type_idx = std::type_index(typeid(U));

	if constexpr (is_entity_hook<U>) {
		if (const auto it = m_hook_links.find(type_idx); it != m_hook_links.end()) {
			it->second->remove(id);
		}
	}
	else {
		if (const auto it = m_component_links.find(type_idx); it != m_component_links.end()) {
			it->second->remove(id);
		}
	}
}

template <typename U>
auto gse::registry::linked_objects_read() const -> std::span<const U> {
	const auto type_idx = std::type_index(typeid(U));

	if constexpr (is_entity_hook<U>) {
		const auto it = m_hook_links.find(type_idx);

		if (it == m_hook_links.end()) {
			return std::span<const U>{};
		}

		auto& lnk = static_cast<hook_link<U>&>(*it->second);
		auto span_mut = lnk.objects();
		return std::span<const U>(span_mut.data(), span_mut.size());
	}
	else {
		const auto it = m_component_links.find(type_idx);

		if (it == m_component_links.end()) {
			return std::span<const U>{};
		}

		auto& lnk = static_cast<component_link<U>&>(*it->second);
		return lnk.objects_read();
	}
}

template <typename U>
auto gse::registry::linked_objects_write() -> std::span<U> {
	const auto type_idx = std::type_index(typeid(U));

	if constexpr (is_entity_hook<U>) {
		const auto it = m_hook_links.find(type_idx);

		if (it == m_hook_links.end()) {
			return std::span<U>{};
		}

		auto& lnk = static_cast<hook_link<U>&>(*it->second);
		return lnk.objects();
	}
	else {
		const auto it = m_component_links.find(type_idx);

		if (it == m_component_links.end()) {
			return std::span<U>{};
		}

		auto& lnk = static_cast<component_link<U>&>(*it->second);
		return lnk.objects_write();
	}
}

auto gse::registry::all_hooks() const -> std::vector<hook<entity>*> {
	std::vector<hook<entity>*> collected_hooks;

	for (const auto& link_ptr : m_hook_links | std::views::values) {
		auto hooks_from_link = link_ptr->hooks_as_base();
		collected_hooks.insert(collected_hooks.end(), hooks_from_link.begin(), hooks_from_link.end());
	}

	return collected_hooks;
}

template <typename U>
auto gse::registry::linked_object_read(id id) -> const U& {
	const U* ptr = try_linked_object_read<U>(id);
	assert(ptr, std::source_location::current(), "Linked object (read) of type {} with id {} not found.", typeid(U).name(), id);
	return *ptr;
}

template <typename U>
auto gse::registry::linked_object_write(id id) -> U& {
	const auto type_idx = std::type_index(typeid(U));

	const auto it = m_component_links.find(type_idx);

	assert(
		it != m_component_links.end(),
		std::source_location::current(),
		"Linked object (write) of type {} with id {} not found.",
		typeid(U).name(), id
	);

	auto& lnk = static_cast<component_link<U>&>(*it->second);
	if (auto* p = lnk.try_get_write(id)) {
		return *p;
	}

	assert(false, std::source_location::current(), "Linked object (write) of type {} with id {} not found.", typeid(U).name(), id);
	std::unreachable();
}

template <typename U>
auto gse::registry::try_linked_object_read(id id) -> const U* {
	const auto type_idx = std::type_index(typeid(U));

	if constexpr (is_entity_hook<U>) {
		const auto it = m_hook_links.find(type_idx);
		if (it == m_hook_links.end()) {
			return nullptr;
		}
		auto& lnk = static_cast<hook_link<U>&>(*it->second);
		return lnk.try_get(id);
	}
	else {
		const auto it = m_component_links.find(type_idx);
		if (it == m_component_links.end()) {
			return nullptr;
		}
		auto& lnk = static_cast<component_link<U>&>(*it->second);
		return lnk.try_get_read(id);
	}
}

template <typename U>
auto gse::registry::try_linked_object_write(id id) -> U* {
	const auto type_idx = std::type_index(typeid(U));

	if constexpr (is_entity_hook<U>) {
		const auto it = m_hook_links.find(type_idx);
		if (it == m_hook_links.end()) {
			return nullptr;
		}
		auto& lnk = static_cast<hook_link<U>&>(*it->second);
		return lnk.try_get(id);
	}
	else {
		const auto it = m_component_links.find(type_idx);
		if (it == m_component_links.end()) {
			return nullptr;
		}
		auto& lnk = static_cast<component_link<U>&>(*it->second);
		return lnk.try_get_write(id);
	}
}

template <typename U>
auto gse::registry::try_linked_object_by_link_id_read(id link_id) -> const U* {
	const auto type_idx = std::type_index(typeid(U));
	const auto it = m_component_links.find(type_idx);

	if (it == m_component_links.end()) {
		return nullptr;
	}

	auto& lnk = static_cast<component_link<U>&>(*it->second);
	return lnk.try_get_by_link_id_read(link_id);
}

template <typename U>
auto gse::registry::try_linked_object_by_link_id_write(id link_id) -> U* {
	const auto type_idx = std::type_index(typeid(U));
	const auto it = m_component_links.find(type_idx);

	if (it == m_component_links.end()) {
		return nullptr;
	}

	auto& lnk = static_cast<component_link<U>&>(*it->second);
	return lnk.try_get_by_link_id_write(link_id);
}

template <typename U>
auto gse::registry::linked_object_by_link_id_read(const id link_id) -> const U& {
	const U* ptr = try_linked_object_by_link_id_read<U>(link_id);
	assert(ptr);
	return *ptr;
}

template <typename U>
auto gse::registry::linked_object_by_link_id_write(const id link_id) -> U& {
	U* ptr = try_linked_object_by_link_id_write<U>(link_id);
	assert(ptr);
	return *ptr;
}

auto gse::registry::exists(const id id) const -> bool {
	return m_active_entities.contains(id) || m_inactive_ids.contains(id);
}

auto gse::registry::active(const id id) const -> bool {
	return m_active_entities.contains(id);
}

auto gse::registry::ensure_exists(const id id) -> void {
	if (exists(id)) {
		return;
	}
	m_inactive_ids.insert(id);
}

auto gse::registry::ensure_active(const id id) -> void {
	if (!exists(id)) {
		m_inactive_ids.insert(id);
	}
	if (!active(id)) {
		activate(id);
	}
}

template <typename U>
auto gse::registry::drain_component_adds() -> std::vector<id> {
	const auto type_idx = std::type_index(typeid(U));
	const auto it = m_component_links.find(type_idx);
	if (it == m_component_links.end()) return {};
	auto& lnk = static_cast<component_link<U>&>(*it->second);
	return lnk.drain_adds();
}

template <typename U>
auto gse::registry::drain_component_updates() -> std::vector<id> {
	const auto type_idx = std::type_index(typeid(U));
	const auto it = m_component_links.find(type_idx);
	if (it == m_component_links.end()) return {};
	auto& lnk = static_cast<component_link<U>&>(*it->second);
	return lnk.drain_updates();
}

template <typename U>
auto gse::registry::drain_component_removes() -> std::vector<id> {
	const auto type_idx = std::type_index(typeid(U));
	const auto it = m_component_links.find(type_idx);
	if (it == m_component_links.end()) return {};
	auto& lnk = static_cast<component_link<U>&>(*it->second);
	return lnk.drain_removes();
}

template <typename U>
auto gse::registry::mark_component_updated(const id owner_id) -> void {
	const auto type_idx = std::type_index(typeid(U));
	const auto it = m_component_links.find(type_idx);
	if (it == m_component_links.end()) return;
	auto& lnk = static_cast<component_link<U>&>(*it->second);
	lnk.mark_updated(owner_id);
}

template <typename U>
auto gse::registry::any_components(const std::span<const std::reference_wrapper<registry>> registries) -> bool {
	return std::ranges::any_of(
		registries, [](const auto& reg) {
		return !reg.get().template linked_objects_read<U>().empty();
	}
	);
}

template <typename U, typename Fn>
auto gse::registry::for_each(Fn&& fn) -> void {
	for (auto& obj : linked_objects_write<U>()) {
		fn(obj);
	}
}
