#pragma once

#include <float.h>
#include <math.h>
#include <stdint.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wparentheses"
#pragma GCC diagnostic ignored "-Wunused-function"

namespace gse::libm::coremath {
#include "fma.c"
#include "fmaf.c"
#undef ASUINT64
#undef ZEROINFNAN

	namespace sin_impl {
#include "sinf.c"
	}

	namespace cos_impl {
#include "cosf.c"
	}

	namespace tan_impl {
#include "tanf.c"
	}

	namespace asin_impl {
#include "asinf.c"
	}

	namespace acos_impl {
#include "acosf.c"
	}

	namespace atan_impl {
#include "atanf.c"
	}
#undef PI_OVER2_H
#undef PI_OVER2_L

	namespace atan2_impl {
#include "atan2f.c"
	}
#undef pi
#undef pi2
#undef pi2l

	namespace exp_impl {
#include "expf.c"
	}

	namespace log_impl {
#include "logf.c"
	}

	namespace pow_impl {
#include "powf.c"
	}
#undef FLAG_T
}

#pragma GCC diagnostic pop
