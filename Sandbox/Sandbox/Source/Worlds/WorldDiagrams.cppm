export module sandbox:world_diagrams;

import std;
import gse;

export namespace sandbox::diagram {
	auto playground() -> std::span<const gse::gui::symbol::stroke>;

	auto pyramid() -> std::span<const gse::gui::symbol::stroke>;

	auto tumbler() -> std::span<const gse::gui::symbol::stroke>;

	auto sky() -> std::span<const gse::gui::symbol::stroke>;

	auto light_hall() -> std::span<const gse::gui::symbol::stroke>;

	auto carousel() -> std::span<const gse::gui::symbol::stroke>;

	auto machines() -> std::span<const gse::gui::symbol::stroke>;

	auto swings() -> std::span<const gse::gui::symbol::stroke>;

	auto drop() -> std::span<const gse::gui::symbol::stroke>;

	auto pair() -> std::span<const gse::gui::symbol::stroke>;

	auto stack() -> std::span<const gse::gui::symbol::stroke>;

	auto cluster() -> std::span<const gse::gui::symbol::stroke>;

	auto heap() -> std::span<const gse::gui::symbol::stroke>;

	auto mound() -> std::span<const gse::gui::symbol::stroke>;

	auto pile() -> std::span<const gse::gui::symbol::stroke>;

	auto hull_pile() -> std::span<const gse::gui::symbol::stroke>;

	auto overlap() -> std::span<const gse::gui::symbol::stroke>;

	auto shapes() -> std::span<const gse::gui::symbol::stroke>;

	auto domino() -> std::span<const gse::gui::symbol::stroke>;
}

namespace sandbox::diagram {
	using gse::gui::symbol::arc;
	using gse::gui::symbol::segment;
	using gse::gui::symbol::stroke;

	constexpr float ground_y = 0.2f;
	constexpr float full_turn_degrees = 360.f;
	constexpr float half_turn_degrees = 180.f;

	template <std::size_t N>
	consteval auto outline(
		const std::array<gse::vec2f, N>& corners
	) -> std::array<stroke, N>;

	consteval auto box(
		gse::vec2f min,
		float size
	) -> std::array<stroke, 4>;

	consteval auto body(
		gse::vec2f center,
		float radius
	) -> stroke;

	consteval auto ground() -> stroke;
}

template <std::size_t N>
consteval auto sandbox::diagram::outline(const std::array<gse::vec2f, N>& corners) -> std::array<stroke, N> {
	std::array<stroke, N> out{};
	for (std::size_t i = 0; i < N; ++i) {
		out[i] = segment(corners[i], corners[(i + 1) % N]);
	}
	return out;
}

consteval auto sandbox::diagram::box(const gse::vec2f min, const float size) -> std::array<stroke, 4> {
	return outline<4>({
		gse::vec2f{ min.x(), min.y() },
		gse::vec2f{ min.x() + size, min.y() },
		gse::vec2f{ min.x() + size, min.y() + size },
		gse::vec2f{ min.x(), min.y() + size },
	});
}

consteval auto sandbox::diagram::body(const gse::vec2f center, const float radius) -> stroke {
	return arc(center, radius, gse::degrees(0.f), gse::degrees(full_turn_degrees));
}

consteval auto sandbox::diagram::ground() -> stroke {
	return segment({ 0.1f, ground_y }, { 0.9f, ground_y });
}

auto sandbox::diagram::playground() -> std::span<const stroke> {
	static constexpr std::array<stroke, 10> data = [] {
		std::array<stroke, 10> out{};
		out[0] = ground();
		std::ranges::copy(box({ 0.20f, ground_y }, 0.24f), out.begin() + 1);
		std::ranges::copy(
			outline<4>({
				gse::vec2f{ 0.47f, ground_y },
				gse::vec2f{ 0.71f, ground_y },
				gse::vec2f{ 0.75f, 0.44f },
				gse::vec2f{ 0.51f, 0.44f },
			}),
			out.begin() + 5
		);
		out[9] = body({ 0.32f, 0.64f }, 0.10f);
		return out;
	}();
	return data;
}

auto sandbox::diagram::pyramid() -> std::span<const stroke> {
	static constexpr std::array<stroke, 25> data = [] {
		constexpr float unit = 0.20f;
		std::array<stroke, 25> out{};
		out[0] = ground();
		std::ranges::copy(box({ 0.20f, ground_y }, unit), out.begin() + 1);
		std::ranges::copy(box({ 0.40f, ground_y }, unit), out.begin() + 5);
		std::ranges::copy(box({ 0.60f, ground_y }, unit), out.begin() + 9);
		std::ranges::copy(box({ 0.30f, 0.40f }, unit), out.begin() + 13);
		std::ranges::copy(box({ 0.50f, 0.40f }, unit), out.begin() + 17);
		std::ranges::copy(box({ 0.40f, 0.60f }, unit), out.begin() + 21);
		return out;
	}();
	return data;
}

