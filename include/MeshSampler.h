#pragma once
#include "core.h"
#include "OBJLoader.h"
#include <vector>

class MeshSampler {
public:
    // Sample N points uniformly by surface area from the given triangles.
    // Large triangles contribute proportionally more points than small ones.
    // seed: random seed for reproducibility.
    static std::vector<glm::vec3> Sample(const std::vector<Triangle>& triangles,
                                          int N,
                                          unsigned int seed = 42);

private:
    static float TriangleArea(const Triangle& t);
};
