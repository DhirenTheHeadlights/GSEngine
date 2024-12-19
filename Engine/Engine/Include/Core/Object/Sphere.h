#pragma once
#include "Engine.h"

namespace gse {
    class sphere : public object {
    public:
        sphere(const vec3<length>& position, const length radius, const int sectors = 36, const int stacks = 18);
    };
}
