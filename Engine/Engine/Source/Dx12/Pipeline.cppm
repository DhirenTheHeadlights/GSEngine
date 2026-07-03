export module gse.dx12:pipeline;

import std;

import gse.gpu_backend;
import gse.directx;

export namespace gse::dx12 {
	class pipeline_layout final {
	public:
		pipeline_layout() = default;

		explicit pipeline_layout(
			directx::com_ptr<directx::ID3D12RootSignature> root_signature
		);

		[[nodiscard]] auto root_signature() const -> directx::ID3D12RootSignature*;

		[[nodiscard]] auto valid() const -> bool;

	private:
		directx::com_ptr<directx::ID3D12RootSignature> m_root_signature;
	};

	[[nodiscard]] auto create_bindless_pipeline_layout(
		directx::ID3D12Device* device
	) -> pipeline_layout;
}

gse::dx12::pipeline_layout::pipeline_layout(directx::com_ptr<directx::ID3D12RootSignature> root_signature)
	: m_root_signature(std::move(root_signature)) {
}

auto gse::dx12::pipeline_layout::root_signature() const -> directx::ID3D12RootSignature* {
	return m_root_signature.get();
}

auto gse::dx12::pipeline_layout::valid() const -> bool {
	return static_cast<bool>(m_root_signature);
}

auto gse::dx12::create_bindless_pipeline_layout(directx::ID3D12Device* device) -> pipeline_layout {
	constexpr std::uint32_t root_constant_count = 64;
	return pipeline_layout(directx::create_bindless_root_signature(device, root_constant_count));
}
