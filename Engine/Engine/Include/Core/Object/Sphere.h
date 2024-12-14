#pragma once
#include "Engine.h"

namespace gse {
    class sphere : public object {
    public:
        sphere(const vec3<length>& position, const length radius, const int sectors = 36, const int stacks = 18)
            : object("Sphere"), m_initial_position(position), m_radius(radius), m_sectors(sectors), m_stacks(stacks) {}

        void initialize() override;
        void update() override;
        void render() override {}

        length get_radius() const { return m_radius; }
    private:
        vec3<length> m_initial_position;
        length m_radius;
        int m_sectors; // Number of slices around the sphere
        int m_stacks;  // Number of stacks from top to bottom
    };
}
