export module gse.gpu_record:render_pass;

import std;

import gse.core;
import gse.concurrency;
import gse.diag;
import gse.ecs;

import gse.gpu;
import :recording_context;

export namespace gse::gpu {
	struct color_attachment {
		load_op op = load_op::clear;
		gpu::color_clear clear{};
		const image* target = nullptr;
		transient_image_handle transient_target;
	};

	struct depth_attachment {
		load_op op = load_op::clear;
		gpu::depth_clear clear{};
		const image* target = nullptr;
		transient_image_handle transient_target;
	};

	struct render_pass_descriptor {
		id pass_kind{};
		std::string_view pass_name{};
		id trace_kind{};
		queue_type queue = queue_type::graphics;
		const shader_program* primary_pipeline = nullptr;
		std::vector<color_attachment> colors;
		std::optional<depth_attachment> depth;
		std::vector<id> after_deps;
		id chain_id;
	};

	struct [[= same_frame_channel]] render_pass_request {
		render_pass_descriptor desc;
		std::coroutine_handle<> record_handle;
		recording_context_init* record_ctx_slot = nullptr;
	};

	class request_pass_awaitable : non_copyable {
	public:
		request_pass_awaitable(
			const gse::context& ctx,
			render_pass_descriptor desc
		) noexcept;

		request_pass_awaitable(
			request_pass_awaitable&&
		) noexcept = default;

		auto operator=(
			request_pass_awaitable&&
		) noexcept -> request_pass_awaitable& = default;

		auto await_ready() const noexcept -> bool;

		auto await_suspend(
			std::coroutine_handle<> h
		) noexcept -> void;

		[[nodiscard]] auto await_resume() noexcept -> recording_context;

	private:
		const gse::context* m_ctx;
		render_pass_descriptor m_desc;
		recording_context_init m_init;
		id m_trace_id{};
		std::uint64_t m_trace_key = 0;
	};

	class pass_builder {
	public:
		pass_builder(
			const gse::context& ctx,
			id pass_kind,
			std::string_view pass_name = {},
			id trace_kind = {}
		) noexcept;

		auto on(
			queue_type queue
		) && -> pass_builder&&;

		auto pipeline(
			const gpu::shader_program& p
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

		template <std::meta::info... Hooks>
		auto after() && -> pass_builder&&;

		auto operator co_await() && -> request_pass_awaitable;

	private:
		const gse::context* m_ctx;
		render_pass_descriptor m_desc;
	};

	[[nodiscard]] auto pass(
		const gse::context& ctx,
		id pass_kind
	) -> pass_builder;

	template <typename Owner>
	[[nodiscard]] auto pass(
		const gse::context& ctx
	) -> pass_builder;

	template <std::meta::info Hook>
	[[nodiscard]] auto pass(
		const gse::context& ctx
	) -> pass_builder;

	auto clear_color(
		gpu::color_clear value
	) -> color_attachment;

	auto clear_color(
		gpu::color_clear value,
		const image& target
	) -> color_attachment;

	auto load_color() -> color_attachment;

	auto load_color(
		const image& target
	) -> color_attachment;

	auto clear_depth(
		gpu::depth_clear value
	) -> depth_attachment;

	auto load_depth() -> depth_attachment;
}

export namespace gse::gpu::context {
	auto execute_frame(
		data& d,
		scheduler& s
	) -> void;
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

template <typename Owner>
auto gse::gpu::pass(const gse::context& ctx) -> pass_builder {
	static const id record_cached = find_or_generate_id(std::format("record<{}>", type_tag<Owner>()));
	return pass_builder{ ctx, trace_id<Owner>(), type_tag<Owner>(), record_cached };
}

namespace gse::gpu {
	template <std::meta::info Hook>
	consteval auto hook_name() -> std::string_view {
		if constexpr (std::meta::is_type(Hook)) {
			return type_tag<typename [:Hook:]>();
		}
		else {
			auto walk = [](this auto self, std::meta::info entity) consteval -> std::string {
				std::string own(std::meta::identifier_of(entity));
				const auto parent = std::meta::parent_of(entity);
				if (!std::meta::has_identifier(parent)) {
					return own;
				}
				return self(parent) + "::" + own;
			};
			return std::define_static_string(walk(Hook));
		}
	}

}

template <std::meta::info... Hooks>
auto gse::gpu::pass_builder::after() && -> pass_builder&& {
	static constexpr std::array<std::string_view, sizeof...(Hooks)> names{ hook_name<Hooks>()... };
	for (const auto pass_name : names) {
		m_desc.after_deps.push_back(find_or_generate_id(pass_name));
	}
	return std::move(*this);
}

template <std::meta::info Hook>
auto gse::gpu::pass(const gse::context& ctx) -> pass_builder {
	static constexpr std::string_view name = hook_name<Hook>();
	static const id cached = find_or_generate_id(name);
	static const id record_cached = find_or_generate_id(std::format("record<{}>", name));
	return pass_builder{ ctx, cached, name, record_cached };
}
