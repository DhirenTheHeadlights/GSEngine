export module gse.gpu:spirv_reflect;

import std;

import :types;

import gse.assert;

export namespace gse::gpu {
	auto used_bindings(
		std::span<const std::uint32_t> spirv
	) -> std::vector<binding_use>;
}

namespace gse::gpu {
	constexpr std::uint32_t spirv_magic = 0x07230203;

	enum spirv_op : std::uint16_t {
		op_function_call = 54,
		op_variable = 59,
		op_load = 61,
		op_store = 62,
		op_copy_memory = 63,
		op_access_chain = 65,
		op_in_bounds_access_chain = 66,
		op_ptr_access_chain = 67,
		op_in_bounds_ptr_access_chain = 70,
		op_decorate = 71,
		op_copy_object = 83,
		op_sampled_image = 86,
		op_image_sample_implicit_lod = 87,
		op_image_sample_explicit_lod = 88,
		op_image_sample_dref_implicit_lod = 89,
		op_image_sample_dref_explicit_lod = 90,
		op_image_sample_proj_implicit_lod = 91,
		op_image_sample_proj_explicit_lod = 92,
		op_image_sample_proj_dref_implicit_lod = 93,
		op_image_sample_proj_dref_explicit_lod = 94,
		op_image_fetch = 95,
		op_image_read = 98,
		op_image_write = 99,
		op_atomic_load = 227,
		op_atomic_store = 228,
		op_atomic_exchange = 229,
		op_atomic_compare_exchange = 230,
		op_atomic_i_increment = 232,
		op_atomic_i_decrement = 233,
		op_atomic_i_add = 234,
		op_atomic_i_sub = 235,
		op_atomic_s_min = 236,
		op_atomic_u_min = 237,
		op_atomic_s_max = 238,
		op_atomic_u_max = 239,
		op_atomic_and = 240,
		op_atomic_or = 241,
		op_atomic_xor = 242,
	};

	constexpr std::uint32_t decoration_binding = 33;
	constexpr std::uint32_t decoration_descriptor_set = 34;

	struct decoration_info {
		std::uint32_t set = 0;
		std::uint32_t slot = 0;
		bool has_set = false;
		bool has_slot = false;
	};

	auto opcode_writes_pointer(std::uint16_t op) -> bool {
		switch (op) {
			case op_store:
			case op_atomic_store:
			case op_atomic_exchange:
			case op_atomic_compare_exchange:
			case op_atomic_i_increment:
			case op_atomic_i_decrement:
			case op_atomic_i_add:
			case op_atomic_i_sub:
			case op_atomic_s_min:
			case op_atomic_u_min:
			case op_atomic_s_max:
			case op_atomic_u_max:
			case op_atomic_and:
			case op_atomic_or:
			case op_atomic_xor:
				return true;
			default:
				return false;
		}
	}
}

