export module gse.dx12:accel;

import std;

import gse.gpu_types;
import gse.core;

import :resources;

export namespace gse::dx12 {
	class blas final : public non_copyable {
	public:
		blas() {}
		~blas() = default;

		blas(
			blas&&
		) noexcept = default;

		auto operator=(
			blas&&
		) noexcept -> blas& = default;

		[[nodiscard]] auto handle() const -> gpu::acceleration_structure;

		[[nodiscard]] auto valid() const -> bool;

	private:
		gpu::acceleration_structure m_handle;
	};

	class tlas final : public non_copyable {
	public:
		tlas() {}
		~tlas() = default;

		tlas(
			tlas&&
		) noexcept = default;

		auto operator=(
			tlas&&
		) noexcept -> tlas& = default;

		[[nodiscard]] auto handle() const -> gpu::acceleration_structure;

		[[nodiscard]] auto instance_buffer(
			this auto& self
		) -> auto&;

		[[nodiscard]] auto scratch_buffer(
			this auto& self
		) -> auto&;

		[[nodiscard]] auto valid() const -> bool;

	private:
		gpu::acceleration_structure m_handle;
		buffer m_instance_buffer;
		buffer m_scratch_buffer;
	};
}

auto gse::dx12::blas::handle() const -> gpu::acceleration_structure {
	return m_handle;
}

auto gse::dx12::blas::valid() const -> bool {
	return false;
}

auto gse::dx12::tlas::handle() const -> gpu::acceleration_structure {
	return m_handle;
}

auto gse::dx12::tlas::instance_buffer(this auto& self) -> auto& {
	return self.m_instance_buffer;
}

auto gse::dx12::tlas::scratch_buffer(this auto& self) -> auto& {
	return self.m_scratch_buffer;
}

auto gse::dx12::tlas::valid() const -> bool {
	return false;
}
