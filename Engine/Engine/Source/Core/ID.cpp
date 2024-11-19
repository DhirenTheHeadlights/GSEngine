#include "Core/ID.h"

namespace {
    std::vector<std::weak_ptr<Engine::ID>> ids;
    std::unordered_map<int, std::weak_ptr<Engine::ID>> idMap;
    std::unordered_map<std::string, std::weak_ptr<Engine::ID>> tagMap;

    void registerObject(const std::shared_ptr<Engine::ID>& obj) {
        idMap[obj->id] = obj;
        tagMap[obj->tag] = obj;
    }
}

namespace Engine {
    std::shared_ptr<ID> generateID(const std::string& tag) {
        const int newID = static_cast<int>(ids.size());

        std::string newTag = tag;
		if (const auto it = tagMap.find(tag); it != tagMap.end()) {
			newTag += std::to_string(newID);
		}

        auto idPtr = std::shared_ptr<ID>(new ID(newID, newTag));
        ids.push_back(idPtr);
        registerObject(idPtr);
        return idPtr;
    }

    bool isObjectAlive(const int id) {
        if (const auto it = idMap.find(id); it != idMap.end()) {
            if (!it->second.expired()) {
                return true;
            }
            idMap.erase(it);
        }
        return false;
    }

    std::weak_ptr<ID> grabID(const int id) {
        if (const auto it = idMap.find(id); it != idMap.end()) {
            return it->second;
        }
        return {};
    }

    std::weak_ptr<ID> grabID(const std::string& tag) {
        if (const auto it = tagMap.find(tag); it != tagMap.end()) {
            return it->second;
        }
        return {};
    }
}
