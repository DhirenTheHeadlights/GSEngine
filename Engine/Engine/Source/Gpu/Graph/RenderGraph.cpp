module gse.gpu:render_graph_impl;

import std;

import :render_graph;
import :device;
import :swap_chain;
import :frame;
import :transient_pool;
import :image;
import :pass_recorder;

import gse.gpu_backend;
import gse.assert;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.log;
import gse.math;
import gse.meta;

namespace gse::gpu {
	constexpr auto profile_stats_flags = gpu::pipeline_statistic_flag::input_assembly_vertices |
		gpu::pipeline_statistic_flag::input_assembly_primitives | gpu::pipeline_statistic_flag::clipping_invocations |
		gpu::pipeline_statistic_flag::fragment_shader_invocations;
}

gse::gpu::render_graph::render_graph(gpu::device& device, gpu::swap_chain* swapchain, gpu::frame& frame)
	: m_device(std::addressof(device)), m_swapchain(swapchain), m_frame(std::addressof(frame)), m_transient_pool(device) {
	m_timestamp_period_per_tick = nanoseconds(static_cast<double>(device.timestamp_period()));
	for (auto& q : m_queue_states) {
		q.timeline = gpu::queue_timeline<gpu::device>::create(device);
	}
	if (m_swapchain) {
		m_swapchain->on_recreate([this] {
			recreate_framebuffer_images();
		});
	}
}

auto gse::gpu::render_graph::create_framebuffer_image(const framebuffer_image_desc& desc, const std::string_view tag) -> image {
	const auto ext = m_swapchain->extent();
	if (ext.x() == 0 || ext.y() == 0) {
		return {};
	}
	auto img = m_device->create_image(
		gpu::image_desc{
			.size = ext,
			.format = desc.format,
			.usage = desc.usage,
		},
		tag
	);
	gpu::transition_image_to(*m_device, img);
	return img;
}

auto gse::gpu::render_graph::recreate_framebuffer_images() -> void {
	for (auto& [name, slot] : m_framebuffer_images) {
		slot->img = create_framebuffer_image(slot->desc, name.tag());
	}
}

auto gse::gpu::render_graph::register_framebuffer_image(const id name, const framebuffer_image_desc& desc) -> const image& {
	auto& slot = m_framebuffer_images[name];
	if (!slot) {
		slot = std::make_unique<registered_image>(registered_image{
			.desc = desc,
			.img = create_framebuffer_image(desc, name.tag()),
		});
	}
	return slot->img;
}

auto gse::gpu::render_graph::take_aux_submissions() -> std::vector<gpu::queue_submission> {
	return std::move(m_pending_aux_submissions);
}

auto gse::gpu::render_graph::take_graphics_extra_waits() -> std::vector<gpu::semaphore_submit_info> {
	return std::move(m_pending_graphics_extra_waits);
}

auto gse::gpu::render_graph::take_graphics_buffers() -> std::vector<gpu::command_buffer_handle> {
	return std::move(m_pending_graphics_buffers);
}

auto gse::gpu::render_graph::set_gpu_timestamps_enabled(const bool enabled) -> void {
	m_gpu_timestamps_enabled.store(enabled, std::memory_order_relaxed);
}

auto gse::gpu::render_graph::set_gpu_pipeline_stats_enabled(const bool enabled) -> void {
	m_gpu_pipeline_stats_enabled.store(enabled, std::memory_order_relaxed);
}

auto gse::gpu::render_graph::set_swapchain_clear(const gpu::color_clear value, const load_op op) -> void {
	m_swapchain_clear = value;
	m_swapchain_load = op;
}

auto gse::gpu::render_graph::ensure_profile_pools(gpu_profile_slot& slot, const bool allow_stats) const -> void {
	if (!slot.timestamp_pool) {
		slot.timestamp_pool = m_device->create_timestamp_query_pool(max_profiled_passes * 2 + 1);
	}
	if (allow_stats && !slot.stats_pool) {
		slot.stats_pool = m_device->create_pipeline_stats_query_pool(max_profiled_passes, profile_stats_flags);
	}
}

auto gse::gpu::render_graph::read_profile_slot(gpu_profile_slot& slot) -> void {
	if (!slot.results_valid || slot.pass_count == 0) {
		return;
	}

	const std::uint32_t timestamp_count = slot.pass_count * 2 + 1;
	const auto [ts_status, timestamps] =
		m_device->query_pool_results(slot.timestamp_pool, 0, timestamp_count, sizeof(std::uint64_t));

	if (ts_status != gpu::query_status::success) {
		slot.results_valid = false;
		return;
	}

	const auto period = m_timestamp_period_per_tick;
	const auto gpu_ref = static_cast<double>(timestamps[0]) * period;
	const auto offset = time_t<double>(slot.cpu_ref) - gpu_ref;

	for (std::uint32_t i = 0; i < slot.pass_count; ++i) {
		const auto start = static_cast<double>(timestamps[1 + i * 2]) * period + offset;
		const auto end = static_cast<double>(timestamps[2 + i * 2]) * period + offset;
		const auto gpu_id = slot.pass_types[i];
		const auto queue = slot.pass_queues[i];
		const std::uint64_t key = (slot.frame_counter << 16) | (static_cast<std::uint64_t>(queue) << 14) | i;
		const auto tid = (queue == gpu::queue_type::compute) ? trace::gpu_compute_virtual_tid : trace::gpu_virtual_tid;

		trace::begin_async_at(gpu_id, key, tid, time_t<std::uint64_t>(start));
		trace::end_async_at(gpu_id, key, tid, time_t<std::uint64_t>(end));

		profile::ingest_gpu_sample(gpu_id, end - start);
	}

	if (slot.stats_issued) {
		const auto [stats_status, stats] =
			m_device->query_pool_results(slot.stats_pool, 0, slot.pass_count, sizeof(std::uint64_t) * stats_per_pass);

		if (stats_status == gpu::query_status::success) {
			static constexpr std::array<const char*, stats_per_pass> labels{ ":ia_verts",
																			 ":ia_prims",
																			 ":clip_invocs",
																			 ":fs_invocs" };
			for (std::uint32_t i = 0; i < slot.pass_count; ++i) {
				const auto start = static_cast<double>(timestamps[1 + i * 2]) * period + offset;
				auto cache_it = m_stat_ids.find(slot.pass_types[i]);
				if (cache_it == m_stat_ids.end()) {
					const auto pass_name = std::string(slot.pass_types[i].tag());
					std::array<id, stats_per_pass> ids;
					for (std::uint32_t s = 0; s < stats_per_pass; ++s) {
						ids[s] = find_or_generate_id(pass_name + labels[s]);
					}
					cache_it = m_stat_ids.emplace(slot.pass_types[i], ids).first;
				}
				const auto& stat_ids = cache_it->second;
				for (std::uint32_t s = 0; s < stats_per_pass; ++s) {
					trace::counter_at(
						stat_ids[s],
						static_cast<double>(stats[i * stats_per_pass + s]),
						trace::gpu_stats_virtual_tid,
						time_t<std::uint64_t>(start)
					);
				}
			}
		}
	}

	slot.results_valid = false;
}