auto sandbox::diagram::tumbler() -> std::span<const stroke> {
	static constexpr std::array<stroke, 13> data = [] {
		std::array<stroke, 13> out{};
		out[0] = body({ 0.5f, 0.5f }, 0.36f);
		out[1] = segment({ 0.5f, 0.5f }, { 0.5f, 0.86f });
		out[2] = segment({ 0.5f, 0.5f }, { 0.86f, 0.5f });
		out[3] = segment({ 0.5f, 0.5f }, { 0.5f, 0.14f });
		out[4] = segment({ 0.5f, 0.5f }, { 0.14f, 0.5f });
		std::ranges::copy(box({ 0.32f, 0.28f }, 0.13f), out.begin() + 5);
		std::ranges::copy(box({ 0.55f, 0.56f }, 0.13f), out.begin() + 9);
		return out;
	}();
	return data;
}

auto sandbox::diagram::sky() -> std::span<const stroke> {
	static constexpr std::array<stroke, 6> data{
		segment({ 0.10f, 0.26f }, { 0.90f, 0.26f }),
		body({ 0.73f, 0.73f }, 0.12f),
		arc({ 0.33f, 0.52f }, 0.12f, gse::degrees(0.f), gse::degrees(half_turn_degrees)),
		arc({ 0.47f, 0.55f }, 0.16f, gse::degrees(0.f), gse::degrees(half_turn_degrees)),
		arc({ 0.60f, 0.52f }, 0.11f, gse::degrees(0.f), gse::degrees(half_turn_degrees)),
		segment({ 0.21f, 0.52f }, { 0.71f, 0.52f }),
	};
	return data;
}

auto sandbox::diagram::light_hall() -> std::span<const stroke> {
	static constexpr std::array<stroke, 10> data{
		segment({ 0.14f, ground_y }, { 0.86f, ground_y }),
		segment({ 0.18f, ground_y }, { 0.18f, 0.82f }),
		segment({ 0.82f, ground_y }, { 0.82f, 0.82f }),
		segment({ 0.18f, 0.82f }, { 0.82f, 0.82f }),
		segment({ 0.33f, 0.82f }, { 0.33f, 0.70f }),
		segment({ 0.50f, 0.82f }, { 0.50f, 0.62f }),
		segment({ 0.67f, 0.82f }, { 0.67f, 0.70f }),
		body({ 0.33f, 0.63f }, 0.07f),
		body({ 0.50f, 0.55f }, 0.07f),
		body({ 0.67f, 0.63f }, 0.07f),
	};
	return data;
}

auto sandbox::diagram::carousel() -> std::span<const stroke> {
	static constexpr std::array<stroke, 11> data = [] {
		std::array<stroke, 11> out{};
		out[0] = body({ 0.42f, 0.52f }, 0.34f);
		out[1] = segment({ 0.42f, 0.52f }, { 0.42f, 0.86f });
		out[2] = segment({ 0.42f, 0.52f }, { 0.76f, 0.52f });
		std::ranges::copy(box({ 0.24f, 0.30f }, 0.12f), out.begin() + 3);
		std::ranges::copy(box({ 0.48f, 0.60f }, 0.12f), out.begin() + 7);
		return out;
	}();
	return data;
}

auto sandbox::diagram::machines() -> std::span<const stroke> {
	static constexpr std::array<stroke, 10> data = [] {
		std::array<stroke, 10> out{};
		out[0] = ground();
		out[1] = segment({ 0.26f, ground_y }, { 0.26f, 0.58f });
		out[2] = segment({ 0.18f, 0.58f }, { 0.34f, 0.58f });
		std::ranges::copy(box({ 0.18f, 0.62f }, 0.16f), out.begin() + 3);
		out[7] = segment({ 0.58f, ground_y }, { 0.58f, 0.74f });
		out[8] = segment({ 0.58f, 0.46f }, { 0.80f, 0.46f });
		out[9] = segment({ 0.80f, 0.46f }, { 0.80f, 0.74f });
		return out;
	}();
	return data;
}

auto sandbox::diagram::swings() -> std::span<const stroke> {
	static constexpr std::array<stroke, 9> data = [] {
		std::array<stroke, 9> out{};
		out[0] = segment({ 0.14f, 0.84f }, { 0.86f, 0.84f });
		out[1] = segment({ 0.30f, 0.84f }, { 0.24f, 0.44f });
		out[2] = body({ 0.23f, 0.36f }, 0.08f);
		out[3] = segment({ 0.50f, 0.84f }, { 0.50f, 0.40f });
		out[4] = body({ 0.50f, 0.32f }, 0.08f);
		out[5] = segment({ 0.70f, 0.84f }, { 0.76f, 0.44f });
		out[6] = body({ 0.77f, 0.36f }, 0.08f);
		out[7] = segment({ 0.14f, 0.84f }, { 0.14f, 0.74f });
		out[8] = segment({ 0.86f, 0.84f }, { 0.86f, 0.74f });
		return out;
	}();
	return data;
}

