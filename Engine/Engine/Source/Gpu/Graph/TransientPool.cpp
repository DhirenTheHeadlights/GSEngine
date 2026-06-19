module gse.gpu:transient_pool_impl;

import std;

import :transient_pool;
import :device;

import gse.assert;
import gse.concurrency;
import gse.core;
import gse.diag;
import gse.log;

namespace gse::gpu {
	auto allocate_transient_key() -> std::uint64_t;

	auto aspect_for_format(
		image_format fmt
	) -> image_aspect_flags;

	auto depth_for_format(
		image_format fmt
	) -> bool;

	auto image_create_info_from(
		const transient_image_desc& desc
	) -> image_create_info;

	auto image_view_create_info_from(
		const transient_image_desc& desc
	) -> image_view_create_info;

	struct lifetime_entry {
		std::uint32_t first_pass = 0;
		std::uint32_t last_pass = 0;
		bool is_image = false;
		std::size_t request_index = 0;
	};

	auto compute_lifetime(
		const std::vector<id>& used_by,
		std::span<const id> pass_kind_order
	) -> std::
		pair<std::uint32_t, std::uint32_t>;

	auto greedy_color(
		std::span<const lifetime_entry> intervals
	) -> std::vector<std::uint32_t>;
}

auto gse::gpu::allocate_transient_key() -> std::uint64_t {
	static std::atomic<std::uint64_t> counter{ 1 };
	return counter.fetch_add(1, std::memory_order_relaxed);
}

auto gse::gpu::transient_image(const gse::context& ctx, transient_image_desc desc) -> transient_image_handle {
	const transient_image_handle h{
		.key = allocate_transient_key()
	};
	ctx.channels.push<transient_image_request>({
		.handle = h,
		.desc = std::move(desc),
	});
	return h;
}

auto gse::gpu::transient_buffer(const gse::context& ctx, transient_buffer_desc desc) -> transient_buffer_handle {
	const transient_buffer_handle h{
		.key = allocate_transient_key()
	};
	ctx.channels.push<transient_buffer_request>({
		.handle = h,
		.desc = std::move(desc),
	});
	return h;
}

auto gse::gpu::aspect_for_format(const image_format fmt) -> image_aspect_flags {
	if (fmt == image_format::d32_sfloat) {
		return image_aspect_flag::depth;
	}
	return image_aspect_flag::color;
}

auto gse::gpu::depth_for_format(const image_format fmt) -> bool {
	return fmt == image_format::d32_sfloat;
}

auto gse::gpu::image_create_info_from(const transient_image_desc& desc) -> image_create_info {
	return {
		.type = image_type::e2d,
		.format = desc.format,
		.extent = vec3u{ desc.extent.x(), desc.extent.y(), 1 },
		.mip_levels = 1,
		.array_layers = 1,
		.samples = sample_count::e1,
		.usage = desc.usage,
	};
}

auto gse::gpu::image_view_create_info_from(const transient_image_desc& desc) -> image_view_create_info {
	return {
		.format = desc.format,
		.view_type = desc.view,
		.aspects = aspect_for_format(desc.format),
		.base_mip_level = 0,
		.level_count = 1,
		.base_array_layer = 0,
		.layer_count = 1,
	};
}

auto gse::gpu::compute_lifetime(const std::vector<id>& used_by, const std::span<const id> pass_kind_order) -> std::pair<std::uint32_t, std::uint32_t> {
	if (pass_kind_order.empty()) {
		return { 0, 0 };
	}
	if (used_by.empty()) {
		return { 0, static_cast<std::uint32_t>(pass_kind_order.size() - 1) };
	}

	std::uint32_t first = std::numeric_limits<std::uint32_t>::max();
	std::uint32_t last = 0;
	bool any = false;

	for (std::uint32_t i = 0; i < pass_kind_order.size(); ++i) {
		for (const auto& k : used_by) {
			if (pass_kind_order[i] == k) {
				first = std::min(first, i);
				last = std::max(last, i);
				any = true;
				break;
			}
		}
	}

	if (!any) {
		return { 0, static_cast<std::uint32_t>(pass_kind_order.size() - 1) };
	}
	return { first, last };
}

