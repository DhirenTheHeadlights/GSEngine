#include "Core/ID.h"

namespace {
    std::vector<std::weak_ptr<gse::id>> g_ids;
    std::unordered_map<int, std::weak_ptr<gse::id>> g_id_map;
    std::unordered_map<std::string, std::weak_ptr<gse::id>> g_tag_map;

    void register_object(const std::shared_ptr<gse::id>& obj) {
        g_id_map[obj->number] = obj;
        g_tag_map[obj->tag] = obj;
    }
}

namespace gse {
    std::shared_ptr<id> generate_id(const std::string& tag) {
        const int new_id = static_cast<int>(g_ids.size());

        std::string new_tag = tag;
		if (const auto it = g_tag_map.find(tag); it != g_tag_map.end()) {
			new_tag += std::to_string(new_id);
		}

        auto id_ptr = std::shared_ptr<id>(new id(new_id, new_tag));
        g_ids.push_back(id_ptr);
        register_object(id_ptr);
        return id_ptr;
    }

    bool is_object_alive(const int id) {
        if (const auto it = g_id_map.find(id); it != g_id_map.end()) {
            if (!it->second.expired()) {
                return true;
            }
            g_id_map.erase(it);
        }
        return false;
    }

    std::weak_ptr<id> grab_id(const int id) {
        if (const auto it = g_id_map.find(id); it != g_id_map.end()) {
            return it->second;
        }
        return {};
    }

    std::weak_ptr<id> grab_id(const std::string& tag) {
        if (const auto it = g_tag_map.find(tag); it != g_tag_map.end()) {
            return it->second;
        }
        return {};
    }
}
