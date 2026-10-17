export module gse.tests.complete;

import std;

import gse.complete;
import gse.syntax;
import gse.test;

export namespace gse::tests::complete {
	[[= test::unit{ .tags = "complete" }]]
	auto fuzzy_prefers_word_boundaries(
		test::context& ctx
	) -> void;

	[[= test::unit{ .tags = "complete" }]]
	auto model_records_round_trip(
		test::context& ctx
	) -> void;

	[[= test::unit{ .tags = "complete" }]]
	auto module_exports_are_namespaced(
		test::context& ctx
	) -> void;

	[[= test::unit{ .tags = "complete" }]]
	auto context_reads_member_chain(
		test::context& ctx
	) -> void;

	[[= test::unit{ .tags = "complete" }]]
	auto context_is_silent_in_literals(
		test::context& ctx
	) -> void;

	[[= test::unit{ .tags = "complete" }]]
	auto context_finds_call_argument(
		test::context& ctx
	) -> void;

	[[= test::unit{ .tags = "complete" }]]
	auto member_completion_follows_types(
		test::context& ctx
	) -> void;

	[[= test::unit{ .tags = "complete" }]]
	auto lexical_completion_uses_buffer_and_usings(
		test::context& ctx
	) -> void;

	[[= test::unit{ .tags = "complete" }]]
	auto signature_tracks_argument(
		test::context& ctx
	) -> void;

	[[= test::unit{ .tags = "complete" }]]
	auto session_opens_refines_and_accepts(
		test::context& ctx
	) -> void;

	[[= test::unit{ .tags = "complete" }]]
	auto session_ignores_its_own_accept(
		test::context& ctx
	) -> void;

	[[= test::unit{ .tags = "complete" }]]
	auto exact_match_outranks_longer_words(
		test::context& ctx
	) -> void;

	[[= test::unit{ .tags = "complete" }]]
	auto matching_case_ranks_within_prefix_tier(
		test::context& ctx
	) -> void;

	[[= test::unit{ .tags = "complete" }]]
	auto hover_describes_entities(
		test::context& ctx
	) -> void;
}

namespace gse::tests::complete {
	constexpr std::string_view sample_records =
		"GSECTYPE\tpoint\n"
		"GSECMEMBER\tpoint\tx\tfield\t\tint\t0\tint\t\n"
		"GSECMEMBER\tpoint\tlength\tmethod\tc\tdouble\t0\tdouble\t\n"
		"GSECMEMBER\tpoint\tsecret\tfield\tp\tint\t0\tint\t\n"
		"GSECTYPE\tshape\n"
		"GSECMEMBER\tshape\tcorner\tmethod\t\tpoint\t0\tpoint\tint\x1eindex\n"
		"GSECMEMBER\tshape\tparent\tfield\t\tshape\t1\tshape *\t\n"
		"GSECSCOPE\t0\t-1\tname_space\t1\t1\t40\t1\t\t\t0\t\n"
		"GSECSCOPE\t1\t0\tfunction\t3\t13\t20\t1\t\tint\t0\tint\n"
		"GSECDECL\t1\ts\tvariable\t4\t8\tshape\t0\tshape\t\n"
		"GSECDECL\t1\tlater\tvariable\t9\t6\tint\t0\tint\t\n"
		"GSECNS\t\tscale\tfunction\tpoint\t0\tpoint\tconst point&\x1ep\x1f" "double\x1e" "factor\n";

	auto sample_lines() -> std::vector<std::string_view>;

	auto labels_of(
		const gse::complete::completion& result
	) -> std::vector<std::string>;

	auto contains(
		const std::vector<std::string>& labels,
		std::string_view label
	) -> bool;

	auto kind_of(
		const gse::complete::completion& result,
		std::string_view label
	) -> std::optional<gse::complete::entity_kind>;
}

auto gse::tests::complete::sample_lines() -> std::vector<std::string_view> {
	static constexpr std::array lines = std::to_array<std::string_view>({
		"",
		"",
		"int main() {",
		"    shape s;",
		"    s.corner(0).;",
		"    s.parent->;",
		"    scale(s.corner(1), );",
		"    ",
		"    int later = 0;",
		"}",
	});
	return { lines.begin(), lines.end() };
}

auto gse::tests::complete::labels_of(const gse::complete::completion& result) -> std::vector<std::string> {
	std::vector<std::string> out;
	for (const gse::complete::candidate& c : result.items) {
		out.push_back(c.label);
	}
	return out;
}