auto gse::gpu::greedy_color(const std::span<const lifetime_entry> intervals) -> std::vector<std::uint32_t> {
	std::vector<std::uint32_t> colors(intervals.size(), std::numeric_limits<std::uint32_t>::max());

	std::vector<std::size_t> order(intervals.size());
	std::iota(order.begin(), order.end(), 0);
	std::ranges::sort(
		order,
		[&](const std::size_t a, const std::size_t b) {
			return intervals[a].first_pass < intervals[b].first_pass;
		}
	);

	struct color_state {
		std::uint32_t last_end = 0;
		bool used = false;
	};
	std::vector<color_state> active;

	for (const auto idx : order) {
		const auto& iv = intervals[idx];
		std::uint32_t chosen = std::numeric_limits<std::uint32_t>::max();

		for (std::uint32_t c = 0; c < active.size(); ++c) {
			if (!active[c].used) {
				continue;
			}
			if (active[c].last_end < iv.first_pass) {
				chosen = c;
				break;
			}
		}

		if (chosen == std::numeric_limits<std::uint32_t>::max()) {
			chosen = static_cast<std::uint32_t>(active.size());
			active.push_back({});
		}

		active[chosen].last_end = iv.last_pass;
		active[chosen].used = true;
		colors[idx] = chosen;
	}

	return colors;
}

gse::gpu::transient_pool::transient_pool(gpu::device& dev) : m_device(std::addressof(dev)) {
}

gse::gpu::transient_pool::~transient_pool() {
	if (!m_device) {
		return;
	}
	for (auto& slot : m_slots.resources()) {
		free_slot_resources(slot);
		free_slot_memory(slot);
	}
}

gse::gpu::transient_pool::transient_pool(transient_pool&& other) noexcept
	: m_device(other.m_device), m_slots(std::move(other.m_slots)), m_current_frame(other.m_current_frame) {
	other.m_device = nullptr;
}

auto gse::gpu::transient_pool::operator=(transient_pool&& other) noexcept -> transient_pool& {
	if (this != &other) {
		if (m_device) {
			for (auto& slot : m_slots.resources()) {
				free_slot_resources(slot);
				free_slot_memory(slot);
			}
		}
		m_device = other.m_device;
		m_slots = std::move(other.m_slots);
		m_current_frame = other.m_current_frame;
		other.m_device = nullptr;
	}
	return *this;
}

auto gse::gpu::transient_pool::free_slot_resources(frame_state& slot) -> void {
	slot.images.clear();
	slot.buffers.clear();
}

auto gse::gpu::transient_pool::free_slot_memory(frame_state& slot) -> void {
	for (auto& block : slot.blocks) {
		if (block.memory) {
			m_device->free_aliased_memory(block.memory);
		}
	}
	slot.blocks.clear();
}

auto gse::gpu::transient_pool::reset() -> void {
	for (auto& slot : m_slots.resources()) {
		free_slot_resources(slot);
	}
}

auto gse::gpu::transient_pool::resolve_image(const transient_image_handle h) const -> const image* {
	if (!h) {
		return nullptr;
	}
	const auto& slot = m_slots[m_current_frame];
	const auto it = slot.images.find(h.key);
	if (it == slot.images.end()) {
		return nullptr;
	}
	return it->second.resource.get();
}

auto gse::gpu::transient_pool::resolve_buffer(const transient_buffer_handle h) const -> const buffer* {
	if (!h) {
		return nullptr;
	}
	const auto& slot = m_slots[m_current_frame];
	const auto it = slot.buffers.find(h.key);
	if (it == slot.buffers.end()) {
		return nullptr;
	}
	return it->second.resource.get();
}

auto gse::gpu::transient_pool::transient_images() const -> std::vector<transient_image_info> {
	const auto& slot = m_slots[m_current_frame];
	std::vector<transient_image_info> out;
	out.reserve(slot.images.size());
	for (const auto& [_, alloc] : slot.images) {
		out.push_back({
			.resource = alloc.resource.get(),
			.aspects = alloc.aspects,
			.format = alloc.format,
		});
	}
	return out;
}

auto gse::gpu::transient_pool::ensure_block_for_color(frame_state& slot, const std::uint32_t color, const gpu::device_size required_size, const std::uint32_t memory_type_mask) -> void {
	while (slot.blocks.size() <= color) {
		slot.blocks.push_back({});
	}

	auto& block = slot.blocks[color];

	const bool type_compatible = block.memory && (memory_type_mask & (1u << block.memory_type_index)) != 0;
	const bool size_ok = block.memory && block.size >= required_size;

	if (type_compatible && size_ok) {
		return;
	}

	if (block.memory) {
		m_device->free_aliased_memory(block.memory);
		block = {};
	}

	const auto type_index = m_device->find_memory_type_index(memory_type_mask, memory_property_flag::device_local);
	block.memory = m_device->allocate_aliased_memory(required_size, type_index);
	block.size = required_size;
	block.memory_type_index = type_index;
}

