export module gse.tests.math;

import std;

import gse.math;
import gse.meta;
import gse.test;

export namespace gse::tests::math {
	[[= test::unit{ .tags = "math" }]]
	auto dot_is_commutative(
		test::context& ctx
	) -> void;

	[[= test::unit{ .tags = "math" }]]
	auto inverse_times_original_is_identity(
		test::context& ctx
	) -> void;

	[[= test::unit{ .tags = "math" }]]
	auto quaternion_matches_its_matrix(
		test::context& ctx
	) -> void;

	[[= test::unit{ .tags = "math" }]]
	auto degenerate_input_returns_zero(
		test::context& ctx
	) -> void;

	[[= test::unit{ .tags = "math" }]]
	auto vector_text_round_trips(
		test::context& ctx
	) -> void;
}

auto gse::tests::math::dot_is_commutative(test::context& ctx) -> void {
	const vec3f a{ 1.f, 2.f, 3.f };
	const vec3f b{ 4.f, 5.f, 6.f };
	ctx.expect_eq(dot(a, b), dot(b, a));

	const vec3<length> scaled{ meters(1.f), meters(2.f), meters(3.f) };
	ctx.expect_near(dot(scaled, b), dot(b, scaled), millimeters(1e-2f));
}

auto gse::tests::math::inverse_times_original_is_identity(test::context& ctx) -> void {
	const mat3f m{ vec3f{ 1.f, 2.f, 0.f }, vec3f{ 0.f, 1.f, 3.f }, vec3f{ 2.f, 0.f, 1.f } };
	const mat3f identity = m.inverse() * m;
	const mat3f expected(1.f);

	for (std::size_t column = 0; column < mat3f::extent_cols; ++column) {
		for (std::size_t row = 0; row < mat3f::extent_rows; ++row) {
			ctx.expect_near(identity[column][row], expected[column][row], 1e-4f);
		}
	}
}

auto gse::tests::math::quaternion_matches_its_matrix(test::context& ctx) -> void {
	const quat q = from_axis_angle(normalize(vec3f{ 1.f, 2.f, 3.f }), degrees(37.f));
	const mat3f cast = mat3_cast(q);
	const mat3f direct(q);

	for (std::size_t column = 0; column < mat3f::extent_cols; ++column) {
		for (std::size_t row = 0; row < mat3f::extent_rows; ++row) {
			ctx.expect_near(cast[column][row], direct[column][row], 1e-5f);
		}
	}
}

auto gse::tests::math::degenerate_input_returns_zero(test::context& ctx) -> void {
	const vec3f zero{};
	const vec3f a{ 1.f, 2.f, 3.f };

	ctx.expect_eq(normalize(zero), zero);
	ctx.expect_eq(project(a, zero), zero);
	ctx.expect_eq(angle_between(a, zero), radians(0.f));
}

auto gse::tests::math::vector_text_round_trips(test::context& ctx) -> void {
	const auto round_trips = [&ctx]<typename V>(const V& value) {
		V parsed{};
		if (ctx.expect(parse(std::format("{}", value), parsed))) {
			ctx.expect_eq(parsed, value);
		}
	};

	round_trips(vec3f{ 1.5f, -2.f, 0.25f });
	round_trips(vec2i{ 3, -4 });
	round_trips(vec3<length>{ meters(1.5f), meters(-2.f), meters(0.25f) });
	round_trips(vec3<length_t<float, kilometers>>{ kilometers(1.5f), kilometers(-2.f), kilometers(0.25f) });
	round_trips(vec2<angle>{ radians(0.5f), radians(-1.25f) });

	vec2<length> mixed{};
	if (ctx.expect(parse("(2 km, 3 m)", mixed))) {
		ctx.expect_eq(mixed, vec2<length>{ kilometers(2.f), meters(3.f) });
	}

	vec3<length> rejected{};
	ctx.expect(!parse("(1, 2, 3)", rejected));
	ctx.expect(!parse("(1 m, 2 parsecs, 3 m)", rejected));
	ctx.expect(!parse("(1 m, 2 m)", rejected));
	ctx.expect(!parse("(1 m, 2 m, 3 m, 4 m)", rejected));
}
