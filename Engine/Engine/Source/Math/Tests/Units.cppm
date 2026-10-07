export module gse.tests.units;

import std;

import gse.math;
import gse.test;

export namespace gse::tests::units {
	[[= test::unit{ .tags = "units" }]]
	auto conversions_round_trip(
		test::context& ctx
	) -> void;

	[[= test::unit{ .tags = "units" }]]
	auto dimension_products_hold_at_runtime(
		test::context& ctx
	) -> void;

	[[= test::unit{ .tags = "units" }]]
	auto sqrt_of_area_is_length(
		test::context& ctx
	) -> void;

	[[= test::unit{ .tags = "units" }]]
	auto mixed_unit_multiply_lands_at_the_right_scale(
		test::context& ctx
	) -> void;
}

auto gse::tests::units::conversions_round_trip(test::context& ctx) -> void {
	const length tolerance = millimeters(1e-3f);
	ctx.expect_near(kilometers(1.f), meters(1000.f), tolerance);
	ctx.expect_near(centimeters(250.f), meters(2.5f), tolerance);
	ctx.expect_near(millimeters(1500.f), meters(1.5f), tolerance);

	const angle angular_tolerance = radians(1e-6f);
	ctx.expect_near(degrees(180.f), radians(std::numbers::pi_v<float>), angular_tolerance);
}

auto gse::tests::units::dimension_products_hold_at_runtime(test::context& ctx) -> void {
	ctx.expect_near(meters(3.f) * meters(4.f), square_meters(12.f), square_meters(1e-5f));
	ctx.expect_near(meters(10.f) / seconds(2.f), meters_per_second(5.f), meters_per_second(1e-5f));
	ctx.expect_near(newtons(4.f) * meters(3.f), joules(12.f), joules(1e-5f));
	ctx.expect_near(meters_per_second(6.f) / seconds(3.f), meters_per_second_squared(2.f), meters_per_second_squared(1e-5f));
}

auto gse::tests::units::sqrt_of_area_is_length(test::context& ctx) -> void {
	const area square = meters(3.f) * meters(3.f);
	if (!ctx.expect(square > area{})) {
		return;
	}
	ctx.expect_near(sqrt(square), meters(3.f), millimeters(1e-3f));
}

auto gse::tests::units::mixed_unit_multiply_lands_at_the_right_scale(test::context& ctx) -> void {
	ctx.expect_near(centimeters(50.f) * meters(2.f), square_meters(1.f), square_meters(1e-5f));
	ctx.expect_near(kilometers(2.f) / hours(1.f), kilometers_per_hour(2.f), kilometers_per_hour(1e-3f));
}