auto gse::tests::complete::contains(const std::vector<std::string>& labels, const std::string_view label) -> bool {
	return std::ranges::find(labels, label) != labels.end();
}

auto gse::tests::complete::kind_of(const gse::complete::completion& result, const std::string_view label) -> std::optional<gse::complete::entity_kind> {
	const auto found = std::ranges::find(result.items, label, &gse::complete::candidate::label);
	return found == result.items.end() ? std::nullopt : std::optional(found->kind);
}

auto gse::tests::complete::fuzzy_prefers_word_boundaries(test::context& ctx) -> void {
	const auto boundary = gse::complete::fuzzy_match("pb", "push_back", "push_back");
	const auto inner = gse::complete::fuzzy_match("pb", "capable", "capable");
	ctx.expect(boundary.matched);
	ctx.expect(inner.matched);
	ctx.expect(boundary.score > inner.score);
	ctx.expect(!gse::complete::fuzzy_match("xyz", "push_back", "push_back").matched);
}

auto gse::tests::complete::model_records_round_trip(test::context& ctx) -> void {
	const gse::complete::model m = gse::complete::parse_model(sample_records);
	ctx.expect_eq(m.types.size(), 2uz);
	ctx.expect_eq(m.scopes.size(), 2uz);
	ctx.expect_eq(m.declarations.size(), 2uz);

	const gse::complete::type_entry* point = m.find_type("point");
	if (!ctx.expect(point != nullptr)) {
		return;
	}
	ctx.expect_eq(point->members.size(), 3uz);
	ctx.expect(point->members[1].is_const);
	ctx.expect(point->members[2].access == gse::complete::member_access::owner);

	const gse::complete::type_entry* shape = m.find_type("shape");
	if (!ctx.expect(shape != nullptr)) {
		return;
	}
	ctx.expect_eq(shape->members[0].what.params.size(), 1uz);
	ctx.expect_eq(shape->members[0].what.params[0].name, std::string("index"));
	ctx.expect_eq(static_cast<int>(shape->members[1].what.type.indirection), 1);

	ctx.expect_eq(m.namespace_members("").size(), 1uz);
	ctx.expect_eq(m.namespace_entries[0].what.params.size(), 2uz);

	const std::optional<std::uint32_t> inner = m.innermost_scope({ .line = 5, .column = 4 });
	ctx.expect(inner == std::optional<std::uint32_t>(1));
	ctx.expect(m.scopes[1].parent == std::optional<std::uint32_t>(0));
}

auto gse::tests::complete::module_exports_are_namespaced(test::context& ctx) -> void {
	constexpr std::string_view source =
		"module;\n"
		"#include <vector>\n"
		"export module std;\n"
		"export namespace std {\n"
		"  using std::vector;\n"
		"#if __cpp_lib_ranges\n"
		"  namespace ranges {\n"
		"    using std::ranges::sort;\n"
		"  }\n"
		"  using ranges::contains;\n"
		"#endif\n"
		"  using namespace literals;\n"
		"  using std::operator==;\n"
		"}\n";
	const gse::complete::model m = gse::complete::parse_module_exports(source);

	auto names_in = [&](const std::string_view name_space) {
		std::vector<std::string> out;
		for (const std::uint32_t index : m.namespace_members(name_space)) {
			out.push_back(m.namespace_entries[index].what.name);
		}
		std::ranges::sort(out);
		return out;
	};

	ctx.expect(names_in("std") == std::vector<std::string>{ "contains", "ranges", "vector" });
	ctx.expect(names_in("std::ranges") == std::vector<std::string>{ "sort" });
	ctx.expect(names_in("") == std::vector<std::string>{ "std" });
}

