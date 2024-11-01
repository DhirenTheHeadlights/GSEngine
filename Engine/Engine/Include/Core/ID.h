#pragma once

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

namespace Engine {
    class ID {
    public:
        int id;
        std::string tag = "You should never see this message. If you do, something went wrong.";

        ID(const std::string& tag) : id(-1), tag(tag) {}

        bool operator==(const ID& other) const {
            if (id == -1 || other.id == -1) {
                return false;
            }
            return id == other.id;
        }
    private:
        explicit ID(const int id, const std::string& tag) : id(id), tag(tag) {}

        friend class IDHandler;
    };

    class IDHandler {
    public:
        std::shared_ptr<ID> generateID(const std::string& tag) {
	        const int newID = static_cast<int>(ids.size());
            auto idPtr = std::shared_ptr<ID>(new ID(newID, tag));
            ids.push_back(idPtr);
            registerObject(idPtr);
            return idPtr;
        }

        void registerObject(const std::shared_ptr<ID>& obj) {
            idMap[obj->id] = obj;
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

    private:
        std::vector<std::shared_ptr<ID>> ids;
        std::unordered_map<int, std::weak_ptr<ID>> idMap;
    };
}
