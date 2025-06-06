#pragma once

#include <cmath>
#include <optional>

#include "Eigen/Core"
#include "Eigen/Geometry"
#include "pros/distance.hpp"
#include "constants.hpp"

inline double angleDiff(double a, double b) {
    return std::remainder(b - a, 2 * M_PI);
}

// Cheap approximation of normpdf (normal distribution PDF)
double normPdf(double x, double mu = 0.0, double sigma = 1.0) {
    constexpr double invSqrt2Pi = 0.3989422804014327; // 1 / sqrt(2 * pi)
    double z = (x - mu) / sigma;
    return (invSqrt2Pi / sigma) * std::exp(-0.5 * z * z);
}

class Distance {
public:
    explicit Distance(pros::Distance sensor, Eigen::Vector3f offset)
        : distance(0), stddev(0), sensor(std::move(sensor)), offset(std::move(offset)) {}

    void update() {
        distance = sensor.get() * MM_TO_IN;
        stddev = 0.2 * distance / std::sqrt(sensor.get_confidence() / 64.0);
    }

    [[nodiscard]] std::optional<float> getProbability(const Eigen::Vector3f& pose) const {
        if (distance > 9999 * MM_TO_IN) {
            return std::nullopt;
        }

        // Get the current absolute angle of the sensor (robot angle + offset angle)
        const auto angle = pose.z() + offset.z();

        // Get the current absolute position of the sensor (robot position + offset relative to field)
        const auto position = pose.head<2>() + Eigen::Rotation2Df(pose.z()) * offset.head<2>();

        // Predict the value of the distance sensor by finding the closest wall to it
        // then dividing the distance from the wall by cos(theta) to find the hypotenuse
        // which represents the predicted distance sensor reading
        double predicted = 50.0;

        if (const auto theta = std::fabs(angleDiff(0, angle)); theta < M_PI_2) {
            predicted = std::min(predicted, (WALL - position.x()) / std::cos(theta));
        }

        if (const auto theta = std::fabs(angleDiff(M_PI_2, angle)); theta < M_PI_2) {
            predicted = std::min(predicted, (WALL - position.y()) / std::cos(theta));
        }

        if (const auto theta = std::fabs(angleDiff(M_PI, angle)); theta < M_PI_2) {
            predicted = std::min(predicted, (position.x() + WALL) / std::cos(theta));
        }

        if (const auto theta = std::fabs(angleDiff(3 * M_PI_2, angle)); theta < M_PI_2) {
            predicted = std::min(predicted, (position.y() + WALL) / std::cos(theta));
        }

        return normPdf(distance, predicted, stddev);
    }

protected:
    Eigen::Vector3f offset;
    pros::Distance sensor;

    double distance;
    double stddev;
};