auto gse::tests::complete::context_reads_member_chain(test::context& ctx) -> void {
	const std::vector<std::string_view> lines = sample_lines();

	const gse::complete::context dot = gse::complete::context_at(lines, { .line = 4, .column = 16 });
	ctx.expect(dot.kind == gse::complete::trigger::member);
	ctx.expect(dot.op == gse::complete::access_op::dot);
	if (ctx.expect_eq(dot.receiver.size(), 2uz)) {
		ctx.expect_eq(dot.receiver[0].name, std::string("s"));
		ctx.expect_eq(dot.receiver[1].name, std::string("corner"));
		ctx.expect(dot.receiver[1].postfixes == std::vector{ gse::complete::postfix::call });
	}

	const gse::complete::context arrow = gse::complete::context_at(lines, { .line = 5, .column = 14 });
	ctx.expect(arrow.op == gse::complete::access_op::arrow);
	ctx.expect_eq(arrow.receiver.size(), 2uz);

	const gse::complete::context typed = gse::complete::context_at(lines, { .line = 3, .column = 7 });
	ctx.expect(typed.kind == gse::complete::trigger::identifier);
	ctx.expect_eq(typed.prefix, std::string("sha"));
	ctx.expect_eq(typed.replace_from.column, 4u);
	ctx.expect_eq(typed.replace_to.column, 9u);

	const std::array scoped = std::to_array<std::string_view>({ "std::ranges::so" });
	const gse::complete::context scope = gse::complete::context_at(scoped, { .line = 0, .column = 15 });
	ctx.expect(scope.kind == gse::complete::trigger::scope);
	ctx.expect_eq(scope.prefix, std::string("so"));
	ctx.expect_eq(scope.receiver.size(), 2uz);
}

auto gse::tests::complete::context_is_silent_in_literals(test::context& ctx) -> void {
	const std::array lines = std::to_array<std::string_view>({
		"auto s = \"abc.",
		"int x; // note.",
		"#include <vec",
		"auto t = \"done\".",
		"/* open",
		"still.",
	});
	ctx.expect(gse::complete::context_at(lines, { .line = 0, .column = 14 }).kind == gse::complete::trigger::none);
	ctx.expect(gse::complete::context_at(lines, { .line = 1, .column = 15 }).kind == gse::complete::trigger::none);
	ctx.expect(gse::complete::context_at(lines, { .line = 2, .column = 13 }).kind == gse::complete::trigger::none);
	ctx.expect(gse::complete::context_at(lines, { .line = 3, .column = 16 }).kind == gse::complete::trigger::member);
	ctx.expect(gse::complete::context_at(lines, { .line = 5, .column = 6 }).kind == gse::complete::trigger::none);
}

auto gse::tests::complete::context_finds_call_argument(test::context& ctx) -> void {
	const std::vector<std::string_view> lines = sample_lines();
	const gse::complete::context c = gse::complete::context_at(lines, { .line = 6, .column = 23 });
	if (!ctx.expect(c.call.has_value())) {
		return;
	}
	ctx.expect_eq(c.call->argument, 1u);
	ctx.expect_eq(c.call->callee.back().name, std::string("scale"));
	ctx.expect_eq(c.call->open.column, 9u);
}

auto gse::tests::complete::member_completion_follows_types(test::context& ctx) -> void {
	const gse::complete::model m = gse::complete::parse_model(sample_records);
	const std::array<const gse::complete::model*, 1> models{ &m };
	const std::vector<std::string_view> lines = sample_lines();
	const gse::complete::usage_table usage;
	const gse::complete::sources from{
		.models = models,
		.lines = lines,
		.usage = usage,
	};

	const std::vector<std::string> through_call = labels_of(gse::complete::complete_at(from, { .line = 4, .column = 16 }));
	ctx.expect(contains(through_call, "x"));
	ctx.expect(contains(through_call, "length"));
	ctx.expect(!contains(through_call, "secret"));

	const std::vector<std::string> through_pointer = labels_of(gse::complete::complete_at(from, { .line = 5, .column = 14 }));
	ctx.expect(contains(through_pointer, "corner"));
	ctx.expect(contains(through_pointer, "parent"));

	const gse::complete::completion scoped = gse::complete::complete_at(from, { .line = 7, .column = 4 });
	ctx.expect(kind_of(scoped, "s") == gse::complete::entity_kind::variable);
	ctx.expect(kind_of(scoped, "scale") == gse::complete::entity_kind::function);
	ctx.expect(kind_of(scoped, "later") == gse::complete::entity_kind::word);
}

auto gse::tests::complete::lexical_completion_uses_buffer_and_usings(test::context& ctx) -> void {
	const gse::complete::model exports = gse::complete::parse_module_exports(
		"export module std;\n"
		"export namespace std {\n"
		"  using std::vector;\n"
		"  using std::views::iota;\n"
		"}\n"
	);
	const std::array<const gse::complete::model*, 1> models{ &exports };
	const std::array lines = std::to_array<std::string_view>({
		"using namespace std;",
		"int counter = 0;",
		"vec",
		"cou",
		"v.push_back(1); v.",
	});
	gse::complete::usage_table usage;
	const gse::complete::sources from{
		.models = models,
		.lines = lines,
		.usage = usage,
	};

	const gse::complete::completion vec = gse::complete::complete_at(from, { .line = 2, .column = 3 });
	ctx.expect(!vec.items.empty() && vec.items.front().label == "vector");

	const std::vector<std::string> cou = labels_of(gse::complete::complete_at(from, { .line = 3, .column = 3 }));
	ctx.expect(contains(cou, "counter"));

	const std::vector<std::string> members = labels_of(gse::complete::complete_at(from, { .line = 4, .column = 18 }));
	ctx.expect(contains(members, "push_back"));
	ctx.expect(!contains(members, "counter"));
}

