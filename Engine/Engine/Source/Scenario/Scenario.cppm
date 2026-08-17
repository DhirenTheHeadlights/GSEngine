export module gse.scenario;

import std;

import gse.core;
import gse.math;
import gse.time;
import gse.meta;
import gse.ecs;
import gse.concurrency;

export namespace gse::scenario {
	struct info {
		char name[64];
		char scene[64] = "";
		bool headless = false;
		bool gpu_solver = false;
		int warmup_frames = 120;
		int frames = 600;
		char settings[8][96] = {};
	};

	struct wait_gate {
		std::uint64_t frames_remaining = 0;
		bool until_settled = false;
		bool armed = false;
		async::checked_handle resume{};
	};

	class context {
	public:
		context(
			channel_writer writer,
			wait_gate* gate
		);

		[[nodiscard]] auto frame() const -> std::uint64_t;

		auto channels() -> channel_writer&;

		auto advance_frame() -> void;

		[[nodiscard]] auto gate() const -> wait_gate*;

	private:
		channel_writer m_writer;
		wait_gate* m_gate = nullptr;
		std::uint64_t m_frame = 0;
	};

	struct wait_awaitable {
		wait_gate* gate = nullptr;
		std::uint64_t frames = 0;
		bool until_settled = false;

		[[nodiscard]] auto await_ready() const noexcept -> bool;

		auto await_suspend(
			std::coroutine_handle<> h
		) const -> void;

		auto await_resume() const noexcept -> void;
	};

	auto wait_frames(
		context& ctx,
		std::uint64_t count
	) -> wait_awaitable;

	auto wait(
		context& ctx,
		time duration
	) -> wait_awaitable;

	auto wait_settled(
		context& ctx
	) -> wait_awaitable;

	using body_fn = auto (*)(context&) -> async::task<>;

	struct entry {
		id id;
		info info;
		body_fn body = nullptr;
	};

	template <std::meta::info Ns>
	auto registry() -> std::span<const entry>;

	auto find(
		std::span<const entry> table,
		std::string_view name
	) -> const entry*;
}

namespace gse::scenario {
	template <std::meta::info Ns>
	consteval auto scenario_members() -> std::vector<std::meta::info>;
}

gse::scenario::context::context(channel_writer writer, wait_gate* gate)
	: m_writer(writer), m_gate(gate) {
}

auto gse::scenario::context::frame() const -> std::uint64_t {
	return m_frame;
}

auto gse::scenario::context::channels() -> channel_writer& {
	return m_writer;
}

auto gse::scenario::context::advance_frame() -> void {
	++m_frame;
}

auto gse::scenario::context::gate() const -> wait_gate* {
	return m_gate;
}

auto gse::scenario::wait_awaitable::await_ready() const noexcept -> bool {
	return gate == nullptr;
}

auto gse::scenario::wait_awaitable::await_suspend(const std::coroutine_handle<> h) const -> void {
	gate->frames_remaining = frames;
	gate->until_settled = until_settled;
	gate->resume = async::track_frame(h);
	gate->armed = true;
}

auto gse::scenario::wait_awaitable::await_resume() const noexcept -> void {
}

auto gse::scenario::wait_frames(context& ctx, const std::uint64_t count) -> wait_awaitable {
	return {
		.gate = ctx.gate(),
		.frames = count,
	};
}

auto gse::scenario::wait(context& ctx, const time duration) -> wait_awaitable {
	const auto step = system_clock::fixed_dt();
	const auto count = step > time{} ? static_cast<std::uint64_t>(duration / step + 0.5) : 0;
	return wait_frames(ctx, count);
}

auto gse::scenario::wait_settled(context& ctx) -> wait_awaitable {
	return {
		.gate = ctx.gate(),
		.until_settled = true,
	};
}

template <std::meta::info Ns>
consteval auto gse::scenario::scenario_members() -> std::vector<std::meta::info> {
	std::vector<std::meta::info> found;
	for (const auto m : std::meta::members_of(Ns, std::meta::access_context::unchecked())) {
		if (std::meta::is_function(m) && has_annotation<scenario::info>(m)) {
			found.push_back(m);
		}
	}
	return found;
}

template <std::meta::info Ns>
auto gse::scenario::registry() -> std::span<const entry> {
	static const std::vector<entry> table = [] {
		std::vector<entry> built;
		template for (constexpr auto m : std::define_static_array(scenario_members<Ns>())) {
			constexpr auto ann = first_annotation_of_type(m, ^^scenario::info);
			static constexpr scenario::info declared = [:std::meta::constant_of(ann):];
			built.push_back({
				.id = find_or_generate_id(std::string_view(declared.name)),
				.info = declared,
				.body = &[:m:],
			});
		}
		return built;
	}();
	return table;
}

auto gse::scenario::find(const std::span<const entry> table, const std::string_view name) -> const entry* {
	const auto it = std::ranges::find_if(
		table,
		[name](const entry& e) {
			return std::string_view(e.info.name) == name;
		}
	);
	return it != table.end() ? &*it : nullptr;
}