auto gse::gpu::render_graph::current_frame() const -> std::uint32_t {
	return m_frame->current_frame();
}

auto gse::gpu::render_graph::extent() const -> vec2u {
	return m_swapchain->extent();
}

auto gse::gpu::render_graph::frame_in_progress() const -> bool {
	return m_frame->frame_in_progress();
}

auto gse::gpu::render_graph::execute(frame_request_drain drain) -> void {
	m_pending_aux_submissions.clear();
	m_pending_graphics_extra_waits.clear();
	m_pending_graphics_buffers.clear();

	if (!m_frame->frame_in_progress()) {
		return;
	}

	const auto frame_idx = m_frame->current_frame();
	const auto graphics_family = m_device->queue_family(gpu::queue_type::graphics);
	std::array<bool, gpu::queue_type_count> queue_distinct{};
	for (std::size_t qi = 0; qi < gpu::queue_type_count; ++qi) {
		queue_distinct[qi] = m_device->queue_family(static_cast<gpu::queue_type>(qi)) != graphics_family;
	}
	queue_distinct[static_cast<std::size_t>(gpu::queue_type::graphics)] = true;

	auto effective_queue = [&](const gpu::queue_type requested) -> gpu::queue_type {
		return queue_distinct[static_cast<std::size_t>(requested)] ? requested : gpu::queue_type::graphics;
	};

	const auto image_index = m_frame->image_index();
	const auto swap_extent = m_swapchain ? m_swapchain->extent() : vec2u{};

	const bool timestamps_enabled = m_gpu_timestamps_enabled.load(std::memory_order_relaxed);
	const bool stats_enabled = m_gpu_pipeline_stats_enabled.load(std::memory_order_relaxed);

	if (m_frames_submitted >= per_frame_resource<gpu_profile_slot>::frames_in_flight) {
		for (auto& slots : m_profile_slots) {
			read_profile_slot(slots[frame_idx]);
		}
	}

	auto bump_frames = make_scope_exit([this] {
		++m_frames_submitted;
	});

	auto reset_slot = [](gpu_profile_slot& s) {
		s.pass_types.resize(max_profiled_passes);
		s.pass_queues.resize(max_profiled_passes);
		s.pass_count = 0;
		s.stats_issued = false;
		s.results_valid = false;
	};
	for (auto& slots : m_profile_slots) {
		reset_slot(slots[frame_idx]);
	}

	if (timestamps_enabled) {
		for (std::size_t qi = 0; qi < gpu::queue_type_count; ++qi) {
			const bool with_stats = qi == static_cast<std::size_t>(gpu::queue_type::graphics) && stats_enabled;
			ensure_profile_pools(m_profile_slots[qi][frame_idx], with_stats);
		}
	}

	m_device->reset_worker_command_pools(frame_idx);
	const auto color_format_value = m_swapchain ? gpu::format_value(m_swapchain->format()) : gpu::image_format_value{ 0 };

	auto resolve_color_target = [&](const color_output_info& info) -> const image* {
		if (info.transient_target) {
			return m_transient_pool.resolve_image(info.transient_target);
		}
		if (info.custom_target) {
			return info.custom_target;
		}
		return nullptr;
	};

	auto resolve_depth_target = [&](const depth_output_info& info) -> const image* {
		if (info.transient_target) {
			return m_transient_pool.resolve_image(info.transient_target);
		}
		if (info.custom_target) {
			return info.custom_target;
		}
		return nullptr;
	};

	auto passes = drain.drain_passes();
	auto transient_images = drain.drain_images();
	auto transient_buffers = drain.drain_buffers();

	{
		std::vector<id> pass_kind_order;
		pass_kind_order.reserve(passes.size());
		for (const auto& p : passes) {
			pass_kind_order.push_back(p.pass_type);
		}
		m_transient_pool.plan(frame_idx, transient_images, transient_buffers, pass_kind_order);
	}

	auto pass_queue = [&](const std::size_t pi) -> gpu::queue_type {
		return effective_queue(passes[pi].queue);
	};

	std::vector<gpu::command_buffer_handle> pass_bodies;
	std::array<std::atomic<std::uint32_t>, gpu::queue_type_count> profile_next_slot{};
	std::array<std::atomic<bool>, gpu::queue_type_count> profile_stats_issued{};

	auto record_range = [&](const std::size_t start, const std::size_t end) {
		task::parallel_invoke_range(
			start,
			end,
			[&](std::size_t pi) {
				auto& pass = passes[pi];
				const auto queue = pass_queue(pi);
				const bool is_graphics_pass = !pass.color_outputs.empty() || pass.depth_output;

				const auto worker_idx = task::current_worker();
				assert(worker_idx.has_value(), "graph::record_parallel: thread has no arena slot");
				const auto body = m_device->acquire_worker_command_buffer(queue, *worker_idx, frame_idx);

				const auto* depth_target = pass.depth_output ? resolve_depth_target(*pass.depth_output) : nullptr;

				std::vector<const image*> color_targets;
				color_targets.reserve(pass.color_outputs.size());
				for (const auto& info : pass.color_outputs) {
					color_targets.push_back(resolve_color_target(info));
				}

				std::vector<gpu::rendering_attachment_info> color_attachments;
				color_attachments.reserve(pass.color_outputs.size());
				std::optional<gpu::rendering_attachment_info> depth_att;
				vec2u pass_extent = swap_extent;
				bool extent_set = false;
				for (std::size_t ci = 0; ci < pass.color_outputs.size(); ++ci) {
					const auto* color_target = color_targets[ci];
					gpu::handle<gpu::image_view> color_view;
					if (color_target) {
						color_view = color_target->view();
						if (!extent_set) {
							const auto ext = color_target->extent();
							pass_extent = vec2u{ ext.x(), ext.y() };
							extent_set = true;
						}
					}
					else {
						color_view = m_swapchain->image_view(image_index);
					}
					color_attachments.push_back(
						gpu::rendering_attachment_info{
							.image_view = color_view,
							.load = pass.color_outputs[ci].op,
							.store = gpu::store_op::store,
							.color_clear_value = pass.color_outputs[ci].clear_value,
						}
					);
				}
				if (pass.depth_output) {
					gpu::handle<gpu::image_view> depth_view;
					if (depth_target) {
						depth_view = depth_target->view();
						if (!extent_set) {
							const auto ext = depth_target->extent();
							pass_extent = vec2u{ ext.x(), ext.y() };
							extent_set = true;
						}
					}
					else {
						depth_view = m_swapchain->depth_image().view();
					}
					depth_att = gpu::rendering_attachment_info{
						.image_view = depth_view,
						.load = pass.depth_output->op,
						.store = gpu::store_op::store,
						.depth_clear_value = pass.depth_output->clear_value,
					};
				}

				const auto body_cmd = m_device->recorder(body);
				body_cmd.begin();

				const auto marker_domain = (queue == gpu::queue_type::graphics)
					? gpu::device::pass_marker_domain::graphics_queue
					: gpu::device::pass_marker_domain::compute_queue;
				const auto marker_handle = m_device->begin_pass_marker(
					body,
					marker_domain,
					{
						.frame_counter = m_frames_submitted,
						.pass_index = static_cast<std::uint32_t>(pi),
						.pass_type = pass.pass_type,
					},
					pass.pass_name
				);

				std::uint32_t profile_slot = max_profiled_passes;
				gpu_profile_slot* profile = nullptr;
				bool issue_stats = false;
				if (timestamps_enabled) {
					const auto slot = profile_next_slot[static_cast<std::size_t>(queue)].fetch_add(1, std::memory_order_relaxed);
					if (slot < max_profiled_passes) {
						profile_slot = slot;
						profile = std::addressof(m_profile_slots[static_cast<std::size_t>(queue)][frame_idx]);
						body_cmd.write_timestamp(gpu::pipeline_stage_flags{}, profile->timestamp_pool, 1 + profile_slot * 2);
						profile->pass_types[profile_slot] = pass.pass_type;
						profile->pass_queues[profile_slot] = queue;
						issue_stats = stats_enabled && is_graphics_pass && queue == gpu::queue_type::graphics;
						if (issue_stats) {
							body_cmd.begin_query(profile->stats_pool, profile_slot);
							profile_stats_issued[static_cast<std::size_t>(queue)].store(true, std::memory_order_relaxed);
						}
					}
				}

				if (is_graphics_pass) {
					body_cmd.begin_rendering(
						gpu::rendering_info{
							.render_area = gse::rect_t<vec2i>({
								.min = vec2i{ 0, 0 },
								.max = vec2i{ static_cast<int>(pass_extent.x()), static_cast<int>(pass_extent.y()) }
							}),
							.layer_count = 1,
							.color_attachments = color_attachments,
							.depth_attachment = depth_att ? &*depth_att : nullptr,
							.secondary_command_buffers = false,
						}
					);
				}

				auto& rec_init = *static_cast<recording_context_init*>(pass.record_ctx_slot);
				rec_init.recorder = m_device->recorder(body);
				rec_init.pass = std::addressof(pass);
				rec_init.transient_pool = std::addressof(m_transient_pool);
				rec_init.device = m_device;
				rec_init.primary = pass.primary_pipeline;
				rec_init.touches.clear();
				const auto note = [&](const resource_ref ref, const gpu::pipeline_stage_flags stages, const gpu::access_flags access) {
					rec_init.touches.push_back({ ref, stages, access });
				};
				if (pass.depth_output) {
					const auto* depth_img = depth_target ? depth_target : std::addressof(m_swapchain->depth_image());
					const auto depth_ref = resource_ref{
						.ptr = std::bit_cast<const void*>(depth_img->handle()),
						.type = resource_type::image,
						.aspects = gpu::image_aspect_for(depth_img->format()),
					};
					if (pass.depth_output->op == load_op::load) {
						note(
							depth_ref,
							gpu::pipeline_stage_flag::early_fragment_tests | gpu::pipeline_stage_flag::late_fragment_tests,
							gpu::access_flag::depth_stencil_attachment_read | gpu::access_flag::depth_stencil_attachment_write
						);
					}
					else {
						note(
							depth_ref,
							gpu::pipeline_stage_flag::late_fragment_tests,
							gpu::access_flag::depth_stencil_attachment_write
						);
					}
				}
				for (std::size_t ci = 0; ci < pass.color_outputs.size(); ++ci) {
					const auto* color_img = color_targets[ci];
					if (color_img == nullptr) {
						continue;
					}
					const auto color_ref = resource_ref{
						.ptr = std::bit_cast<const void*>(color_img->handle()),
						.type = resource_type::image,
						.aspects = gpu::image_aspect_for(color_img->format()),
					};
					if (pass.color_outputs[ci].op == load_op::load) {
						note(
							color_ref,
							gpu::pipeline_stage_flag::color_attachment_output,
							gpu::access_flag::color_attachment_read | gpu::access_flag::color_attachment_write
						);
					}
					else {
						note(
							color_ref,
							gpu::pipeline_stage_flag::color_attachment_output,
							gpu::access_flag::color_attachment_write
						);
					}
				}
				pass.record_handle.resume();

				if (is_graphics_pass) {
					body_cmd.end_rendering();
				}

				if (issue_stats) {
					body_cmd.end_query(profile->stats_pool, profile_slot);
				}
				if (profile != nullptr) {
					body_cmd.write_timestamp(gpu::pipeline_stage_flag::all_commands, profile->timestamp_pool, 2 + profile_slot * 2);
				}

				m_device->post_renderpass_pass_marker(body, marker_handle);
				m_device->end_pass_marker(body, marker_handle);
				body_cmd.end();

				pass_bodies[pi] = body;
			},
			trace_id<"render_graph::record_passes">()
		);
	};

	std::size_t round_start = 0;
	while (true) {
		pass_bodies.resize(passes.size());
		record_range(round_start, passes.size());

		auto more = drain.drain_passes();
		if (more.empty()) {
			break;
		}

		auto more_images = drain.drain_images();
		auto more_buffers = drain.drain_buffers();
		assert(
			more_images.empty(),
			"render_graph: round 2+ passes may not request new transient_image resources (option A); declare all "
			"transients before the first co_await pass(...)"
		);
		assert(
			more_buffers.empty(),
			"render_graph: round 2+ passes may not request new transient_buffer resources (option A); declare all "
			"transients before the first co_await pass(...)"
		);

		round_start = passes.size();
		passes.insert(passes.end(), std::make_move_iterator(more.begin()), std::make_move_iterator(more.end()));
	}

	if (timestamps_enabled) {
		for (std::size_t qi = 0; qi < gpu::queue_type_count; ++qi) {
			auto& slot = m_profile_slots[qi][frame_idx];
			slot.pass_count = std::min(profile_next_slot[qi].load(std::memory_order_relaxed), max_profiled_passes);
			slot.stats_issued = profile_stats_issued[qi].load(std::memory_order_relaxed);
		}
	}

	std::array<bool, gpu::queue_type_count> queue_has_work{};
	queue_has_work[static_cast<std::size_t>(gpu::queue_type::graphics)] = true;
	for (const auto& p : passes) {
		queue_has_work[static_cast<std::size_t>(effective_queue(p.queue))] = true;
	}

	std::vector<std::size_t> sorted;
	std::array<std::vector<gpu::command_buffer_handle>, gpu::queue_type_count> queue_submit_order;

	if (timestamps_enabled) {
		for (std::size_t qi = 0; qi < gpu::queue_type_count; ++qi) {
			if (!queue_has_work[qi]) {
				continue;
			}
			auto& slot = m_profile_slots[qi][frame_idx];
			const auto q = static_cast<gpu::queue_type>(qi);
			const bool with_stats = qi == static_cast<std::size_t>(gpu::queue_type::graphics) && stats_enabled;
			const auto profile_begin = m_device->acquire_worker_command_buffer(q, 0, frame_idx);
			const auto pcmd = m_device->recorder(profile_begin);
			pcmd.begin();
			pcmd.reset_query_pool(slot.timestamp_pool, 0, max_profiled_passes * 2 + 1);
			if (with_stats) {
				pcmd.reset_query_pool(slot.stats_pool, 0, max_profiled_passes);
			}
			slot.cpu_ref = system_clock::now<trace::tick_step>();
			slot.frame_counter = m_frames_submitted;
			pcmd.write_timestamp(gpu::pipeline_stage_flag::all_commands, slot.timestamp_pool, 0);
			pcmd.end();
			queue_submit_order[qi].push_back(profile_begin);
		}
	}

	{
		trace::scope_guard sg{ gse::trace_id<"graph::plan">() };
		std::unordered_map<id, std::size_t> type_to_index;
		for (std::size_t i = 0; i < passes.size(); ++i) {
			type_to_index[passes[i].pass_type] = i;
		}

		enum class edge_kind : std::uint8_t {
			explicit_after,
			chain,
			write_after_read,
			both_write,
		};

		struct edge {
			std::size_t to;
			edge_kind kind;
		};

		std::vector<std::vector<edge>> adj(passes.size());
		std::vector<std::size_t> in_degree(passes.size(), 0);

		const std::size_t n = passes.size();
		std::vector<std::vector<bool>> reaches(n, std::vector<bool>(n, false));

		auto has_edge = [&](const std::size_t from, const std::size_t to) -> bool {
			for (const auto& e : adj[from]) {
				if (e.to == to) {
					return true;
				}
			}
			return false;
		};

		auto update_reachability_for_new_edge = [&](const std::size_t from, const std::size_t to) {
			for (std::size_t x = 0; x < n; ++x) {
				const bool x_reaches_from = (x == from) || reaches[x][from];
				if (!x_reaches_from) {
					continue;
				}
				for (std::size_t y = 0; y < n; ++y) {
					const bool to_reaches_y = (y == to) || reaches[to][y];
					if (to_reaches_y) {
						reaches[x][y] = true;
					}
				}
			}
		};

		auto add_edge = [&](const std::size_t from, const std::size_t to, const edge_kind kind) {
			if (has_edge(from, to)) {
				return;
			}
			adj[from].push_back({
				.to = to,
				.kind = kind
			});
			++in_degree[to];
			update_reachability_for_new_edge(from, to);
		};

		for (std::size_t i = 0; i < passes.size(); ++i) {
			for (const auto& dep : passes[i].after_passes) {
				if (auto it = type_to_index.find(dep); it != type_to_index.end()) {
					add_edge(it->second, i, edge_kind::explicit_after);
				}
			}
		}

		for (std::size_t i = 0; i < passes.size(); ++i) {
			for (std::size_t j = i + 1; j < passes.size(); ++j) {
				if (passes[i].chain_id.exists() && passes[i].chain_id == passes[j].chain_id) {
					add_edge(i, j, edge_kind::chain);
					continue;
				}

				bool i_writes_j_reads = false;
				bool j_writes_i_reads = false;
				bool both_write = false;

				for (const auto& w : passes[i].writes) {
					for (const auto& r : passes[j].reads) {
						if (w.resource.ptr && r.resource.ptr && w.resource.ptr == r.resource.ptr) {
							i_writes_j_reads = true;
						}
					}
				}
				for (const auto& w : passes[j].writes) {
					for (const auto& r : passes[i].reads) {
						if (w.resource.ptr && r.resource.ptr && w.resource.ptr == r.resource.ptr) {
							j_writes_i_reads = true;
						}
					}
				}
				for (const auto& wi : passes[i].writes) {
					for (const auto& wj : passes[j].writes) {
						if (wi.resource.ptr && wj.resource.ptr && wi.resource.ptr == wj.resource.ptr) {
							both_write = true;
						}
					}
				}

				if (i_writes_j_reads || both_write) {
					if (!reaches[j][i]) {
						if (both_write && !reaches[i][j]) {
							const auto a = passes[i].pass_type;
							const auto b = passes[j].pass_type;
							const auto key = std::pair{ std::min(a, b), std::max(a, b) };
							if (m_warned_ambiguous_pairs.insert(key).second) {
								log::println(
									log::level::warning,
									log::category::render,
									"render_graph: passes '{}' and '{}' both write the same resource with no "
									"explicit ordering; recording order picks ('{}' first). Add `.after<>` to "
									"pin the visual order.",
									a.tag(),
									b.tag(),
									a.tag()
								);
							}
						}
						add_edge(i, j, both_write ? edge_kind::both_write : edge_kind::write_after_read);
					}
				}
				else if (j_writes_i_reads) {
					if (!reaches[i][j]) {
						add_edge(j, i, edge_kind::write_after_read);
					}
				}
			}
		}

		sorted.reserve(passes.size());

		std::vector<std::size_t> remaining_in_degree = in_degree;
		std::queue<std::size_t> queue;
		for (std::size_t i = 0; i < passes.size(); ++i) {
			if (remaining_in_degree[i] == 0) {
				queue.push(i);
			}
		}

		while (!queue.empty()) {
			auto front = queue.front();
			queue.pop();
			sorted.push_back(front);
			for (const auto& e : adj[front]) {
				if (--remaining_in_degree[e.to] == 0) {
					queue.push(e.to);
				}
			}
		}

		if (sorted.size() != passes.size()) {
			std::vector<std::uint8_t> state(passes.size(), 0);
			std::vector<std::size_t> stack;
			std::vector<edge_kind> stack_kinds;
			std::vector<std::size_t> cycle_nodes;
			std::vector<edge_kind> cycle_kinds;

			auto find_cycle = [&](this auto& self, const std::size_t u) -> bool {
				state[u] = 1;
				stack.push_back(u);
				for (const auto& e : adj[u]) {
					if (state[e.to] == 1) {
						const auto start_it = std::ranges::find(stack, e.to);
						const auto start_idx = static_cast<std::size_t>(start_it - stack.begin());
						cycle_nodes.assign(start_it, stack.end());
						cycle_nodes.push_back(e.to);
						cycle_kinds.assign(stack_kinds.begin() + start_idx, stack_kinds.end());
						cycle_kinds.push_back(e.kind);
						return true;
					}
					if (state[e.to] == 0) {
						stack_kinds.push_back(e.kind);
						if (self(e.to)) {
							return true;
						}
						stack_kinds.pop_back();
					}
				}
				stack.pop_back();
				state[u] = 2;
				return false;
			};

			for (std::size_t i = 0; i < passes.size() && cycle_nodes.empty(); ++i) {
				if (state[i] == 0) {
					find_cycle(i);
				}
			}

			std::string cycle_str;
			for (std::size_t k = 0; k < cycle_nodes.size(); ++k) {
				if (k > 0) {
					cycle_str += std::format(" --[{}]--> ", cycle_kinds[k - 1]);
				}
				cycle_str += passes[cycle_nodes[k]].pass_type.tag();
			}

			assert(false, "render_graph: cyclic pass dependency graph:\n  {}", cycle_str);
		}
	}

	{
		bool swapchain_cleared = false;
		for (const auto pi : sorted) {
			for (auto& info : passes[pi].color_outputs) {
				if (resolve_color_target(info) != nullptr) {
					continue;
				}
				if (swapchain_cleared) {
					info.op = load_op::load;
				}
				else {
					info.op = m_swapchain_load;
					info.clear_value = m_swapchain_clear;
					swapchain_cleared = true;
				}
			}
		}
	}

	auto queue_label = [](const std::size_t qi) -> const char* {
		return qi == static_cast<std::size_t>(gpu::queue_type::graphics) ? "graphics" : "compute";
	};

	std::array<std::array<bool, gpu::queue_type_count>, gpu::queue_type_count> queue_waits_on{};
	for (std::size_t i = 0; i < passes.size(); ++i) {
		for (std::size_t j = 0; j < passes.size(); ++j) {
			if (i == j) {
				continue;
			}
			const auto qi = pass_queue(i);
			const auto qj = pass_queue(j);
			if (qi == qj) {
				continue;
			}
			for (const auto& w : passes[i].writes) {
				for (const auto& r : passes[j].reads) {
					if (w.resource.ptr && r.resource.ptr && w.resource.ptr == r.resource.ptr) {
						queue_waits_on[static_cast<std::size_t>(qj)][static_cast<std::size_t>(qi)] = true;
					}
				}
			}
		}
	}

	{
		std::unordered_map<id, std::size_t> after_index;
		for (std::size_t i = 0; i < passes.size(); ++i) {
			after_index[passes[i].pass_type] = i;
		}
		for (std::size_t j = 0; j < passes.size(); ++j) {
			const auto qj = pass_queue(j);
			for (const auto dep : passes[j].after_passes) {
				const auto it = after_index.find(dep);
				if (it == after_index.end()) {
					continue;
				}
				const auto qi = pass_queue(it->second);
				if (qi != qj) {
					queue_waits_on[static_cast<std::size_t>(qj)][static_cast<std::size_t>(qi)] = true;
				}
			}
		}
	}

	for (std::size_t a = 0; a < gpu::queue_type_count; ++a) {
		for (std::size_t b = a + 1; b < gpu::queue_type_count; ++b) {
			if (queue_waits_on[a][b] && queue_waits_on[b][a] && m_warned_queue_cycles.insert({ a, b }).second) {
				log::println(
					log::level::error,
					log::category::render,
					"render_graph: cyclic cross-queue dependency between '{}' and '{}'; frame-granular sync cannot express this -- one direction is dropped (races) and the device may hang. See docs/render_graph_cross_queue_batching_plan.md",
					queue_label(a),
					queue_label(b)
				);
			}
		}
	}

	{
		trace::scope_guard sg{ gse::trace_id<"graph::record_replay">() };

		auto access_has_write = [](const gpu::access_flags a) -> bool {
			using ac = gpu::access_flag;
			constexpr auto write_mask = ac::shader_storage_write | ac::shader_write | ac::color_attachment_write |
				ac::depth_stencil_attachment_write | ac::transfer_write | ac::host_write | ac::memory_write |
				ac::acceleration_structure_write;
			return static_cast<bool>(a & write_mask);
		};

		auto append_barrier_for_resource = [&](
			const resource_ref& resource,
			const gpu::pipeline_stage_flags src_stages,
			const gpu::access_flags src_access,
			const gpu::pipeline_stage_flags dst_stages,
			const gpu::access_flags dst_access,
			std::vector<gpu::memory_barrier>& memory_out,
			std::vector<gpu::buffer_barrier>& buffer_out,
			std::vector<gpu::image_barrier>& image_out
		) {
			if (!access_has_write(src_access) && !access_has_write(dst_access) && src_stages.bits() == dst_stages.bits()) {
				return;
			}
			if (resource.type == resource_type::buffer) {
				buffer_out.push_back({
					.src_stages = src_stages,
					.src_access = src_access,
					.dst_stages = dst_stages,
					.dst_access = dst_access,
					.buffer = std::bit_cast<gpu::handle<gpu::buffer>>(resource.ptr),
					.offset = 0,
					.size = resource.buffer_size,
				});
			}
			else if (resource.type == resource_type::image) {
				image_out.push_back({
					.src_stages = src_stages,
					.src_access = src_access,
					.dst_stages = dst_stages,
					.dst_access = dst_access,
					.image = std::bit_cast<gpu::handle<gpu::image>>(resource.ptr),
					.aspects = resource.aspects,
					.base_mip_level = 0,
					.level_count = 1,
					.base_array_layer = 0,
					.layer_count = 1,
				});
			}
			else {
				memory_out.push_back({
					.src_stages = src_stages,
					.src_access = src_access,
					.dst_stages = dst_stages,
					.dst_access = dst_access,
				});
			}
		};

		auto append_host_dirty_barriers = [&](const render_pass_data& p, std::vector<gpu::buffer_barrier>& out) {
			auto walk = [&](const std::vector<resource_usage>& list) {
				for (const auto& [resource, stage, access] : list) {
					if (resource.type != resource_type::buffer || !resource.host_buffer) {
						continue;
					}
					const auto* buf = static_cast<const buffer*>(resource.host_buffer);
					if (!buf->host_dirty()) {
						continue;
					}
					out.push_back({
						.src_stages = gpu::pipeline_stage_flag::host,
						.src_access = gpu::access_flag::host_write,
						.dst_stages = stage,
						.dst_access = access,
						.buffer = buf->handle(),
						.offset = 0,
						.size = buf->size(),
					});
					buf->clear_host_dirty();
				}
			};
			walk(p.reads);
			walk(p.writes);
		};

		struct prev_use_record {
			resource_ref resource;
			gpu::pipeline_stage_flags stages;
			gpu::access_flags access;
		};

		std::array<std::unordered_map<const void*, prev_use_record>, gpu::queue_type_count> latest_writes;
		std::array<std::unordered_map<const void*, std::vector<prev_use_record>>, gpu::queue_type_count> reads_since_write;

		auto append_prev_pass_barriers = [&](
			const render_pass_data& cur,
			const gpu::queue_type cur_queue,
			std::vector<gpu::memory_barrier>& memory_out,
			std::vector<gpu::buffer_barrier>& buffer_out,
			std::vector<gpu::image_barrier>& image_out
		) {
			auto& latest = latest_writes[static_cast<std::size_t>(cur_queue)];
			auto& reads = reads_since_write[static_cast<std::size_t>(cur_queue)];

			for (const auto& [cur_resource, cur_stage, cur_access] : cur.reads) {
				if (!cur_resource.ptr) {
					continue;
				}
				if (const auto it = latest.find(cur_resource.ptr); it != latest.end()) {
					const auto& prev = it->second;
					append_barrier_for_resource(
						prev.resource,
						prev.stages,
						prev.access,
						cur_stage,
						cur_access,
						memory_out,
						buffer_out,
						image_out
					);
				}
				else if (cur_resource.type == resource_type::image) {
					append_barrier_for_resource(
						cur_resource,
						{},
						{},
						cur_stage,
						cur_access,
						memory_out,
						buffer_out,
						image_out
					);
				}
			}

			for (const auto& [cur_resource, cur_stage, cur_access] : cur.writes) {
				if (!cur_resource.ptr) {
					continue;
				}
				bool had_prev = false;
				if (const auto it = latest.find(cur_resource.ptr); it != latest.end()) {
					const auto& prev = it->second;
					append_barrier_for_resource(
						prev.resource,
						prev.stages,
						prev.access,
						cur_stage,
						cur_access,
						memory_out,
						buffer_out,
						image_out
					);
					had_prev = true;
				}
				if (const auto it = reads.find(cur_resource.ptr); it != reads.end()) {
					for (const auto& prev_read : it->second) {
						append_barrier_for_resource(
							prev_read.resource,
							prev_read.stages,
							{},
							cur_stage,
							cur_access,
							memory_out,
							buffer_out,
							image_out
						);
					}
					had_prev = true;
				}
				if (!had_prev) {
					append_barrier_for_resource(
						cur_resource,
						cur_stage,
						{},
						cur_stage,
						cur_access,
						memory_out,
						buffer_out,
						image_out
					);
				}
			}

			for (const auto& [cur_resource, cur_stage, cur_access] : cur.writes) {
				if (!cur_resource.ptr) {
					continue;
				}
				latest[cur_resource.ptr] = { cur_resource, cur_stage, cur_access };
				reads.erase(cur_resource.ptr);
			}
			for (const auto& [cur_resource, cur_stage, cur_access] : cur.reads) {
				if (!cur_resource.ptr) {
					continue;
				}
				reads[cur_resource.ptr].push_back({ cur_resource, cur_stage, cur_access });
			}
		};

		std::vector<std::vector<gpu::image_barrier>> alias_barriers_for_sorted(sorted.size());
		{
			const auto transient_infos = m_transient_pool.transient_images();
			for (const auto& info : transient_infos) {
				std::size_t first_si = sorted.size();
				gpu::pipeline_stage_flags first_stages;
				gpu::access_flags first_access;
				for (std::size_t si = 0; si < sorted.size(); ++si) {
					const auto& p = passes[sorted[si]];
					auto match = [&](const std::vector<resource_usage>& list) -> bool {
						for (const auto& u : list) {
							if (u.resource.type == resource_type::image && u.resource.ptr == std::bit_cast<const void*>(info.resource->handle())) {
								first_stages |= u.stage;
								first_access |= u.access;
								return true;
							}
						}
						return false;
					};
					const bool in_reads = match(p.reads);
					const bool in_writes = match(p.writes);
					if (in_reads || in_writes) {
						first_si = si;
						break;
					}
				}
				if (first_si == sorted.size()) {
					continue;
				}
				alias_barriers_for_sorted[first_si].push_back({
					.src_stages = gpu::pipeline_stage_flag::all_commands,
					.src_access = gpu::access_flag::memory_write,
					.dst_stages = first_stages,
					.dst_access = first_access,
					.discard_contents = true,
					.image = info.resource->handle(),
					.aspects = info.aspects,
					.base_mip_level = 0,
					.level_count = 1,
					.base_array_layer = 0,
					.layer_count = 1,
				});
			}
		}

		for (std::size_t si = 0; si < sorted.size(); ++si) {
			const auto pass_idx = sorted[si];
			auto& pass = passes[pass_idx];
			const auto queue = pass_queue(pass_idx);

			std::vector<gpu::memory_barrier> memory_barriers;
			std::vector<gpu::buffer_barrier> buffer_barriers;
			std::vector<gpu::image_barrier> image_barriers = std::move(alias_barriers_for_sorted[si]);

			append_host_dirty_barriers(pass, buffer_barriers);
			append_prev_pass_barriers(pass, queue, memory_barriers, buffer_barriers, image_barriers);

			{
				std::vector<gpu::memory_barrier> coalesced;
				coalesced.reserve(memory_barriers.size());
				for (const auto& b : memory_barriers) {
					bool merged = false;
					for (auto& o : coalesced) {
						if (o.src_stages.bits() == b.src_stages.bits() && o.dst_stages.bits() == b.dst_stages.bits()) {
							o.src_access |= b.src_access;
							o.dst_access |= b.dst_access;
							merged = true;
							break;
						}
					}
					if (!merged) {
						coalesced.push_back(b);
					}
				}
				memory_barriers = std::move(coalesced);
			}

			{
				std::vector<gpu::buffer_barrier> coalesced;
				coalesced.reserve(buffer_barriers.size());
				for (const auto& b : buffer_barriers) {
					bool merged = false;
					for (auto& o : coalesced) {
						if (o.buffer.value == b.buffer.value && o.offset == b.offset && o.size == b.size && o.src_stages.bits() == b.src_stages.bits() && o.dst_stages.bits() == b.dst_stages.bits()) {
							o.src_access |= b.src_access;
							o.dst_access |= b.dst_access;
							merged = true;
							break;
						}
					}
					if (!merged) {
						coalesced.push_back(b);
					}
				}
				buffer_barriers = std::move(coalesced);
			}

			{
				std::vector<gpu::image_barrier> coalesced;
				coalesced.reserve(image_barriers.size());
				for (const auto& b : image_barriers) {
					bool merged = false;
					for (auto& o : coalesced) {
						if (o.image.value == b.image.value && o.aspects.bits() == b.aspects.bits() && o.base_mip_level == b.base_mip_level && o.level_count == b.level_count && o.base_array_layer == b.base_array_layer && o.layer_count == b.layer_count && o.src_stages.bits() == b.src_stages.bits() && o.dst_stages.bits() == b.dst_stages.bits()) {
							o.src_access |= b.src_access;
							o.dst_access |= b.dst_access;
							merged = true;
							break;
						}
					}
					if (!merged) {
						coalesced.push_back(b);
					}
				}
				image_barriers = std::move(coalesced);
			}

			const auto queue_index = static_cast<std::size_t>(queue);
			if (!memory_barriers.empty() || !buffer_barriers.empty() || !image_barriers.empty()) {
				const auto transition = m_device->acquire_worker_command_buffer(queue, 0, frame_idx);
				const auto tcmd = m_device->recorder(transition);
				tcmd.begin();
				tcmd.pipeline_barrier(
					gpu::dependency_info{
						.memory_barriers = memory_barriers,
						.buffer_barriers = buffer_barriers,
						.image_barriers = image_barriers,
					}
				);
				tcmd.end();
				queue_submit_order[queue_index].push_back(transition);
			}

			queue_submit_order[queue_index].push_back(pass_bodies[pass_idx]);
		}
	}

	if (timestamps_enabled) {
		for (std::size_t qi = 0; qi < gpu::queue_type_count; ++qi) {
			auto& slot = m_profile_slots[qi][frame_idx];
			if (slot.pass_count > 0) {
				slot.results_valid = true;
			}
		}
	}

	std::array<std::uint64_t, gpu::queue_type_count> this_frame_signal_values{};

	for (std::size_t qi = 0; qi < gpu::queue_type_count; ++qi) {
		if (qi == static_cast<std::size_t>(gpu::queue_type::graphics)) {
			continue;
		}
		if (!queue_has_work[qi]) {
			continue;
		}

		const auto q = static_cast<gpu::queue_type>(qi);

		auto& state = m_queue_states[qi];
		const std::uint64_t previous_value = state.signal_counter;
		const std::uint64_t signal_value = ++state.signal_counter;
		this_frame_signal_values[qi] = signal_value;

		gpu::queue_submission sub;
		sub.queue = q;
		sub.command_buffers = std::move(queue_submit_order[qi]);
		if (previous_value > 0) {
			sub.waits.push_back({
				.semaphore = state.timeline.handle(),
				.value = previous_value,
				.stages = gpu::pipeline_stage_flag::all_commands,
			});
		}
		for (std::size_t producer = 0; producer < gpu::queue_type_count; ++producer) {
			if (producer == qi) {
				continue;
			}
			if (queue_waits_on[qi][producer] && this_frame_signal_values[producer] > 0) {
				sub.waits.push_back({
					.semaphore = m_queue_states[producer].timeline.handle(),
					.value = this_frame_signal_values[producer],
					.stages = gpu::pipeline_stage_flag::all_commands,
				});
			}
			else if (queue_waits_on[qi][producer] && queue_has_work[producer] && m_warned_dropped_waits.insert({ qi, producer }).second) {
				log::println(
					log::level::error,
					log::category::render,
					"render_graph: cross-queue wait '{}' -> '{}' dropped (producer emits no timeline signal this frame); consumer reads producer output unsynchronized",
					queue_label(qi),
					queue_label(producer)
				);
			}
		}
		sub.signals.push_back({
			.semaphore = state.timeline.handle(),
			.value = signal_value,
			.stages = gpu::pipeline_stage_flag::all_commands,
		});
		m_pending_aux_submissions.push_back(std::move(sub));
	}

	const auto graphics_qi = static_cast<std::size_t>(gpu::queue_type::graphics);
	m_pending_graphics_buffers = std::move(queue_submit_order[graphics_qi]);
	for (std::size_t producer = 0; producer < gpu::queue_type_count; ++producer) {
		if (producer == graphics_qi) {
			continue;
		}
		if (queue_waits_on[graphics_qi][producer] && this_frame_signal_values[producer] > 0) {
			m_pending_graphics_extra_waits.push_back({
				.semaphore = m_queue_states[producer].timeline.handle(),
				.value = this_frame_signal_values[producer],
				.stages = gpu::pipeline_stage_flag::all_commands,
			});
		}
		else if (queue_waits_on[graphics_qi][producer] && queue_has_work[producer] && m_warned_dropped_waits.insert({ graphics_qi, producer }).second) {
			log::println(
				log::level::error,
				log::category::render,
				"render_graph: cross-queue wait '{}' -> '{}' dropped (producer emits no timeline signal this frame); consumer reads producer output unsynchronized",
				queue_label(graphics_qi),
				queue_label(producer)
			);
		}
	}
}