auto gse::tests::complete::signature_tracks_argument(test::context& ctx) -> void {
	const gse::complete::model m = gse::complete::parse_model(sample_records);
	const std::array<const gse::complete::model*, 1> models{ &m };
	const std::vector<std::string_view> lines = sample_lines();
	const gse::complete::usage_table usage;
	const gse::complete::sources from{
		.models = models,
		.lines = lines,
		.usage = usage,
	};

	const std::optional<gse::complete::signature_help> help = gse::complete::signature_at(from, { .line = 6, .column = 23 });
	if (!ctx.expect(help.has_value()) || !ctx.expect_eq(help->overloads.size(), 1uz)) {
		return;
	}
	ctx.expect_eq(help->argument, 1u);
	const gse::complete::signature& s = help->overloads[0];
	ctx.expect_eq(s.label, std::string("scale(const point& p, double factor) -> point"));
	if (ctx.expect_eq(s.parameters.size(), 2uz)) {
		ctx.expect_eq(s.label.substr(s.parameters[1].start, s.parameters[1].length), std::string("double factor"));
	}
}

auto gse::tests::complete::session_opens_refines_and_accepts(test::context& ctx) -> void {
	const gse::complete::model m = gse::complete::parse_model(sample_records);
	const std::array<const gse::complete::model*, 1> models{ &m };
	const std::vector<std::string_view> lines = sample_lines();
	const gse::complete::usage_table usage;
	const gse::complete::sources from{
		.models = models,
		.lines = lines,
		.usage = usage,
	};

	gse::complete::session s;
	gse::complete::update(s, { .from = from, .caret = { .line = 4, .column = 16 }, .edited = true });
	ctx.expect(s.open);
	const std::vector<std::string> labels = labels_of(s.current);
	ctx.expect(contains(labels, "length"));

	gse::complete::update(s, { .from = from, .caret = { .line = 4, .column = 16 }, .generation = 1 });
	ctx.expect(s.open);

	const auto length = std::ranges::find(s.current.items, std::string("length"), &gse::complete::candidate::label);
	if (!ctx.expect(length != s.current.items.end())) {
		return;
	}
	const std::optional<gse::complete::accept_edit> edit = gse::complete::accept(s, static_cast<std::size_t>(length - s.current.items.begin()));
	if (ctx.expect(edit.has_value())) {
		ctx.expect_eq(edit->text, std::string("length("));
	}
	ctx.expect(!s.open);

	gse::complete::update(s, { .from = from, .caret = { .line = 4, .column = 16 }, .edited = true });
	gse::complete::update(s, { .from = from, .caret = { .line = 0, .column = 0 } });
	ctx.expect(!s.open);
}

auto gse::tests::complete::session_ignores_its_own_accept(test::context& ctx) -> void {
	const gse::complete::model m = gse::complete::parse_model(sample_records);
	const std::array<const gse::complete::model*, 1> models{ &m };
	const std::vector<std::string_view> lines = sample_lines();
	const gse::complete::usage_table usage;
	const gse::complete::sources from{
		.models = models,
		.lines = lines,
		.usage = usage,
	};

	gse::complete::session s;
	gse::complete::update(s, { .from = from, .caret = { .line = 4, .column = 16 }, .edited = true });
	const auto x = std::ranges::find(s.current.items, std::string("x"), &gse::complete::candidate::label);
	if (!ctx.expect(s.open) || !ctx.expect(x != s.current.items.end())) {
		return;
	}
	const std::optional<gse::complete::accept_edit> edit = gse::complete::accept(s, static_cast<std::size_t>(x - s.current.items.begin()));
	if (!ctx.expect(edit.has_value())) {
		return;
	}
	ctx.expect_eq(edit->text, std::string("x"));

	const std::array accepted = std::to_array<std::string_view>({
		"",
		"",
		"int main() {",
		"    shape s;",
		"    s.corner(0).x;",
	});
	const gse::complete::sources after{
		.models = models,
		.lines = accepted,
		.usage = usage,
	};
	gse::complete::update(s, { .from = after, .caret = { .line = 4, .column = 17 }, .edited = true });
	ctx.expect(!s.open);
}