auto sandbox::diagram::drop() -> std::span<const stroke> {
	static constexpr std::array<stroke, 8> data = [] {
		std::array<stroke, 8> out{};
		out[0] = ground();
		std::ranges::copy(box({ 0.38f, 0.64f }, 0.24f), out.begin() + 1);
		out[5] = segment({ 0.50f, 0.58f }, { 0.50f, 0.50f });
		out[6] = segment({ 0.50f, 0.44f }, { 0.50f, 0.36f });
		out[7] = segment({ 0.50f, 0.30f }, { 0.50f, 0.26f });
		return out;
	}();
	return data;
}

auto sandbox::diagram::pair() -> std::span<const stroke> {
	static constexpr std::array<stroke, 9> data = [] {
		std::array<stroke, 9> out{};
		out[0] = ground();
		std::ranges::copy(box({ 0.26f, ground_y }, 0.24f), out.begin() + 1);
		std::ranges::copy(box({ 0.50f, ground_y }, 0.24f), out.begin() + 5);
		return out;
	}();
	return data;
}

auto sandbox::diagram::stack() -> std::span<const stroke> {
	static constexpr std::array<stroke, 13> data = [] {
		constexpr float unit = 0.24f;
		std::array<stroke, 13> out{};
		out[0] = ground();
		std::ranges::copy(box({ 0.38f, ground_y }, unit), out.begin() + 1);
		std::ranges::copy(box({ 0.38f, 0.44f }, unit), out.begin() + 5);
		std::ranges::copy(box({ 0.38f, 0.68f }, unit), out.begin() + 9);
		return out;
	}();
	return data;
}

auto sandbox::diagram::cluster() -> std::span<const stroke> {
	static constexpr std::array<stroke, 21> data = [] {
		constexpr float unit = 0.18f;
		std::array<stroke, 21> out{};
		out[0] = ground();
		std::ranges::copy(box({ 0.32f, ground_y }, unit), out.begin() + 1);
		std::ranges::copy(box({ 0.50f, ground_y }, unit), out.begin() + 5);
		std::ranges::copy(box({ 0.32f, 0.38f }, unit), out.begin() + 9);
		std::ranges::copy(box({ 0.50f, 0.38f }, unit), out.begin() + 13);
		std::ranges::copy(box({ 0.41f, 0.56f }, unit), out.begin() + 17);
		return out;
	}();
	return data;
}

auto sandbox::diagram::heap() -> std::span<const stroke> {
	static constexpr std::array<stroke, 29> data = [] {
		constexpr float unit = 0.16f;
		std::array<stroke, 29> out{};
		out[0] = ground();
		std::ranges::copy(box({ 0.18f, ground_y }, unit), out.begin() + 1);
		std::ranges::copy(box({ 0.34f, ground_y }, unit), out.begin() + 5);
		std::ranges::copy(box({ 0.50f, ground_y }, unit), out.begin() + 9);
		std::ranges::copy(box({ 0.66f, ground_y }, unit), out.begin() + 13);
		std::ranges::copy(box({ 0.26f, 0.36f }, unit), out.begin() + 17);
		std::ranges::copy(box({ 0.42f, 0.36f }, unit), out.begin() + 21);
		std::ranges::copy(box({ 0.58f, 0.36f }, unit), out.begin() + 25);
		return out;
	}();
	return data;
}

auto sandbox::diagram::mound() -> std::span<const stroke> {
	static constexpr std::array<stroke, 18> data = [] {
		constexpr float unit = 0.15f;
		std::array<stroke, 18> out{};
		out[0] = ground();
		out[1] = arc({ 0.50f, ground_y }, 0.34f, gse::degrees(0.f), gse::degrees(half_turn_degrees));
		std::ranges::copy(box({ 0.25f, ground_y }, unit), out.begin() + 2);
		std::ranges::copy(box({ 0.42f, ground_y }, unit), out.begin() + 6);
		std::ranges::copy(box({ 0.59f, ground_y }, unit), out.begin() + 10);
		std::ranges::copy(box({ 0.42f, 0.35f }, unit), out.begin() + 14);
		return out;
	}();
	return data;
}

