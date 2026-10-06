export module gse.test;

import std;

import gse.meta;

export namespace gse::test {
	struct unit {
		char tags[64] = "";
		bool needs_gpu = false;
		bool isolated = false;
	};

	struct violation {
		char expect[128] = "";
	};

	struct failure {
		std::source_location location;
		std::string message;
	};

	class context {
	public:
		explicit context(
			std::string_view name
		);

		[[nodiscard]] auto name() const -> std::string_view;

		[[nodiscard]] auto failures() const -> std::span<const failure>;

		auto expect(
			bool condition,
			std::source_location loc = std::source_location::current()
		) -> bool;

		template <std::formattable<char> T, std::formattable<char> U>
		auto expect_eq(
			const T& actual,
			const U& expected,
			std::source_location loc = std::source_location::current()
		) -> bool;

		template <std::formattable<char> T, std::formattable<char> U, std::formattable<char> V>
		auto expect_near(
			const T& actual,
			const U& expected,
			const V& tolerance,
			std::source_location loc = std::source_location::current()
		) -> bool;

		template <typename T, std::formattable<char> E>
		auto expect_value(
			const std::expected<T, E>& result,
			std::source_location loc = std::source_location::current()
		) -> bool;

		template <typename T, std::formattable<char> E>
		auto expect_error(
			const std::expected<T, E>& result,
			std::source_location loc = std::source_location::current()
		) -> bool;

	private:
		auto record(
			std::string message,
			const std::source_location& loc
		) -> bool;

		std::string_view m_name;
		std::vector<failure> m_failures;
	};

	using body_fn = auto (*)(context&) -> void;

	enum class kind : std::uint8_t {
		in_process,
		violation
	};

	struct entry {
		std::string_view name;
		kind mode = kind::in_process;
		unit spec;
		violation expected;
		body_fn body = nullptr;
	};

	template <std::meta::info Ns>
	auto registry() -> std::span<const entry>;

	struct config {
		std::string filter;
		std::string tags;
		std::string only;
		bool all = false;
		bool list = false;

		[[= at_least<1>{}]]
		int repeat = 1;
	};

	struct request {
		std::span<const std::span<const entry>> tables;
		const config& options;
		std::string_view flag_prefix = "--test";
	};

	auto requested(
		const config& options
	) -> bool;

	auto run(
		const request& req
	) -> int;
}

namespace gse::test {
	consteval auto collect_members(
		std::meta::info ns,
		std::vector<std::meta::info>& out
	) -> void;

	template <std::meta::info Ns>
	consteval auto test_members() -> std::vector<std::meta::info>;
}

template <std::formattable<char> T, std::formattable<char> U>
auto gse::test::context::expect_eq(const T& actual, const U& expected, const std::source_location loc) -> bool {
	if (actual == expected) {
		return true;
	}
	return record(std::format("expected {}, got {}", expected, actual), loc);
}

template <std::formattable<char> T, std::formattable<char> U, std::formattable<char> V>
auto gse::test::context::expect_near(const T& actual, const U& expected, const V& tolerance, const std::source_location loc) -> bool {
	const auto deviation = actual < expected ? expected - actual : actual - expected;
	if (!(deviation > tolerance)) {
		return true;
	}
	return record(std::format("expected {} within {}, got {} (off by {})", expected, tolerance, actual, deviation), loc);
}

template <typename T, std::formattable<char> E>
auto gse::test::context::expect_value(const std::expected<T, E>& result, const std::source_location loc) -> bool {
	if (result) {
		return true;
	}
	return record(std::format("expected a value, got error {}", result.error()), loc);
}

template <typename T, std::formattable<char> E>
auto gse::test::context::expect_error(const std::expected<T, E>& result, const std::source_location loc) -> bool {
	if (!result) {
		return true;
	}
	return record("expected an error, got a value", loc);
}

consteval auto gse::test::collect_members(const std::meta::info ns, std::vector<std::meta::info>& out) -> void {
	for (const auto m : std::meta::members_of(ns, std::meta::access_context::unchecked())) {
		if (std::meta::is_namespace(m)) {
			collect_members(m, out);
		}
		else if (std::meta::is_function(m) && (has_annotation<unit>(m) || has_annotation<violation>(m))) {
			out.push_back(m);
		}
	}
}

template <std::meta::info Ns>
consteval auto gse::test::test_members() -> std::vector<std::meta::info> {
	std::vector<std::meta::info> found;
	collect_members(Ns, found);
	return found;
}

template <std::meta::info Ns>
auto gse::test::registry() -> std::span<const entry> {
	static const std::vector<entry> table = [] {
		std::vector<entry> built;
		template for (constexpr auto m : std::define_static_array(test_members<Ns>())) {
			constexpr std::string_view name = std::define_static_string(std::meta::identifier_of(m));
			constexpr auto unit_annotation = first_annotation_of_type(m, ^^unit);
			if constexpr (unit_annotation != std::meta::info{}) {
				static constexpr unit declared = [:std::meta::constant_of(unit_annotation):];
				built.push_back({
					.name = name,
					.mode = kind::in_process,
					.spec = declared,
					.body = &[:m:],
				});
			}
			else {
				static constexpr violation declared = [:std::meta::constant_of(first_annotation_of_type(m, ^^violation)):];
				built.push_back({
					.name = name,
					.mode = kind::violation,
					.expected = declared,
					.body = &[:m:],
				});
			}
		}
		return built;
	}();
	return table;
}
