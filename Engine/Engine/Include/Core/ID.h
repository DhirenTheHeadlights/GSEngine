#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Platform/PermaAssert.h"

namespace gse {
    class id;

    auto generate_id(const std::string& tag) -> std::unique_ptr<id>;
	auto generate_random_entity_placeholder_id() -> std::uint32_t;
	auto remove_id(const id* id) -> void;
    auto get_id(std::int32_t number) -> id*;
    auto get_id(std::string_view tag) -> id*;
	auto does_id_exist(std::int32_t number) -> bool;
	auto does_id_exist(std::string_view tag) -> bool;

    class id {
    public:
        ~id() {
			if (m_number != -1) {
                remove_id(this);
				std::cout << "Removed id " << m_number << '\n';
			}
        }

        auto operator==(const id& other) const -> bool {
            if (m_number == -1 || other.m_number == -1) {
				std::cerr << "You're comparing something without an id.\n";
                return false;
            }
            return m_number == other.m_number;
        }

        auto number() const -> std::int32_t {
            return m_number;
        }

		auto tag() const -> std::string_view {
			return m_tag;
		}
    private:
        explicit id(const int id, const std::string& tag) : m_number(id), m_tag(tag) {}
        id(const std::string& tag) : m_number(-1), m_tag(tag) {}

        std::int32_t m_number;
		std::string_view m_tag;

		friend auto generate_id(const std::string& tag) -> std::unique_ptr<id>;
    };

    class identifiable {
	public:
		identifiable(const std::string& tag) : m_id(generate_id(tag)) {}

		auto get_id() const -> id* {
			return m_id.get();
		}

		auto operator==(const identifiable& other) const -> bool {
			return *m_id == *other.m_id;
		}
	private:
		std::unique_ptr<id> m_id;
    };
}
