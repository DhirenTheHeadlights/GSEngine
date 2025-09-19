export module gse.utility:identifiable;

import std;

import :id;

export namespace gse {
	template <typename T>
	class identifiable {
	public:
		explicit identifiable(std::string_view tag);
		explicit identifiable(const std::filesystem::path& path);

		auto id() const -> const id&;
		auto operator==(const identifiable& other) const -> bool = default;
	private:
		id_t<T> m_id;

		static auto filename(const std::filesystem::path& path) -> std::string ;
	};
}

gse::identifiable::identifiable(const std::string_view tag) : m_id(generate_id(tag)) {}

gse::identifiable::identifiable(const std::filesystem::path& path): m_id(generate_id(filename(path))) {}

auto gse::identifiable::id() const -> const gse::id& {
	return m_id;
}

auto gse::identifiable::filename(const std::filesystem::path& path) -> std::string {
	std::string name = path.filename().string();
	if (const size_t dot_pos = name.find_first_of('.'); dot_pos != std::string::npos) {
		return name.substr(0, dot_pos);
	}
	return name;
}
