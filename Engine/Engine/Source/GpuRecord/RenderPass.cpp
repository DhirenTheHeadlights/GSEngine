module gse.gpu_record:render_pass_impl;

import std;

import :render_pass;
import :recording_context;

import gse.gpu;

import gse.assert;
import gse.core;
import gse.concurrency;
import gse.diag;

gse::gpu::request_pass_awaitable::request_pass_awaitable(const channel_write<render_pass_request> channels, render_pass_descriptor desc) noexcept
	: m_channels(channels), m_desc(std::move(desc)) {
	assert(
		m_desc.pass_kind.exists(),
		"render pass descriptor missing pass_kind; pass it via gpu::pass(ctx, trace_id<owner>()) or "
		"gpu::pass<owner>(ctx)"
	);
}

auto gse::gpu::request_pass_awaitable::await_ready() const noexcept -> bool {
	return false;
}

auto gse::gpu::request_pass_awaitable::await_suspend(const std::coroutine_handle<> h) noexcept -> void {
	recording_context::finalize_active_on_current_thread();

	m_trace_id = m_desc.trace_kind.exists()
		? m_desc.trace_kind
		: find_or_generate_id(std::format("record<{}>", m_desc.pass_kind.tag()));
	m_trace_key = trace::allocate_async_key();
	trace::begin_async(m_trace_id, m_trace_key);

	m_channels.push<render_pass_request>({
		.desc = std::move(m_desc),
		.record_handle = h,
		.record_ctx_slot = std::addressof(m_init),
	});
}

auto gse::gpu::request_pass_awaitable::await_resume() noexcept -> recording_context {
	trace::end_async(m_trace_id, m_trace_key);
	return recording_context{ std::move(m_init) };
}

gse::gpu::pass_builder::pass_builder(const channel_write<render_pass_request> channels, const id pass_kind, const std::string_view pass_name, const id trace_kind) noexcept
	: m_channels(channels) {
	m_desc.pass_kind = pass_kind;
	m_desc.pass_name = pass_name;
	m_desc.trace_kind = trace_kind;
}

auto gse::gpu::pass_builder::on(const queue_type queue) && -> pass_builder&& {
	m_desc.queue = queue;
	return std::move(*this);
}

auto gse::gpu::pass_builder::pipeline(const gpu::shader_program& p) && -> pass_builder&& {
	m_desc.primary_pipeline = std::addressof(p);
	return std::move(*this);
}

auto gse::gpu::pass_builder::color(color_attachment value) && -> pass_builder&& {
	m_desc.colors.push_back(value);
	return std::move(*this);
}

auto gse::gpu::pass_builder::depth(depth_attachment value) && -> pass_builder&& {
	m_desc.depth = value;
	return std::move(*this);
}

auto gse::gpu::pass_builder::target(const id window) && -> pass_builder&& {
	m_desc.target = window;
	return std::move(*this);
}

auto gse::gpu::pass_builder::early_signal() && -> pass_builder&& {
	m_desc.early_signal = true;
	return std::move(*this);
}

auto gse::gpu::pass_builder::operator co_await() && -> request_pass_awaitable {
	return request_pass_awaitable{ m_channels, std::move(m_desc) };
}

auto gse::gpu::pass(const channel_write<render_pass_request> channels, const id pass_kind) -> pass_builder {
	return pass_builder{ channels, pass_kind };
}

auto gse::gpu::clear_color(const gpu::color_clear value) -> color_attachment {
	return {
		.op = load_op::clear,
		.clear = value,
	};
}

auto gse::gpu::clear_color(const gpu::color_clear value, const image& target) -> color_attachment {
	return {
		.op = load_op::clear,
		.clear = value,
		.target = std::addressof(target),
	};
}

auto gse::gpu::load_color() -> color_attachment {
	return {
		.op = load_op::load,
	};
}

auto gse::gpu::load_color(const image& target) -> color_attachment {
	return {
		.op = load_op::load,
		.target = std::addressof(target),
	};
}

auto gse::gpu::clear_depth(const gpu::depth_clear value) -> depth_attachment {
	return {
		.op = load_op::clear,
		.clear = value,
	};
}

auto gse::gpu::load_depth() -> depth_attachment {
	return {
		.op = load_op::load,
	};
}

namespace gse::gpu {
	auto to_color_output_info(
		const color_attachment& a,
		id window
	) -> gpu::color_output_info;

	auto to_depth_output_info(
		const depth_attachment& a
	) -> gpu::depth_output_info;

	auto to_pass_data(
		render_pass_request req
	) -> gpu::render_pass_data;
}

auto gse::gpu::to_color_output_info(const color_attachment& a, const id window) -> gpu::color_output_info {
	return {
		.is_swapchain = a.target == nullptr && !a.transient_target,
		.window = window,
		.custom_target = a.target,
		.transient_target = a.transient_target,
		.op = a.op,
		.clear_value = a.clear,
	};
}

auto gse::gpu::to_depth_output_info(const depth_attachment& a) -> gpu::depth_output_info {
	return {
		.is_swapchain = a.target == nullptr && !a.transient_target,
		.custom_target = a.target,
		.transient_target = a.transient_target,
		.op = a.op,
		.clear_value = a.clear,
	};
}

auto gse::gpu::to_pass_data(render_pass_request req) -> gpu::render_pass_data {
	gpu::render_pass_data p{
		.pass_type = req.desc.pass_kind,
		.pass_name = req.desc.pass_name,
		.queue = req.desc.queue,
		.primary_pipeline = req.desc.primary_pipeline,
		.after_passes = std::move(req.desc.after_deps),
		.chain_id = req.desc.chain_id,
		.early_signal = req.desc.early_signal,
		.record_handle = req.record_handle,
		.record_ctx_slot = req.record_ctx_slot,
	};

	p.color_outputs.reserve(req.desc.colors.size());
	for (const auto& c : req.desc.colors) {
		p.color_outputs.push_back(to_color_output_info(c, req.desc.target));
	}

	if (req.desc.depth) {
		p.depth_output = to_depth_output_info(*req.desc.depth);
	}

	return p;
}

auto gse::gpu::context::execute_frame(data& d, scheduler& s) -> void {
	d.render_graph->execute(frame_request_drain{
		.drain_passes = [&s] {
			auto requests = s.drain_channel<render_pass_request>();
			std::vector<render_pass_data> passes;
			passes.reserve(requests.size());
			for (auto& req : requests) {
				passes.push_back(to_pass_data(std::move(req)));
			}
			return passes;
		},
		.drain_images = [&s] {
			return s.drain_channel<transient_image_request>();
		},
		.drain_buffers = [&s] {
			return s.drain_channel<transient_buffer_request>();
		},
	});
}
