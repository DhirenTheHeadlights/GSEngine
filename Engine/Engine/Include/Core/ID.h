#pragma once

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

#include "Platform/PermaAssert.h"

namespace gse {
    class ID;

    std::shared_ptr<ID> generateID(const std::string& tag);
    bool isObjectAlive(int id);
    std::weak_ptr<ID> grabID(int id);
    std::weak_ptr<ID> grabID(const std::string& tag);

    class ID {
    public:
        int id;
		std::string tag = "You're grabbing an object without a tag. This should not be possible.\n";

        bool operator==(const ID& other) const {
            if (id == -1 || other.id == -1) {
				std::cerr << "You're comparing an object without and id.\n";
                return false;
            }
            return id == other.id;
        }
    private:
        explicit ID(const int id, std::string tag) : id(id), tag(std::move(tag)) {}
        ID(std::string tag) : id(-1), tag(std::move(tag)) {}

        friend std::shared_ptr<ID> generateID(const std::string& tag);
    };
}
