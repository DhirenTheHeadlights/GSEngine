export module gse.dx12:pipeline;

import std;

import gse.assert;
import gse.gpu_backend;
import gse.directx;

export namespace gse::dx12 {
	constexpr std::uint32_t bindless_root_constant_count = 64;

	struct root_layout {
		std::uint32_t push_bytes = 0;
		std::uint32_t binding_bytes = 0;

		auto operator<=>(const root_layout&) const = default;
	};

	struct root_signature_entry {
		directx::ID3D12RootSignature* signature = nullptr;
		bool bindings_in_cbv = false;
	};

	class root_signature_cache final {
	public:
		[[nodiscard]] auto acquire(
			directx::ID3D12Device* device,
			root_layout layout
		) -> root_signature_entry;

	private:
		std::map<root_layout, directx::com_ptr<directx::ID3D12RootSignature>> m_signatures;
	};
}

auto gse::dx12::root_signature_cache::acquire(directx::ID3D12Device* device, const root_layout layout) -> root_signature_entry {
	constexpr std::uint32_t root_cbv_dwords = 2;
	const auto push_dwords = (layout.push_bytes + 3) / 4;
	const auto binding_dwords = (layout.binding_bytes + 3) / 4;
	const auto round_to_vec4 = [](const std::uint32_t dwords) {
		return std::max((dwords + 3) & ~3u, 4u);
	};
	const auto padded_push = round_to_vec4(push_dwords);
	const auto padded_bindings = round_to_vec4(binding_dwords);
	const bool bindings_in_cbv = padded_push + padded_bindings > bindless_root_constant_count;
	assert(
		padded_push + root_cbv_dwords <= bindless_root_constant_count,
		"dx12: push constants ({} B, {} B once padded to a vec4 boundary) exceed the {} B root signature budget "
		"even with the bindless descriptor indices moved to a root constant buffer. Move the largest push-constant "
		"member into a storage buffer binding.",
		layout.push_bytes,
		padded_push * sizeof(std::uint32_t),
		bindless_root_constant_count * sizeof(std::uint32_t)
	);
	auto& slot = m_signatures[layout];
	if (!slot) {
		slot = directx::create_bindless_root_signature(device, padded_push, padded_bindings, bindings_in_cbv);
	}
	return {
		.signature = slot.get(),
		.bindings_in_cbv = bindings_in_cbv,
	};
}