auto gse::gpu::used_bindings(const std::span<const std::uint32_t> spirv) -> std::vector<binding_use> {
	if (spirv.size() < 5 || spirv[0] != spirv_magic) {
		return {};
	}

	std::unordered_map<std::uint32_t, decoration_info> decorations;
	std::unordered_map<std::uint32_t, std::uint32_t> ptr_to_root;

	std::size_t i = 5;
	while (i < spirv.size()) {
		const std::uint32_t word = spirv[i];
		const std::uint16_t op = static_cast<std::uint16_t>(word & 0xFFFFu);
		const std::uint16_t wcount = static_cast<std::uint16_t>((word >> 16) & 0xFFFFu);
		assert(wcount > 0 && i + wcount <= spirv.size(), "Malformed SPIR-V: bad instruction word count");

		if (op == op_decorate && wcount >= 4) {
			const std::uint32_t target = spirv[i + 1];
			const std::uint32_t deco = spirv[i + 2];
			if (deco == decoration_descriptor_set) {
				auto& info = decorations[target];
				info.set = spirv[i + 3];
				info.has_set = true;
			}
			else if (deco == decoration_binding) {
				auto& info = decorations[target];
				info.slot = spirv[i + 3];
				info.has_slot = true;
			}
		}
		else if (op == op_variable && wcount >= 4) {
			const std::uint32_t result_id = spirv[i + 2];
			const auto it = decorations.find(result_id);
			if (it != decorations.end() && it->second.has_set && it->second.has_slot) {
				ptr_to_root[result_id] = result_id;
			}
		}

		i += wcount;
	}

	if (ptr_to_root.empty()) {
		return {};
	}

	bool changed = true;
	while (changed) {
		changed = false;
		i = 5;
		while (i < spirv.size()) {
			const std::uint32_t word = spirv[i];
			const std::uint16_t op = static_cast<std::uint16_t>(word & 0xFFFFu);
			const std::uint16_t wcount = static_cast<std::uint16_t>((word >> 16) & 0xFFFFu);

			std::uint32_t result_id = 0;
			std::uint32_t base = 0;
			bool propagate = false;

			if ((op == op_access_chain || op == op_in_bounds_access_chain || op == op_ptr_access_chain || op == op_in_bounds_ptr_access_chain) && wcount >= 4) {
				result_id = spirv[i + 2];
				base = spirv[i + 3];
				propagate = true;
			}
			else if (op == op_copy_object && wcount >= 4) {
				result_id = spirv[i + 2];
				base = spirv[i + 3];
				propagate = true;
			}

			if (propagate) {
				const auto base_it = ptr_to_root.find(base);
				if (base_it != ptr_to_root.end() && !ptr_to_root.contains(result_id)) {
					ptr_to_root[result_id] = base_it->second;
					changed = true;
				}
			}

			i += wcount;
		}
	}

	std::unordered_map<std::uint32_t, descriptor_access> root_access;

	auto record = [&](const std::uint32_t ptr_id, const descriptor_access access) {
		const auto it = ptr_to_root.find(ptr_id);
		if (it == ptr_to_root.end()) {
			return;
		}
		const auto [entry, inserted] = root_access.try_emplace(it->second, access);
		if (!inserted && access == descriptor_access::read_write) {
			entry->second = descriptor_access::read_write;
		}
	};

	i = 5;
	while (i < spirv.size()) {
		const std::uint32_t word = spirv[i];
		const std::uint16_t op = static_cast<std::uint16_t>(word & 0xFFFFu);
		const std::uint16_t wcount = static_cast<std::uint16_t>((word >> 16) & 0xFFFFu);

		if (op == op_load && wcount >= 4) {
			record(spirv[i + 3], descriptor_access::read);
		}
		else if (op == op_store && wcount >= 3) {
			record(spirv[i + 1], descriptor_access::read_write);
		}
		else if (op == op_atomic_load && wcount >= 6) {
			record(spirv[i + 3], descriptor_access::read);
		}
		else if (opcode_writes_pointer(op) && op != op_store) {
			const std::uint32_t ptr_operand = (op == op_atomic_store) ? spirv[i + 1] : spirv[i + 3];
			record(ptr_operand, descriptor_access::read_write);
		}
		else if (op == op_image_write && wcount >= 4) {
			record(spirv[i + 1], descriptor_access::read_write);
		}
		else if ((op == op_image_read || op == op_image_fetch
			|| op == op_image_sample_implicit_lod || op == op_image_sample_explicit_lod
			|| op == op_image_sample_dref_implicit_lod || op == op_image_sample_dref_explicit_lod
			|| op == op_image_sample_proj_implicit_lod || op == op_image_sample_proj_explicit_lod
			|| op == op_image_sample_proj_dref_implicit_lod || op == op_image_sample_proj_dref_explicit_lod)
			&& wcount >= 4) {
			record(spirv[i + 3], descriptor_access::read);
		}
		else if (op == op_sampled_image && wcount >= 5) {
			record(spirv[i + 3], descriptor_access::read);
			record(spirv[i + 4], descriptor_access::read);
		}
		else if (op == op_copy_memory && wcount >= 3) {
			record(spirv[i + 1], descriptor_access::read_write);
			record(spirv[i + 2], descriptor_access::read);
		}
		else if (op == op_function_call && wcount >= 4) {
			for (std::uint16_t arg = 4; arg < wcount; ++arg) {
				record(spirv[i + arg], descriptor_access::read_write);
			}
		}

		i += wcount;
	}

	std::vector<binding_use> result;
	result.reserve(root_access.size());
	for (const auto& [root, access] : root_access) {
		const auto deco_it = decorations.find(root);
		if (deco_it == decorations.end()) {
			continue;
		}
		result.push_back({
			.set = deco_it->second.set,
			.slot = deco_it->second.slot,
			.access = access,
		});
	}

	std::ranges::sort(result, [](const binding_use& a, const binding_use& b) {
		if (a.set != b.set) {
			return a.set < b.set;
		}
		return a.slot < b.slot;
	});

	return result;
}
