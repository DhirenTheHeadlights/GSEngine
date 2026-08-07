export module gse.graphics:source_reader;

import std;

import gse.core;
import gse.math;
import gse.physics;

export namespace gse::animation {
	enum class source_error : std::uint8_t {
		unreadable,
		magic_mismatch,
	};

	class source_reader {
	public:
		static auto open(
			const std::filesystem::path& path,
			std::string_view magic
		) -> std::expected<source_reader, source_error>;

		auto u8() -> std::uint8_t;
		auto u16() -> std::uint16_t;
		auto u32() -> std::uint32_t;
		auto f32() -> float;
		auto string() -> std::string;
		auto vec2() -> vec2f;
		auto vec3() -> vec3f;
		auto vec4() -> vec4f;
		auto translation() -> gse::vec3<displacement>;
		auto rotation() -> quat;
		auto matrix() -> mat4f;
		auto shape() -> physics::collision_shape;

		auto bytes(
			void* destination,
			std::size_t count
		) -> void;

		auto overran() const -> bool;
		auto remaining() const -> std::size_t;

	private:
		std::vector<std::byte> m_data;
		std::size_t m_cursor = 0;
		bool m_overran = false;
	};
}

auto gse::animation::source_reader::open(const std::filesystem::path& path, const std::string_view magic) -> std::expected<source_reader, source_error> {
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		return std::unexpected(source_error::unreadable);
	}

	source_reader reader;
	reader.m_data.resize(static_cast<std::size_t>(file.tellg()));
	file.seekg(0);
	file.read(reinterpret_cast<char*>(reader.m_data.data()), static_cast<std::streamsize>(reader.m_data.size()));

	if (reader.m_data.size() < magic.size()) {
		return std::unexpected(source_error::magic_mismatch);
	}
	if (std::memcmp(reader.m_data.data(), magic.data(), magic.size()) != 0) {
		return std::unexpected(source_error::magic_mismatch);
	}
	reader.m_cursor = magic.size();
	return reader;
}

auto gse::animation::source_reader::bytes(void* destination, const std::size_t count) -> void {
	if (m_overran || count > m_data.size() - m_cursor) {
		m_overran = true;
		std::memset(destination, 0, count);
		return;
	}
	std::memcpy(destination, m_data.data() + m_cursor, count);
	m_cursor += count;
}

auto gse::animation::source_reader::u8() -> std::uint8_t {
	std::uint8_t value = 0;
	bytes(&value, sizeof(value));
	return value;
}

auto gse::animation::source_reader::u16() -> std::uint16_t {
	std::uint16_t value = 0;
	bytes(&value, sizeof(value));
	return value;
}

auto gse::animation::source_reader::u32() -> std::uint32_t {
	std::uint32_t value = 0;
	bytes(&value, sizeof(value));
	return value;
}

auto gse::animation::source_reader::f32() -> float {
	float value = 0.f;
	bytes(&value, sizeof(value));
	return value;
}

auto gse::animation::source_reader::string() -> std::string {
	const auto length = u32();
	if (length > remaining()) {
		m_overran = true;
		return {};
	}
	std::string value(length, '\0');
	bytes(value.data(), value.size());
	return value;
}

auto gse::animation::source_reader::vec2() -> vec2f {
	const auto x = f32();
	const auto y = f32();
	return { x, y };
}

auto gse::animation::source_reader::vec3() -> vec3f {
	const auto x = f32();
	const auto y = f32();
	const auto z = f32();
	return { x, y, z };
}

auto gse::animation::source_reader::vec4() -> vec4f {
	const auto x = f32();
	const auto y = f32();
	const auto z = f32();
	const auto w = f32();
	return { x, y, z, w };
}

auto gse::animation::source_reader::translation() -> gse::vec3<displacement> {
	const auto x = f32();
	const auto y = f32();
	const auto z = f32();
	return {
		meters(x),
		meters(y),
		meters(z),
	};
}

auto gse::animation::source_reader::rotation() -> quat {
	const auto w = f32();
	const auto x = f32();
	const auto y = f32();
	const auto z = f32();
	return normalize(quat(w, x, y, z));
}

auto gse::animation::source_reader::matrix() -> mat4f {
	mat4f value;
	for (int row = 0; row < 4; ++row) {
		for (int column = 0; column < 4; ++column) {
			value[column][row] = f32();
		}
	}
	return value;
}

auto gse::animation::source_reader::shape() -> physics::collision_shape {
	const auto kind = u32();
	const auto x = meters(f32());
	const auto y = meters(f32());
	const auto z = meters(f32());

	if (kind == 1) {
		return physics::sphere_shape{
			.radius = x,
		};
	}
	if (kind == 2) {
		return physics::capsule_shape{
			.radius = x,
			.half_height = y,
		};
	}
	return physics::box_shape{
		.size = gse::vec3<length>(x, y, z) * 2.f,
	};
}

auto gse::animation::source_reader::overran() const -> bool {
	return m_overran;
}

auto gse::animation::source_reader::remaining() const -> std::size_t {
	return m_overran ? 0 : m_data.size() - m_cursor;
}
