#include "OptimalTransport.h"
#include <iostream>


//public

OTMap OptimalTransport::Compute(const Gaussian& gaussianA, const Gaussian& gaussianB){
    const Eigen::Matrix3d& SigmaA = gaussianA.covariance;
    const Eigen::Matrix3d& SigmaB = gaussianB.covariance;

    //get Sigma_A^{1/2} and Sigma_A^{-1/2} using helper functions
    Eigen::Matrix3d SigmaA_sqrt    = MatrixSqrt(SigmaA);
    Eigen::Matrix3d SigmaA_invsqrt = MatrixInvSqrt(SigmaA);

    //M = Sigma_A^{1/2} * Sigma_B * Sigma_A^{1/2}
    Eigen::Matrix3d M = SigmaA_sqrt * SigmaB * SigmaA_sqrt;

    //M^{1/2}
    Eigen::Matrix3d M_sqrt = MatrixSqrt(M);

    //transpose A
    Eigen::Matrix3d A = SigmaA_invsqrt * M_sqrt * SigmaA_invsqrt;

    OTMap map;
    map.muA = gaussianA.mean;
    map.muB = gaussianB.mean;
    map.A = A;

    return map;
}

//T(x) = μ_B + A · (x − μ_A)
glm::vec3 OptimalTransport::Apply(const OTMap& map, const glm::vec3& x){
    Eigen::Vector3d xe(x.x, x.y, x.z);
    Eigen::Vector3d result = map.muB + map.A * (xe - map.muA);

    return glm::vec3((float)result(0), (float)result(1), (float)result(2));
}


//private helpers
Eigen::Matrix3d OptimalTransport::MatrixSqrt(const Eigen::Matrix3d& M) {
    Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> solver(M);
    // Clamp negative eigenvalues to 0 (can occur due to float noise)
    Eigen::Vector3d sqrtVals = solver.eigenvalues().cwiseMax(0.0).cwiseSqrt();
    return solver.eigenvectors() * sqrtVals.asDiagonal() * solver.eigenvectors().transpose();
}

Eigen::Matrix3d OptimalTransport::MatrixInvSqrt(const Eigen::Matrix3d& M, double epsilon) {
    Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> solver(M);
    Eigen::Vector3d sqrtVals = solver.eigenvalues().cwiseMax(0.0).cwiseSqrt();
    // Clamp to epsilon before inverting to avoid division by near-zero
    Eigen::Vector3d invSqrtVals = sqrtVals.cwiseMax(epsilon).cwiseInverse();
    return solver.eigenvectors() * invSqrtVals.asDiagonal() * solver.eigenvectors().transpose();
}