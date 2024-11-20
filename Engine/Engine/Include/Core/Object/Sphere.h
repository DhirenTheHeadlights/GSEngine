#pragma once
#include "Engine.h"

namespace Engine {
    class Sphere : public Object {
    public:
        Sphere(const Vec3<Length>& position, const Length radius, const int sectors = 36, const int stacks = 18)
            : Object("Sphere"), position(position), radius(radius), sectors(sectors), stacks(stacks) {}

        void initialize();
        void update() override;
        void render() override;

    private:
        Vec3<Length> position;
        Length radius;
        int sectors; // Number of slices around the sphere
        int stacks;  // Number of stacks from top to bottom
    };
}
