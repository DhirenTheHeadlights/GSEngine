export module gse.math:easing;

import std;

import gse.meta;

import :libm;

export namespace gse {
	auto ease_linear(
		float t
	) -> float;

	auto ease_smoothstep(
		float t
	) -> float;

	auto ease_cubic_in(
		float t
	) -> float;

	auto ease_cubic_out(
		float t
	) -> float;

	auto ease_cubic_in_out(
		float t
	) -> float;

	auto ease_back_out(
		float t
	) -> float;

	auto ease_spring(
		float t
	) -> float;

	struct ease_curve {
		float (*shape)(float) = nullptr;
	};

	enum class ease_kind : std::uint8_t {
		linear [[= ease_curve{
			.shape = &ease_linear,
		}]],
		smoothstep [[= ease_curve{
			.shape = &ease_smoothstep,
		}]],
		cubic_in [[= ease_curve{
			.shape = &ease_cubic_in,
		}]],
		cubic_out [[= ease_curve{
			.shape = &ease_cubic_out,
		}]],
		cubic_in_out [[= ease_curve{
			.shape = &ease_cubic_in_out,
		}]],
		back_out [[= ease_curve{
			.shape = &ease_back_out,
		}]],
		spring [[= ease_curve{
			.shape = &ease_spring,
		}]],
	};

	auto ease(
		ease_kind kind,
		float t
	) -> float;
}

auto gse::ease_linear(const float t) -> float {
	return t;
}

auto gse::ease_smoothstep(const float t) -> float {
	return t * t * (3.f - 2.f * t);
}

auto gse::ease_cubic_in(const float t) -> float {
	return t * t * t;
}

auto gse::ease_cubic_out(const float t) -> float {
	const float inverted = 1.f - t;
	return 1.f - inverted * inverted * inverted;
}

auto gse::ease_cubic_in_out(const float t) -> float {
	if (t < 0.5f) {
		return 4.f * t * t * t;
	}
	const float inverted = -2.f * t + 2.f;
	return 1.f - inverted * inverted * inverted * 0.5f;
}

auto gse::ease_back_out(const float t) -> float {
	const float overshoot = 1.70158f;
	const float cubic = overshoot + 1.f;
	const float offset = t - 1.f;
	return 1.f + cubic * offset * offset * offset + overshoot * offset * offset;
}

auto gse::ease_spring(const float t) -> float {
	if (t <= 0.f) {
		return 0.f;
	}
	if (t >= 1.f) {
		return 1.f;
	}
	const float period = 2.f * std::numbers::pi_v<float> / 3.f;
	return libm::pow(2.f, -10.f * t) * libm::sin((t * 10.f - 0.75f) * period) + 1.f;
}

auto gse::ease(const ease_kind kind, const float t) -> float {
	const ease_curve curve = annotation_from_enum<ease_curve>(kind, { .shape = &ease_linear });
	return curve.shape(std::clamp(t, 0.f, 1.f));
}
