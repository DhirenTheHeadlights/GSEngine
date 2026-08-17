export module gse.gpu:graph_channel;

import std;

import :device;
import :frame;

import gse.gpu_backend;
import gse.assert;

export namespace gse::gpu {
	struct readback_view {
		std::span<const std::byte> bytes;
		std::uint64_t generation = 0;
	};

	class readback_channel {
	public:
		readback_channel() = default;

		readback_channel(
			device& dev,
			const frame& frame,
			std::size_t size,
			std::string_view tag
		);

		[[nodiscard]] auto publish_target(
			std::uint64_t generation,
			std::size_t bytes
		) -> const buffer&;

		[[nodiscard]] auto latest() const -> readback_view;

	private:
		static constexpr std::uint32_t version_count = max_frames_in_flight + 1;

		struct version {
			buffer staging;
			std::uint64_t generation = 0;
			std::uint64_t recorded_frame = 0;
			std::size_t recorded_bytes = 0;
			bool recorded = false;
		};

		std::array<version, version_count> m_versions;
		const frame* m_frame = nullptr;
		std::uint32_t m_cursor = 0;
	};

	class upload_channel {
	public:
		upload_channel() = default;

		upload_channel(
			device& dev,
			const frame& frame,
			const buffer_desc& desc,
			std::string_view tag
		);

		[[nodiscard]] auto write_target() -> const buffer&;

		[[nodiscard]] auto current() const -> const buffer&;

	private:
		static constexpr std::uint32_t version_count = max_frames_in_flight + 1;

		struct version {
			buffer staging;
			std::uint64_t written_frame = 0;
			bool written = false;
		};

		std::array<version, version_count> m_versions;
		const frame* m_frame = nullptr;
		std::uint32_t m_cursor = 0;
		std::uint32_t m_current = 0;
	};
}

gse::gpu::readback_channel::readback_channel(device& dev, const frame& frame, const std::size_t size, const std::string_view tag) : m_frame(&frame) {
	for (auto& v : m_versions) {
		v.staging = dev.create_buffer(
			{
				.size = size,
				.usage = { buffer_flag::storage, buffer_flag::transfer_dst },
				.readback = true
			},
			tag
		);
		v.staging.host_zero();
		v.staging.clear_host_dirty();
	}
}

auto gse::gpu::readback_channel::publish_target(const std::uint64_t generation, const std::size_t bytes) -> const buffer& {
	assert(generation > 0, "readback channel generations start at 1");
	auto& v = m_versions[m_cursor];
	assert(bytes <= v.staging.size(), "readback publish of {} bytes exceeds channel capacity {}", bytes, v.staging.size());
	m_cursor = (m_cursor + 1) % version_count;
	v.generation = generation;
	v.recorded_frame = m_frame->frame_count();
	v.recorded_bytes = bytes;
	v.recorded = true;
	return v.staging;
}

auto gse::gpu::readback_channel::latest() const -> readback_view {
	const version* newest = nullptr;
	for (const auto& v : m_versions) {
		if (!v.recorded || m_frame->frame_count() < v.recorded_frame + max_frames_in_flight) {
			continue;
		}
		if (!newest || v.generation > newest->generation) {
			newest = &v;
		}
	}
	if (!newest) {
		return {};
	}
	return {
		.bytes = newest->staging.host_read().first(newest->recorded_bytes),
		.generation = newest->generation,
	};
}

gse::gpu::upload_channel::upload_channel(device& dev, const frame& frame, const buffer_desc& desc, const std::string_view tag) : m_frame(&frame) {
	for (auto& v : m_versions) {
		v.staging = dev.create_buffer(desc, tag);
		v.staging.host_zero();
		v.staging.clear_host_dirty();
	}
}

auto gse::gpu::upload_channel::write_target() -> const buffer& {
	const auto& blocking = m_versions[(m_cursor + 1) % version_count];
	assert(
		!blocking.written || m_frame->frame_count() >= blocking.written_frame + max_frames_in_flight,
		"upload channel writes are outpacing frame retirement"
	);
	auto& v = m_versions[m_cursor];
	m_current = m_cursor;
	m_cursor = (m_cursor + 1) % version_count;
	v.written_frame = m_frame->frame_count();
	v.written = true;
	return v.staging;
}

auto gse::gpu::upload_channel::current() const -> const buffer& {
	return m_versions[m_current].staging;
}