auto gse::tests::complete::exact_match_outranks_longer_words(test::context& ctx) -> void {
	const std::array lines = std::to_array<std::string_view>({
		"int elsewhere = 0;",
		"elsewhere = elsewhere + elsewhere;",
		"else",
	});
	gse::complete::usage_table usage;
	gse::complete::count_words(lines, usage);
	const gse::complete::sources from{
		.models = {},
		.lines = lines,
		.usage = usage,
	};

	const gse::complete::completion result = gse::complete::complete_at(from, { .line = 2, .column = 4 });
	if (ctx.expect(!result.items.empty())) {
		ctx.expect_eq(result.items.front().label, std::string("else"));
	}
}

auto gse::tests::complete::matching_case_ranks_within_prefix_tier(test::context& ctx) -> void {
	const std::array lines = std::to_array<std::string_view>({
		"int SIZE = max_size;",
		"COUNT + counter;",
		"cou",
		"size",
	});
	gse::complete::usage_table usage;
	gse::complete::count_words(lines, usage);
	const gse::complete::sources from{
		.models = {},
		.lines = lines,
		.usage = usage,
	};

	auto ranked_above = [](const std::vector<std::string>& labels, const std::string_view higher, const std::string_view lower) {
		const auto a = std::ranges::find(labels, higher);
		const auto b = std::ranges::find(labels, lower);
		return a != labels.end() && b != labels.end() && a < b;
	};

	const std::vector<std::string> cou = labels_of(gse::complete::complete_at(from, { .line = 2, .column = 3 }));
	ctx.expect(ranked_above(cou, "counter", "COUNT"));

	const std::vector<std::string> size = labels_of(gse::complete::complete_at(from, { .line = 3, .column = 4 }));
	ctx.expect(ranked_above(size, "SIZE", "max_size"));
}

auto gse::tests::complete::hover_describes_entities(test::context& ctx) -> void {
	const gse::complete::model m = gse::complete::parse_model(sample_records);
	const std::array<const gse::complete::model*, 1> models{ &m };
	const std::vector<std::string_view> lines = sample_lines();
	const gse::complete::usage_table usage;
	const gse::complete::sources from{
		.models = models,
		.lines = lines,
		.usage = usage,
	};

	const std::optional<gse::complete::hover_info> local = gse::complete::describe_at(from, { .line = 4, .column = 4 });
	if (ctx.expect(local.has_value())) {
		ctx.expect(local->kind == gse::complete::entity_kind::variable);
		ctx.expect_eq(local->title, std::string("shape s"));
		ctx.expect_eq(local->detail, std::string("variable"));
		ctx.expect(local->from == gse::complete::position{ .line = 4, .column = 4 });
		ctx.expect(local->to == gse::complete::position{ .line = 4, .column = 5 });
	}

	const std::optional<gse::complete::hover_info> method = gse::complete::describe_at(from, { .line = 4, .column = 8 });
	if (ctx.expect(method.has_value())) {
		ctx.expect_eq(method->title, std::string("corner(int index) -> point"));
		ctx.expect_eq(method->detail, std::string("method of shape"));
	}

	const std::optional<gse::complete::hover_info> field = gse::complete::describe_at(from, { .line = 5, .column = 7 });
	if (ctx.expect(field.has_value())) {
		ctx.expect_eq(field->title, std::string("shape * parent"));
		ctx.expect_eq(field->detail, std::string("field of shape"));
	}

	const std::optional<gse::complete::hover_info> function = gse::complete::describe_at(from, { .line = 6, .column = 4 });
	if (ctx.expect(function.has_value())) {
		ctx.expect_eq(function->title, std::string("scale(const point& p, double factor) -> point"));
		ctx.expect_eq(function->detail, std::string("function"));
	}

	ctx.expect(!gse::complete::describe_at(from, { .line = 2, .column = 1 }).has_value());
	ctx.expect(!gse::complete::describe_at(from, { .line = 7, .column = 2 }).has_value());
	ctx.expect(!gse::complete::describe_at(from, { .line = 4, .column = 13 }).has_value());

	const std::array commented = std::to_array<std::string_view>({ "int x; // s" });
	const gse::complete::sources in_comment{
		.models = models,
		.lines = commented,
		.usage = usage,
	};
	ctx.expect(!gse::complete::describe_at(in_comment, { .line = 0, .column = 10 }).has_value());
}
