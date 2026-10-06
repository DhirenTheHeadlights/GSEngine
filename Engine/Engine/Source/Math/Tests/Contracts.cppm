export module gse.tests.contracts;

import std;

import gse.math;
import gse.test;

export namespace gse::tests::contracts {
	[[= test::violation{ .expect = "index < N" }]]
	auto vector_index_out_of_range(
		test::context& ctx
	) -> void;

	[[= test::violation{ .expect = "index < Cols" }]]
	auto matrix_column_out_of_range(
		test::context& ctx
	) -> void;

	[[= test::violation{ .expect = "det != value_type(0)" }]]
	auto singular_matrix_has_no_inverse(
		test::context& ctx
	) -> void;

	[[= test::violation{ .expect = "norm_squared(q) != T(0)" }]]
	auto zero_quaternion_has_no_inverse(
		test::context& ctx
	) -> void;

	[[= test::violation{ .expect = "v >= T(-1)" }]]
	auto asin_outside_its_domain(
		test::context& ctx
	) -> void;

	[[= test::violation{ .expect = "(q) >= 0" }]]
	auto sqrt_of_a_negative_quantity(
		test::context& ctx
	) -> void;

	[[= test::violation{ .expect = "fov > angle_t<T>{}" }]]
	auto perspective_without_a_field_of_view(
		test::context& ctx
	) -> void;

	[[= test::violation{ .expect = "is_zero(cross(up, position - target))" }]]
	auto look_at_along_its_own_up_axis(
		test::context& ctx
	) -> void;
}

auto gse::tests::contracts::vector_index_out_of_range(test::context& ctx) -> void {
	const vec3f v{ 1.f, 2.f, 3.f };
	std::ignore = v[ctx.failures().size() + 3];
}

auto gse::tests::contracts::matrix_column_out_of_range(test::context& ctx) -> void {
	const mat3f m(1.f);
	std::ignore = m[ctx.failures().size() + 3];
}

auto gse::tests::contracts::singular_matrix_has_no_inverse(test::context& ctx) -> void {
	const float edge = static_cast<float>(ctx.failures().size()) + 1.f;
	const mat3f m{ vec3f{ edge, edge, edge }, vec3f{ edge, edge, edge }, vec3f{ edge, edge, edge } };
	std::ignore = m.inverse();
}

auto gse::tests::contracts::zero_quaternion_has_no_inverse(test::context& ctx) -> void {
	const float empty = static_cast<float>(ctx.failures().size());
	std::ignore = inverse(quat{ empty, empty, empty, empty });
}

auto gse::tests::contracts::asin_outside_its_domain(test::context& ctx) -> void {
	std::ignore = asin(static_cast<float>(ctx.failures().size()) + 2.f);
}

auto gse::tests::contracts::sqrt_of_a_negative_quantity(test::context& ctx) -> void {
	std::ignore = sqrt(square_meters(static_cast<float>(ctx.failures().size()) - 1.f));
}

auto gse::tests::contracts::perspective_without_a_field_of_view(test::context& ctx) -> void {
	const float empty = static_cast<float>(ctx.failures().size());
	std::ignore = perspective(degrees(empty), 1.f, meters(0.1f), meters(100.f));
}

auto gse::tests::contracts::look_at_along_its_own_up_axis(test::context& ctx) -> void {
	const float offset = static_cast<float>(ctx.failures().size()) + 5.f;
	const vec3<length> position{ meters(0.f), meters(offset), meters(0.f) };
	const vec3<length> target{ meters(0.f), meters(0.f), meters(0.f) };
	std::ignore = look_at(position, target, vec3f{ 0.f, 1.f, 0.f });
}
