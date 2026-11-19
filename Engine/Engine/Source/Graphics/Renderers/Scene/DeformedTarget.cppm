export module gse.graphics:deformed_target;

import std;

import gse.gpu;

export namespace gse::renderer {
	struct deformed_slots {
		gpu::bindless_slot current;
		gpu::bindless_slot previous;
	};

	class deformed_target {
	public:
		auto reserve(
			gpu::device& device,
			std::size_t bytes,
			std::size_t stride,
			std::string_view tag
		) -> void;

		auto begin_write(
		) -> void;

		auto write_buffer(
		) -> gpu::buffer&;

		auto finish_write(
		) -> void;

		[[nodiscard]] auto current(
		) const -> const gpu::buffer&;

		[[nodiscard]] auto slots(
		) const -> deformed_slots;

	private:
		std::array<gpu::buffer, 2> m_vertices;
		std::size_t m_capacity = 0;
		std::uint32_t m_write_index = 0;
		std::uint32_t m_writes = 0;
	};
}

auto gse::renderer::deformed_target::reserve(gpu::device& device, const std::size_t bytes, const std::size_t stride, const std::string_view tag) -> void {
	if (m_capacity >= bytes) {
		return;
	}
	constexpr gpu::buffer_usage usage{ gpu::buffer_flag::storage, gpu::buffer_flag::acceleration_structure_build_input };
	for (auto& buffer : m_vertices) {
		buffer = device.create_buffer(
			{
				.size = bytes,
				.stride = stride,
				.usage = usage,
				.writable = true,
			},
			tag
		);
	}
	m_capacity = bytes;
	m_write_index = 0;
	m_writes = 0;
}

auto gse::renderer::deformed_target::begin_write() -> void {
	m_write_index ^= 1u;
}

auto gse::renderer::deformed_target::write_buffer() -> gpu::buffer& {
	return m_vertices[m_write_index];
}

auto gse::renderer::deformed_target::finish_write() -> void {
	++m_writes;
}

auto gse::renderer::deformed_target::current() const -> const gpu::buffer& {
	return m_vertices[m_write_index];
}

auto gse::renderer::deformed_target::slots() const -> deformed_slots {
	const auto previous_index = m_write_index ^ 1u;
	if (!m_vertices[m_write_index].valid()) {
		return {};
	}
	return {
		.current = m_vertices[m_write_index].slot(),
		.previous = m_writes > 0 && m_vertices[previous_index].valid()
			? m_vertices[previous_index].slot()
			: gpu::bindless_slot{},
	};
}
