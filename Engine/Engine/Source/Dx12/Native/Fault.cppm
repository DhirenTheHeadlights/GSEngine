export module gse.dx12:fault;

import gse.gpu_backend;

export namespace gse::dx12 {
	class fault {
	public:
		auto wait_for_crash_dump() -> void;

		[[nodiscard]] auto enabled() const -> bool;

		[[nodiscard]] auto vendor_binary_enabled() const -> bool;

		[[nodiscard]] auto query_counts(
			gpu::device_fault_counts& counts
		) const -> gpu::result;

		[[nodiscard]] auto query_info(
			gpu::device_fault_counts& counts,
			gpu::device_fault_info& info
		) const -> gpu::result;
	};
}

auto gse::dx12::fault::wait_for_crash_dump() -> void {}

auto gse::dx12::fault::enabled() const -> bool {
	return false;
}

auto gse::dx12::fault::vendor_binary_enabled() const -> bool {
	return false;
}

auto gse::dx12::fault::query_counts(gpu::device_fault_counts&) const -> gpu::result {
	return gpu::result::success;
}

auto gse::dx12::fault::query_info(gpu::device_fault_counts&, gpu::device_fault_info&) const -> gpu::result {
	return gpu::result::success;
}
