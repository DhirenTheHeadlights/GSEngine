#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

#if defined(GSE_LIBM_COREMATH)
#include "coremath/coremath.h"

namespace probe_libm {
	auto sin(const float x) -> float {
		return gse::libm::coremath::sin_impl::cr_sinf(x);
	}

	auto cos(const float x) -> float {
		return gse::libm::coremath::cos_impl::cr_cosf(x);
	}

	auto tan(const float x) -> float {
		return gse::libm::coremath::tan_impl::cr_tanf(x);
	}

	auto asin(const float x) -> float {
		return gse::libm::coremath::asin_impl::cr_asinf(x);
	}

	auto acos(const float x) -> float {
		return gse::libm::coremath::acos_impl::cr_acosf(x);
	}

	auto atan2(const float y, const float x) -> float {
		return gse::libm::coremath::atan2_impl::cr_atan2f(y, x);
	}

	using std::fabs;
	using std::sqrt;
}
#else
namespace probe_libm = std;
#endif

namespace {
	volatile std::uint32_t seed_source = 0x9e3779b9u;
	volatile float dt_source = 0.0166667f;

	std::uint32_t state = 0;

	auto next() -> std::uint32_t {
		state = state * 1664525u + 1013904223u;
		return state;
	}

	auto uniform(const float lo, const float hi) -> float {
		return lo + (hi - lo) * static_cast<float>(next() >> 8) * (1.f / 16777216.f);
	}

	auto bits(const float f) -> std::uint32_t {
		std::uint32_t b;
		std::memcpy(&b, &f, sizeof b);
		return b;
	}

	auto from_bits(const std::uint32_t b) -> float {
		float f;
		std::memcpy(&f, &b, sizeof f);
		return f;
	}

	auto log_uniform(const std::uint32_t lo_exp, const std::uint32_t hi_exp) -> float {
		const std::uint32_t e = lo_exp + next() % (hi_exp - lo_exp);
		return from_bits((e << 23) | (next() >> 9));
	}

	int lines = 0;

	auto emit1(const char* fn, const float a, const float r) -> void {
		std::printf("libm %s %08x 00000000 %08x\n", fn, bits(a), bits(r));
		++lines;
	}

	auto emit2(const char* fn, const float a, const float b, const float r) -> void {
		std::printf("libm %s %08x %08x %08x\n", fn, bits(a), bits(b), bits(r));
		++lines;
	}

	auto unary_all(const float a) -> void {
		emit1("sin", a, probe_libm::sin(a));
		emit1("cos", a, probe_libm::cos(a));
		emit1("tan", a, probe_libm::tan(a));
		emit1("sqrt", probe_libm::fabs(a), probe_libm::sqrt(probe_libm::fabs(a)));
		if (a >= -1.f && a <= 1.f) {
			emit1("asin", a, probe_libm::asin(a));
			emit1("acos", a, probe_libm::acos(a));
		}
	}

	auto seed_body() -> void {
		const float w[3] = { from_bits(0xb4b243a7u), from_bits(0x37672fceu), from_bits(0x373b669cu) };
		const float q[4] = { from_bits(0x3f800000u), from_bits(0x3495bb07u), from_bits(0xb447fd24u), from_bits(0xb766d260u) };
		const float dt = dt_source;
		for (int substeps = 1; substeps <= 8; ++substeps) {
			const float h = dt / static_cast<float>(substeps);
			const float sx = w[0] * h;
			const float sy = w[1] * h;
			const float sz = w[2] * h;
			const float angle = probe_libm::sqrt(sx * sx + sy * sy + sz * sz);
			emit1("seed_sqrt", sx * sx + sy * sy + sz * sz, angle);
			emit1("seed_sin", angle * 0.5f, probe_libm::sin(angle * 0.5f));
			emit1("seed_cos", angle * 0.5f, probe_libm::cos(angle * 0.5f));
		}
		const float v = probe_libm::sqrt(q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
		emit1("seed_sqrt", q[1] * q[1] + q[2] * q[2] + q[3] * q[3], v);
		emit2("seed_atan2", v, q[0], probe_libm::atan2(v, q[0]));
		emit1("seed_acos", q[0], probe_libm::acos(q[0]));
		emit1("seed_asin", v, probe_libm::asin(v));
		for (const float c : q) {
			unary_all(c);
		}
		for (const float c : w) {
			unary_all(c);
		}
	}
}

extern "C" int entry() {
	state = seed_source;
	seed_body();
	for (int i = 0; i < 4000; ++i) {
		unary_all(log_uniform(97u, 128u));
	}
	for (int i = 0; i < 4000; ++i) {
		unary_all(uniform(-3.14159265f, 3.14159265f));
	}
	for (int i = 0; i < 2000; ++i) {
		const float one = 1.f;
		const std::uint32_t k = next() % 4096u;
		const float a = from_bits(bits(one) - k);
		emit1("asin", a, probe_libm::asin(a));
		emit1("acos", a, probe_libm::acos(a));
		emit1("asin", -a, probe_libm::asin(-a));
		emit1("acos", -a, probe_libm::acos(-a));
	}
	for (int i = 0; i < 4000; ++i) {
		const float y = log_uniform(97u, 127u) * (next() & 1u ? 1.f : -1.f);
		const float x = uniform(0.5f, 1.f);
		emit2("atan2", y, x, probe_libm::atan2(y, x));
	}
	for (int i = 0; i < 4000; ++i) {
		const float y = uniform(-4.f, 4.f);
		const float x = uniform(-4.f, 4.f);
		emit2("atan2", y, x, probe_libm::atan2(y, x));
	}
	std::printf("libm_lines %d\n", lines);
	std::fflush(stdout);
	return 42;
}

#if !defined(__wasm__)
int main() {
	return entry() == 42 ? 0 : 1;
}
#endif
