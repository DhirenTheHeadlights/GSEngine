#pragma once
#include "Engine.h"

namespace gse {
    class sphere : public object {
    public:
        sphere(const Vec3<Length>& position, const Length radius, const int sectors = 36, const int stacks = 18)
            : object("Sphere"), position(position), radius(radius), sectors(sectors), stacks(stacks) {}

        void initialize() override;
        void update() override {}
        void render() override {}

		Vec3<Length> get_position() const { return position; }
        Length get_radius() const { return radius; }
    private:
        Vec3<Length> position;
        Length radius;
        int sectors; // Number of slices around the sphere
        int stacks;  // Number of stacks from top to bottom
    };
}
