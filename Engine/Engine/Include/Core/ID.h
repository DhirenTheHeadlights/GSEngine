#pragma once

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

#include "Platform/PermaAssert.h"

namespace Engine {
    class ID {
    public:
        int id;
        std::string tag = "You should never see this message. If you do, something went wrong.";

        ID(std::string tag) : id(-1), tag(std::move(tag)) {}

        bool operator==(const ID& other) const {
            if (id == -1 || other.id == -1) {
                return false;
            }
            return id == other.id;
        }
    private:
        explicit ID(const int id, std::string tag) : id(id), tag(std::move(tag)) {}

        friend class IDHandler;
    };

    class IDHandler {
    public:
		std::shared_ptr<ID> generateID(const std::string& tag) {
            permaAssertComment(!tagMap.contains(tag), "Duplicate Tag in IDHandler");

	        const int newID = static_cast<int>(ids.size());
            auto idPtr = std::shared_ptr<ID>(new ID(newID, tag));
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

        std::weak_ptr<ID> grabID (const int id) {
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

    private:
        void registerObject(const std::shared_ptr<ID>& obj) {
            idMap[obj->id] = obj;
			tagMap[obj->tag] = obj;
        }

        std::vector<std::weak_ptr<ID>> ids;
        std::unordered_map<int, std::weak_ptr<ID>> idMap;
        std::unordered_map<std::string, std::weak_ptr<ID>> tagMap;
    };
}
