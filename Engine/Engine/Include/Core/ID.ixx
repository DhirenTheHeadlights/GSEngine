export module gse.core.id;

import std;
import gse.platform.perma_assert;

export namespace gse {
    class id;

    auto generate_id(const std::string& tag) -> std::unique_ptr<id>;
	auto remove_id(const id* id) -> void;
    auto get_id(std::int32_t number) -> id*;
    auto get_id(std::string_view tag) -> id*;
	auto does_id_exist(std::int32_t number) -> bool;
	auto does_id_exist(std::string_view tag) -> bool;

    class id {
    public:
        ~id();

		auto operator==(const id& other) const -> bool;

		auto number() const -> std::int32_t;
		auto tag() const -> std::string_view;
    private:
        explicit id(int id, const std::string& tag);
        id(const std::string& tag) : m_number(-1), m_tag(tag) {}

        std::int32_t m_number;
		std::string_view m_tag;

		friend auto generate_id(const std::string& tag) -> std::unique_ptr<id>;
    };

    class identifiable {
	public:
		identifiable(const std::string& tag);

		auto get_id() const -> id*;
		auto operator==(const identifiable& other) const -> bool;
	private:
		std::unique_ptr<id> m_id;
    };
}
