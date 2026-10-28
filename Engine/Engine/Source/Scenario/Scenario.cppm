export module gse.scenario;

import std;

import gse.assert;
import gse.core;
import gse.math;
import gse.time;
import gse.meta;
import gse.ecs;
import gse.concurrency;
import gse.test;

export namespace gse::scenario {
	struct info {
		char name[64];
		char scene[64] = "";
		bool headless = false;
		bool gpu_solver = false;
		bool backend_variants = false;
		bool video_encode = false;
		bool real_time = false;
		int warmup_frames = 120;
		int frames = 600;
		char settings[16][96] = {};
	};

	struct wait_gate {
		std::uint64_t frames_remaining = 0;
		bool until_settled = false;
		bool armed = false;
		async::checked_handle resume{};
	};

	class context : public test::check_sink {
	public:
		context(
			channel_writer writer,
			wait_gate* gate,
			gse::registry* reg,
			test::report_fn report = nullptr
		);

		[[nodiscard]] auto frame() const -> std::uint64_t;

		auto channels() -> channel_writer&;

		template <typename C>
		[[nodiscard]] auto owners() const -> std::span<const id>;

		template <typename C>
		[[nodiscard]] auto components() const -> std::span<const C>;

		template <typename C>
		[[nodiscard]] auto try_component(
			id owner
		) const -> const C*;

		auto advance_frame() -> void;

		[[nodiscard]] auto gate() const -> wait_gate*;

	private:
		channel_writer m_writer;
		wait_gate* m_gate = nullptr;
		gse::registry* m_registry = nullptr;
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

	[[nodiscard]] auto backend_variant_of(
		const info& declared,
		std::string_view suffix,
		bool gpu_solver
	) -> info;
}

auto gse::scenario::backend_variant_of(const info& declared, const std::string_view suffix, const bool gpu_solver) -> info {
	const std::string_view stem(declared.name);
	assert(stem.size() + suffix.size() < std::size(declared.name), "scenario '{}' with suffix '{}' exceeds the {} character name limit", stem, suffix, std::size(declared.name) - 1);

	info out = declared;
	out.gpu_solver = gpu_solver;
	out.backend_variants = false;

	auto* cursor = std::ranges::copy(stem, std::begin(out.name)).out;
	cursor = std::ranges::copy(suffix, cursor).out;
	*cursor = '\0';

	return out;
}

gse::scenario::context::context(channel_writer writer, wait_gate* gate, gse::registry* reg, const test::report_fn report)
	: check_sink(report), m_writer(writer), m_gate(gate), m_registry(reg) {
}

auto gse::scenario::context::frame() const -> std::uint64_t {
	return m_frame;
}

auto gse::scenario::context::channels() -> channel_writer& {
	return m_writer;
}

template <typename C>
auto gse::scenario::context::owners() const -> std::span<const id> {
	return m_registry ? m_registry->owner_ids<C>() : std::span<const id>{};
}

template <typename C>
auto gse::scenario::context::components() const -> std::span<const C> {
	return m_registry ? std::span<const C>(m_registry->components<C>()) : std::span<const C>{};
}

template <typename C>
auto gse::scenario::context::try_component(const id owner) const -> const C* {
	return m_registry ? m_registry->try_component<C>(owner) : nullptr;
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
		if (std::meta::is_function(m) && has_annotation<info>(m)) {
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
			constexpr auto ann = first_annotation_of_type(m, ^^info);
			static constexpr info declared = [:std::meta::constant_of(ann):];
			if constexpr (declared.backend_variants) {
				constexpr std::array<std::pair<std::string_view, bool>, 2> variants = {
					std::pair{ std::string_view("_cpu"), false },
					std::pair{ std::string_view("_gpu"), true },
				};
				for (const auto& [suffix, gpu_solver] : variants) {
					const info resolved = backend_variant_of(declared, suffix, gpu_solver);
					built.push_back({
						.id = find_or_generate_id(std::string_view(resolved.name)),
						.info = resolved,
						.body = &[:m:],
					});
				}
			}
			else {
				built.push_back({
					.id = find_or_generate_id(std::string_view(declared.name)),
					.info = declared,
					.body = &[:m:],
				});
			}
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