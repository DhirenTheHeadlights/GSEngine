export module gse.gpu:render_pass;

import std;

import gse.core;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.meta;

import :types;
import :render_graph;

export namespace gse::gpu {
	struct color_attachment {
		load_op op = load_op::clear;
		gpu::color_clear clear{};
	};

	struct depth_attachment {
		load_op op = load_op::clear;
		gpu::depth_clear clear{};
	};

	struct render_pass_descriptor {
		id pass_kind{};
		queue_type queue = queue_type::graphics;
		const pipeline* primary_pipeline = nullptr;
		std::optional<color_attachment> color;
		std::optional<depth_attachment> depth;
		std::vector<id> after_deps;
		id chain_id;
	};

	struct [[= same_frame_channel]] render_pass_request {
		render_pass_descriptor desc;
		std::coroutine_handle<> record_handle;
		std::optional<recording_context>* record_ctx_slot = nullptr;
		pass_body_fn body;
	};

	class request_pass_awaitable {
	public:
		request_pass_awaitable(
			const frame_context& ctx,
			render_pass_descriptor desc
		) noexcept;

		request_pass_awaitable(
			const request_pass_awaitable&
		) = delete;

		auto operator=(
			const request_pass_awaitable&
		) -> request_pass_awaitable& = delete;

		request_pass_awaitable(
			request_pass_awaitable&&
		) noexcept = default;

		auto operator=(
			request_pass_awaitable&&
		) noexcept -> request_pass_awaitable& = default;

		auto await_ready(
		) const noexcept -> bool;

		auto await_suspend(
			std::coroutine_handle<> h
		) noexcept -> void;

		auto await_resume(
		) noexcept -> recording_context;

	private:
		const frame_context* m_ctx;
		render_pass_descriptor m_desc;
		std::optional<recording_context> m_rec;
		id m_trace_id{};
		std::uint64_t m_trace_key = 0;
	};

	class pass_builder {
	public:
		pass_builder(
			const frame_context& ctx,
			id pass_kind
		) noexcept;

		auto on(
			queue_type queue
		) && -> pass_builder&&;

		auto pipeline(
			const gpu::pipeline& p
		) && -> pass_builder&&;

		auto color(
			color_attachment value
		) && -> pass_builder&&;

		auto depth(
			depth_attachment value
		) && -> pass_builder&&;

		template <typename Chain>
		auto in_chain() && -> pass_builder&&;

		template <typename... States>
		auto after() && -> pass_builder&&;

		template <typename F>
		auto record(
			F&& body
		) && -> void;

		auto operator co_await(
		) && -> request_pass_awaitable;

	private:
		const frame_context* m_ctx;
		render_pass_descriptor m_desc;
	};

	[[nodiscard]] auto pass(
		const frame_context& ctx,
		id pass_kind
	) -> pass_builder;

	template <typename Owner>
	[[nodiscard]] auto pass(
		const frame_context& ctx
	) -> pass_builder;

	auto clear_color(
		gpu::color_clear value
	) -> color_attachment;

	auto load_color(
	) -> color_attachment;

	auto clear_depth(
		gpu::depth_clear value
	) -> depth_attachment;

	auto load_depth(
	) -> depth_attachment;
}

template <typename... States>
auto gse::gpu::pass_builder::after() && -> pass_builder&& {
	(m_desc.after_deps.push_back(trace_id<States>()), ...);
	return std::move(*this);
}

template <typename Chain>
auto gse::gpu::pass_builder::in_chain() && -> pass_builder&& {
	m_desc.chain_id = trace_id<Chain>();
	return std::move(*this);
}

template <typename F>
auto gse::gpu::pass_builder::record(F&& body) && -> void {
	m_ctx->channels.push<render_pass_request>({
		.desc = std::move(m_desc),
		.body = std::make_shared<move_only_function<void(recording_context&)>>(std::forward<F>(body)),
	});
}

template <typename Owner>
auto gse::gpu::pass(const frame_context& ctx) -> pass_builder {
	return pass_builder{ ctx, trace_id<Owner>() };
}
