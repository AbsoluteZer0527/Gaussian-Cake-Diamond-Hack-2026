#pragma once
#include "core.h"
#include "Gaussian.h"
#include <Eigen/Dense>


//OT map struct T(x) = muB + A * (x - muA)
struct OTMap {
    Eigen::Vector3d muA;
    Eigen::Vector3d muB;
    Eigen::Matrix3d A;    // transport matrix
};

class OptimalTransport {
public:
    //function that computes the closed-form OT map between two Gaussians.
    //A = SigmaA^{-1/2} * (SigmaA^{1/2} * SigmaB * SigmaA^{1/2})^{1/2} * SigmaA^{-1/2}

    static OTMap Compute(const Gaussian& gaussianA, const Gaussian& gaussianB);

    //apply the map to a glm::vec3 particle position
    static glm::vec3 Apply(const OTMap& map, const glm::vec3& x);

private:
    //decompose covariance matrices since they are symmetric
    //(Av = \lambda v)
    //M = V · D · Vᵀ
    //clamps negative values to 0
    static Eigen::Matrix3d MatrixSqrt(const Eigen::Matrix3d& M);

    //symmetric matrix inverse square root
    // V * (1/sqrt(D)) * V^T
    //clamps denominator to epsilon so no /0
    static Eigen::Matrix3d MatrixInvSqrt(const Eigen::Matrix3d& M,
                                          double epsilon = 1e-9);
};