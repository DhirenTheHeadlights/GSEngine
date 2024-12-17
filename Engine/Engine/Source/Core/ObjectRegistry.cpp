#include "Core/ObjectRegistry.h"

#include "Core/Clock.h"

namespace {
	struct weak_ptr_hash {
		template <typename T>
		std::size_t operator()(const std::weak_ptr<T>& weak) const {
			if (auto shared = weak.lock()) {
				return std::hash<T*>()(shared.get());
			}
			return 0; // Fallback for expired pointers
		}
	};

	struct weak_ptr_equal {
		template <typename T>
		bool operator()(const std::weak_ptr<T>& lhs, const std::weak_ptr<T>& rhs) const {
			auto lhs_locked = lhs.lock();
			auto rhs_locked = rhs.lock();
			return lhs_locked && rhs_locked && lhs_locked == rhs_locked;
		}
	};

	std::vector<gse::object*> g_active_objects;
	std::unordered_map<std::weak_ptr<gse::id>, std::vector<gse::object*>, weak_ptr_hash, weak_ptr_equal> g_object_lists;
	std::unordered_map<std::weak_ptr<gse::id>, std::vector<gse::id*>, weak_ptr_hash, weak_ptr_equal> g_id_lists;

	gse::clock g_clean_up_clock;
}

bool gse::registry::is_object_in_list(const std::weak_ptr<id>& list_id, const object* object) {
	const auto it = g_object_lists.find(list_id);
	if (it == g_object_lists.end()) {
		return false;
	}
	const auto& list = it->second;
	return std::ranges::find(list, object) != list.end();
}

bool gse::registry::is_id_in_list(const std::weak_ptr<id>& list_id, const id* id) {
	const auto it = g_id_lists.find(list_id);
	if (it == g_id_lists.end()) {
		return false;
	}
	const auto& list = it->second;
	return std::ranges::find(list, id) != list.end();
}

void gse::registry::register_object(object* new_object) {
	g_active_objects.push_back(new_object);
}

void gse::registry::unregister_object(const object* object_to_remove) {
	std::erase_if(g_active_objects, [&object_to_remove](const object* required_object) {
		return required_object == object_to_remove;
		});
}

void gse::registry::add_new_object_list(const std::weak_ptr<id>& list_id, const std::vector<object*>& objects) {
	g_object_lists[list_id] = objects;
}

void gse::registry::add_new_id_list(const std::weak_ptr<id>& list_id, const std::vector<id*>& ids) {
	g_id_lists[list_id] = ids;
}

void gse::registry::add_object_to_list(const std::weak_ptr<id>& list_id, object* object) {
	g_object_lists[list_id].push_back(object);
}

void gse::registry::add_id_to_list(const std::weak_ptr<id>& list_id, id* id) {
	g_id_lists[list_id].push_back(id);
}

void gse::registry::remove_object_from_list(const std::weak_ptr<id>& list_id, const object* object) {
	std::erase_if(g_object_lists[list_id], [&](const gse::object* obj) {
		return obj == object;
		});
}

void gse::registry::remove_id_from_list(const std::weak_ptr<id>& list_id, const id* id) {
	std::erase_if(g_id_lists[list_id], [&](const gse::id* obj) {
		return obj == id;
		});
}

void gse::registry::periodically_clean_up_stale_lists(const time& clean_up_interval) {
	if (g_clean_up_clock.get_elapsed_time() < clean_up_interval) {
		return;
	}

	for (auto it = g_object_lists.begin(); it != g_object_lists.end();) {
		if (it->first.expired()) {
			it = g_object_lists.erase(it);
		}
		else {
			++it;
		}
	}

	for (auto it = g_id_lists.begin(); it != g_id_lists.end();) {
		if (it->first.expired()) {
			it = g_id_lists.erase(it);
		}
		else {
			++it;
		}
	}

	g_clean_up_clock.reset();
}