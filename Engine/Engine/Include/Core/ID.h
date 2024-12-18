#pragma once

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

#include "Platform/PermaAssert.h"

namespace gse {
    class id;

    std::shared_ptr<id> generate_id(const std::string& tag);
    bool is_object_alive(int id);
    std::weak_ptr<id> grab_id(int id);
    std::weak_ptr<id> grab_id(const std::string& tag);

    class id {
    public:
        int number;
		std::string tag = "You're grabbing an object without a tag. This should not be possible.\n";

        bool operator==(const id& other) const {
            if (number == -1 || other.number == -1) {
				std::cerr << "You're comparing an object without and id.\n";
                return false;
            }
            return number == other.number;
        }
    private:
        explicit id(const int id, std::string tag) : number(id), tag(std::move(tag)) {}
        id(std::string tag) : number(-1), tag(std::move(tag)) {}

        friend std::shared_ptr<id> generate_id(const std::string& tag);
    };

    class identifiable {
	public:
		identifiable(const std::string& tag) : m_id(generate_id(tag)) {}
		identifiable(const std::shared_ptr<id>& id) : m_id(id) {}

		void set_id(const std::shared_ptr<id>& id) { m_id = id; }
        std::weak_ptr<id> get_id() const { return m_id; }
	protected:
		std::shared_ptr<id> m_id;
    };
}
