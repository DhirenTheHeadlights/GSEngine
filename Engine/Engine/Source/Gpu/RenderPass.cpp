module gse.gpu;

import std;

import :render_pass;
import :render_graph;

import gse.assert;
import gse.core;
import gse.concurrency;
import gse.diag;
import gse.ecs;

gse::gpu::request_pass_awaitable::request_pass_awaitable(const frame_context& ctx, render_pass_descriptor desc) noexcept : m_ctx(std::addressof(ctx)), m_desc(std::move(desc)) {
	assert(m_desc.pass_kind.exists(), "render pass descriptor missing pass_kind; pass it via gpu::pass(ctx, trace_id<owner>()) or gpu::pass<owner>(ctx)");
}

auto gse::gpu::request_pass_awaitable::await_ready() const noexcept -> bool {
	return false;
}

auto gse::gpu::request_pass_awaitable::await_suspend(const std::coroutine_handle<> h) noexcept -> void {
	m_trace_id = find_or_generate_id(std::format("record<{}>", m_desc.pass_kind.tag()));
	m_trace_key = trace::allocate_async_key();
	trace::begin_async(m_trace_id, m_trace_key);

	m_ctx->channels.push<render_pass_request>({
		.desc = std::move(m_desc),
		.record_handle = h,
		.record_ctx_slot = std::addressof(m_rec),
	});
}

auto gse::gpu::request_pass_awaitable::await_resume() noexcept -> recording_context {
	trace::end_async(m_trace_id, m_trace_key);
	return std::move(*m_rec);
}

gse::gpu::pass_builder::pass_builder(const frame_context& ctx, const id pass_kind) noexcept : m_ctx(std::addressof(ctx)) {
	m_desc.pass_kind = pass_kind;
}

auto gse::gpu::pass_builder::on(const queue_type queue) && -> pass_builder&& {
	m_desc.queue = queue;
	return std::move(*this);
}

auto gse::gpu::pass_builder::color(color_attachment value) && -> pass_builder&& {
	m_desc.color = value;
	return std::move(*this);
}

auto gse::gpu::pass_builder::depth(depth_attachment value) && -> pass_builder&& {
	m_desc.depth = value;
	return std::move(*this);
}

auto gse::gpu::pass_builder::operator co_await() && -> request_pass_awaitable {
	return request_pass_awaitable{ *m_ctx, std::move(m_desc) };
}

auto gse::gpu::pass(const frame_context& ctx, const id pass_kind) -> pass_builder {
	return pass_builder{ ctx, pass_kind };
}

auto gse::gpu::clear_color(const gpu::color_clear value) -> color_attachment {
	return {
		.op = load_op::clear,
		.clear = value,
	};
}

auto gse::gpu::load_color() -> color_attachment {
	return {
		.op = load_op::load,
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
