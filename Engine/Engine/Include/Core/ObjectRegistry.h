#pragma once

#include <Core/ID.h>
#include "Object/Object.h"

namespace gse::registry {
	bool is_object_in_list(const std::weak_ptr<id>& list_id, const object* object);
	bool is_id_in_list(const std::weak_ptr<id>& list_id, const id* id);

	void register_object(object* new_object);
	void unregister_object(const object* object_to_remove);

	void add_new_object_list(const std::weak_ptr<id>& list_id, const std::vector<object*>& objects = {});
	void add_new_id_list(const std::weak_ptr<id>& list_id, const std::vector<id*>& ids = {});

	void add_object_to_list(const std::weak_ptr<id>& list_id, object* object);
	void add_id_to_list(const std::weak_ptr<id>& list_id, id* id);

	void remove_object_from_list(const std::weak_ptr<id>& list_id, const object* object);
	void remove_id_from_list(const std::weak_ptr<id>& list_id, const id* id);

	void periodically_clean_up_stale_lists(const time& clean_up_interval = seconds(60.f));
}