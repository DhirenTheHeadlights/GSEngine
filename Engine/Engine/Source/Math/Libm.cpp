module;

#include "coremath/coremath.h"

module gse.math:libm_impl;

import std;

import :libm;

auto gse::libm::sinf(const float x) -> float {
	return coremath::sin_impl::cr_sinf(x);
}

auto gse::libm::cosf(const float x) -> float {
	return coremath::cos_impl::cr_cosf(x);
}

auto gse::libm::tanf(const float x) -> float {
	return coremath::tan_impl::cr_tanf(x);
}

auto gse::libm::asinf(const float x) -> float {
	return coremath::asin_impl::cr_asinf(x);
}

auto gse::libm::acosf(const float x) -> float {
	return coremath::acos_impl::cr_acosf(x);
}

auto gse::libm::atanf(const float x) -> float {
	return coremath::atan_impl::cr_atanf(x);
}

auto gse::libm::atan2f(const float y, const float x) -> float {
	return coremath::atan2_impl::cr_atan2f(y, x);
}

auto gse::libm::expf(const float x) -> float {
	return coremath::exp_impl::cr_expf(x);
}

auto gse::libm::logf(const float x) -> float {
	return coremath::log_impl::cr_logf(x);
}

auto gse::libm::powf(const float x, const float y) -> float {
	return coremath::pow_impl::cr_powf(x, y);
}