auto gse::gpu::transient_pool::plan(const std::uint32_t frame_idx, const std::span<const transient_image_request> image_requests, const std::span<const transient_buffer_request> buffer_requests, const std::span<const id> pass_kind_order) -> void {
	m_current_frame = frame_idx;
	auto& slot = m_slots[frame_idx];
	free_slot_resources(slot);

	if (image_requests.empty() && buffer_requests.empty()) {
		return;
	}

	std::vector<lifetime_entry> intervals;
	intervals.reserve(image_requests.size() + buffer_requests.size());

	for (std::size_t i = 0; i < image_requests.size(); ++i) {
		const auto [first, last] = compute_lifetime(image_requests[i].desc.used_by, pass_kind_order);
		intervals.push_back({
			.first_pass = first,
			.last_pass = last,
			.is_image = true,
			.request_index = i,
		});
	}
	for (std::size_t i = 0; i < buffer_requests.size(); ++i) {
		const auto [first, last] = compute_lifetime(buffer_requests[i].desc.used_by, pass_kind_order);
		intervals.push_back({
			.first_pass = first,
			.last_pass = last,
			.is_image = false,
			.request_index = i,
		});
	}

	const auto colors = greedy_color(intervals);

	struct color_aggregate {
		gpu::device_size size = 0;
		std::uint32_t type_mask = std::numeric_limits<std::uint32_t>::max();
	};
	std::vector<color_aggregate> aggregates;

	struct staged_image {
		gpu::handle<gpu::image> handle;
		std::uint32_t color = 0;
		std::size_t entry_index = 0;
	};
	struct staged_buffer {
		gpu::handle<gpu::buffer> handle;
		std::uint32_t color = 0;
		std::size_t entry_index = 0;
	};

	std::vector<staged_image> staged_images;
	std::vector<staged_buffer> staged_buffers;

	for (std::size_t e = 0; e < intervals.size(); ++e) {
		const auto color = colors[e];
		while (aggregates.size() <= color) {
			aggregates.push_back({});
		}

		if (intervals[e].is_image) {
			const auto& req = image_requests[intervals[e].request_index];
			const auto [img_handle, reqs] = m_device->create_image_unbound(image_create_info_from(req.desc));

			aggregates[color].size = std::max(aggregates[color].size, reqs.size);
			aggregates[color].type_mask &= reqs.memory_type_bits;

			staged_images.push_back({
				.handle = img_handle,
				.color = color,
				.entry_index = e,
			});
		}
		else {
			const auto& req = buffer_requests[intervals[e].request_index];
			const auto [buf_handle, reqs] = m_device->create_buffer_unbound({
				.size = req.desc.size,
				.usage = req.desc.usage,
			});

			aggregates[color].size = std::max(aggregates[color].size, reqs.size);
			aggregates[color].type_mask &= reqs.memory_type_bits;

			staged_buffers.push_back({
				.handle = buf_handle,
				.color = color,
				.entry_index = e,
			});
		}
	}

	for (std::uint32_t c = 0; c < aggregates.size(); ++c) {
		assert(
			aggregates[c].type_mask != 0,
			"transient_pool: alias color {} has no compatible memory type across its resources",
			c
		);
		ensure_block_for_color(slot, c, aggregates[c].size, aggregates[c].type_mask);
	}

	for (const auto& si : staged_images) {
		const auto& req = image_requests[intervals[si.entry_index].request_index];

		m_device->bind_image_memory(si.handle, slot.blocks[si.color].memory, 0);
		const auto view_info = image_view_create_info_from(req.desc);
		const auto view_handle = m_device->create_image_view(si.handle, view_info);

		auto img = m_device->make_aliased_image(
			si.handle,
			view_handle,
			req.desc.format,
			vec3u{ req.desc.extent.x(), req.desc.extent.y(), 1 },
			view_info,
			req.desc.tag
		);

		slot.images.emplace(
			req.handle.key,
			transient_image_allocation{
				.resource = std::move(img),
				.aspects = aspect_for_format(req.desc.format),
				.format = req.desc.format,
				.color = si.color,
				.first_pass = intervals[si.entry_index].first_pass,
				.last_pass = intervals[si.entry_index].last_pass,
			}
		);
	}

	for (const auto& sb : staged_buffers) {
		const auto& req = buffer_requests[intervals[sb.entry_index].request_index];

		m_device->bind_buffer_memory(sb.handle, slot.blocks[sb.color].memory, 0);

		auto buf = m_device->make_aliased_buffer(sb.handle, req.desc.size, req.desc.tag);

		slot.buffers.emplace(
			req.handle.key,
			transient_buffer_allocation{
				.resource = std::move(buf),
				.color = sb.color,
				.first_pass = intervals[sb.entry_index].first_pass,
				.last_pass = intervals[sb.entry_index].last_pass,
			}
		);
	}
}