auto sandbox::diagram::pile() -> std::span<const stroke> {
	static constexpr std::array<stroke, 17> data = [] {
		std::array<stroke, 17> out{};
		out[0] = ground();
		std::ranges::copy(
			outline<4>({
				gse::vec2f{ 0.16f, ground_y },
				gse::vec2f{ 0.40f, 0.23f },
				gse::vec2f{ 0.37f, 0.44f },
				gse::vec2f{ 0.13f, 0.41f },
			}),
			out.begin() + 1
		);
		std::ranges::copy(
			outline<4>({
				gse::vec2f{ 0.44f, ground_y },
				gse::vec2f{ 0.66f, ground_y },
				gse::vec2f{ 0.70f, 0.40f },
				gse::vec2f{ 0.48f, 0.38f },
			}),
			out.begin() + 5
		);
		std::ranges::copy(
			outline<4>({
				gse::vec2f{ 0.70f, 0.22f },
				gse::vec2f{ 0.88f, 0.30f },
				gse::vec2f{ 0.80f, 0.48f },
				gse::vec2f{ 0.62f, 0.40f },
			}),
			out.begin() + 9
		);
		std::ranges::copy(
			outline<4>({
				gse::vec2f{ 0.30f, 0.46f },
				gse::vec2f{ 0.52f, 0.42f },
				gse::vec2f{ 0.56f, 0.62f },
				gse::vec2f{ 0.34f, 0.66f },
			}),
			out.begin() + 13
		);
		return out;
	}();
	return data;
}

auto sandbox::diagram::hull_pile() -> std::span<const stroke> {
	static constexpr std::array<stroke, 16> data = [] {
		std::array<stroke, 16> out{};
		out[0] = ground();
		std::ranges::copy(
			outline<5>({
				gse::vec2f{ 0.16f, ground_y },
				gse::vec2f{ 0.36f, ground_y },
				gse::vec2f{ 0.42f, 0.36f },
				gse::vec2f{ 0.26f, 0.48f },
				gse::vec2f{ 0.12f, 0.34f },
			}),
			out.begin() + 1
		);
		std::ranges::copy(
			outline<5>({
				gse::vec2f{ 0.48f, ground_y },
				gse::vec2f{ 0.70f, ground_y },
				gse::vec2f{ 0.78f, 0.34f },
				gse::vec2f{ 0.60f, 0.44f },
				gse::vec2f{ 0.44f, 0.33f },
			}),
			out.begin() + 6
		);
		std::ranges::copy(
			outline<5>({
				gse::vec2f{ 0.34f, 0.50f },
				gse::vec2f{ 0.54f, 0.46f },
				gse::vec2f{ 0.64f, 0.60f },
				gse::vec2f{ 0.48f, 0.72f },
				gse::vec2f{ 0.32f, 0.64f },
			}),
			out.begin() + 11
		);
		return out;
	}();
	return data;
}

auto sandbox::diagram::overlap() -> std::span<const stroke> {
	static constexpr std::array<stroke, 9> data = [] {
		std::array<stroke, 9> out{};
		out[0] = ground();
		std::ranges::copy(box({ 0.28f, ground_y }, 0.28f), out.begin() + 1);
		std::ranges::copy(box({ 0.44f, 0.34f }, 0.28f), out.begin() + 5);
		return out;
	}();
	return data;
}

auto sandbox::diagram::shapes() -> std::span<const stroke> {
	static constexpr std::array<stroke, 9> data = [] {
		std::array<stroke, 9> out{};
		out[0] = ground();
		std::ranges::copy(box({ 0.14f, ground_y }, 0.22f), out.begin() + 1);
		out[5] = body({ 0.52f, ground_y + 0.11f }, 0.11f);
		std::ranges::copy(
			outline<3>({
				gse::vec2f{ 0.68f, ground_y },
				gse::vec2f{ 0.88f, ground_y },
				gse::vec2f{ 0.78f, 0.48f },
			}),
			out.begin() + 6
		);
		return out;
	}();
	return data;
}

auto sandbox::diagram::domino() -> std::span<const stroke> {
	static constexpr std::array<stroke, 17> data = [] {
		constexpr std::array<float, 4> feet{ 0.18f, 0.34f, 0.50f, 0.66f };
		constexpr float lean = 0.09f;
		constexpr float width = 0.07f;
		constexpr float height = 0.46f;
		std::array<stroke, 17> out{};
		out[0] = ground();
		for (std::size_t i = 0; i < feet.size(); ++i) {
			const float foot = feet[i];
			std::ranges::copy(
				outline<4>({
					gse::vec2f{ foot, ground_y },
					gse::vec2f{ foot + width, ground_y },
					gse::vec2f{ foot + width + lean, ground_y + height },
					gse::vec2f{ foot + lean, ground_y + height },
				}),
				out.begin() + 1 + static_cast<std::ptrdiff_t>(i) * 4
			);
		}
		return out;
	}();
	return data;
}
