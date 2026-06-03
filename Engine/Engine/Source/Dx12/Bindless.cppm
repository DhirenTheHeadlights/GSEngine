export module gse.dx12:bindless;

import std;

import gse.gpu_types;
import gse.core;

export namespace gse::dx12 {
	struct descriptor_heap_properties {
		gpu::device_size resource_descriptor_size = 0;
		gpu::device_size sampler_descriptor_size = 0;
	};

	class bindless_resource_heap final : public non_copyable, non_movable {
	public:
		bindless_resource_heap() {}
		~bindless_resource_heap() = default;

		auto begin_frame() -> void;
	};

	class bindless_sampler_heap final : public non_copyable, non_movable {
	public:
		bindless_sampler_heap() {}
		~bindless_sampler_heap() = default;

		auto begin_frame() -> void;
	};

	class bindless_heaps final : public non_copyable, non_movable {
	public:
		bindless_heaps() {}
		~bindless_heaps() = default;

		auto begin_frame() -> void;

		[[nodiscard]] auto resource_heap(
			this auto& self
		) -> auto&;

		[[nodiscard]] auto sampler_heap(
			this auto& self
		) -> auto&;

		[[nodiscard]] auto properties() const -> const descriptor_heap_properties&;

	private:
		descriptor_heap_properties m_props;
		bindless_resource_heap m_resource_heap;
		bindless_sampler_heap m_sampler_heap;
	};
}

auto gse::dx12::bindless_resource_heap::begin_frame() -> void {
}

auto gse::dx12::bindless_sampler_heap::begin_frame() -> void {
}

auto gse::dx12::bindless_heaps::begin_frame() -> void {
}

auto gse::dx12::bindless_heaps::resource_heap(this auto& self) -> auto& {
	return self.m_resource_heap;
}

auto gse::dx12::bindless_heaps::sampler_heap(this auto& self) -> auto& {
	return self.m_sampler_heap;
}

auto gse::dx12::bindless_heaps::properties() const -> const descriptor_heap_properties& {
	return m_props;
}
