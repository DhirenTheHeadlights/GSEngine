module gse.math:libm_impl;

import std;

import :libm;

auto gse::libm::sinf(const float x) -> float {
	return std::sin(x);
}

auto gse::libm::cosf(const float x) -> float {
	return std::cos(x);
}

auto gse::libm::tanf(const float x) -> float {
	return std::tan(x);
}

auto gse::libm::asinf(const float x) -> float {
	return std::asin(x);
}

auto gse::libm::acosf(const float x) -> float {
	return std::acos(x);
}

auto gse::libm::atanf(const float x) -> float {
	return std::atan(x);
}

auto gse::libm::atan2f(const float y, const float x) -> float {
	return std::atan2(y, x);
}

auto gse::libm::expf(const float x) -> float {
	return std::exp(x);
}

auto gse::libm::logf(const float x) -> float {
	return std::log(x);
}

auto gse::libm::powf(const float x, const float y) -> float {
	return std::pow(x, y);
}
