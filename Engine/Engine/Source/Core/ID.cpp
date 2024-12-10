#include "Core/ID.h"

namespace {
    std::vector<std::weak_ptr<gse::id>> ids;
    std::unordered_map<int, std::weak_ptr<gse::id>> idMap;
    std::unordered_map<std::string, std::weak_ptr<gse::id>> tagMap;

    void registerObject(const std::shared_ptr<gse::id>& obj) {
        idMap[obj->number] = obj;
        tagMap[obj->tag] = obj;
    }
}

namespace gse {
    std::shared_ptr<id> generate_id(const std::string& tag) {
        const int newID = static_cast<int>(ids.size());

        std::string newTag = tag;
		if (const auto it = tagMap.find(tag); it != tagMap.end()) {
			newTag += std::to_string(newID);
		}

        auto idPtr = std::shared_ptr<id>(new id(newID, newTag));
        ids.push_back(idPtr);
        registerObject(idPtr);
        return idPtr;
    }

    bool is_object_alive(const int id) {
        if (const auto it = idMap.find(id); it != idMap.end()) {
            if (!it->second.expired()) {
                return true;
            }
            idMap.erase(it);
        }
        return false;
    }

    std::weak_ptr<id> grab_id(const int id) {
        if (const auto it = idMap.find(id); it != idMap.end()) {
            return it->second;
        }
        return {};
    }

    std::weak_ptr<id> grab_id(const std::string& tag) {
        if (const auto it = tagMap.find(tag); it != tagMap.end()) {
            return it->second;
        }
        return {};
    }
}
