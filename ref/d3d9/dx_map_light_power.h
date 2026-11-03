#pragma once
#include <algorithm>
#include <cmath>

namespace dx_map_light {
// Match Remix 268fb5b2's default five-sample legacy conversion. Radius
// changes emitting area, never the authored attenuation or total output.
inline float radiance(float brightness, float range, float a0, float a1,
    float a2, float radius) {
    if (!std::isfinite(brightness) || !std::isfinite(range) ||
        !std::isfinite(radius) || !std::isfinite(a0) ||
        !std::isfinite(a1) || !std::isfinite(a2) || brightness <= 0 ||
        range <= 0 || radius <= 0 || a0 < 0 || a1 < 0 || a2 < 0) return 0;
    constexpr double epsilon = 1e-6, threshold = 1.0 / 255.0;
    if (a0 > 0 && brightness / a0 < threshold) return 0;
    double endSquared = double(range) * range;
    if (a2 >= epsilon || a1 > epsilon) {
        double samplingRange = range, low = 0;
        if (a2 < epsilon) {
            if (a1 > epsilon) low = (10.0 - a0) / a1;
        } else {
            const double disc = double(a1) * a1 - 4.0 * a2 * (a0 - 10.0);
            if (disc >= 0) low = (-a1 + std::sqrt(disc)) / (2.0 * a2);
        }
        if (low > 0) samplingRange = std::min(samplingRange, low);
        double numerator = 0, denominator = 0;
        for (int i = 1; i <= 5; ++i) {
            const double d = samplingRange * i / 5.0, d2 = d * d;
            const double attenuation = a2 * d2 + a1 * d + a0;
            if (attenuation <= 0 || d2 <= 0) return 0;
            numerator += (brightness / attenuation) / d2;
            denominator += 1.0 / (d2 * d2);
        }
        if (denominator <= 0) return 0;
        endSquared = numerator / denominator / threshold;
    }
    const double result = .01 * endSquared / (3.14159265358979323846 * radius * radius);
    return std::isfinite(result) && result > 0 && result < 1e12 ? float(result) : 0;
}

// A margin above the nearest fixture exit exposes a useful emitting area;
// merely touching the shell leaves almost all samples blocked.
inline float clearanceRadius(float exitDistance) {
    if (!std::isfinite(exitDistance) || exitDistance < 0) return 8.f;
    return std::clamp(exitDistance * 1.5f + 2.f, 8.f, 24.f);
}
}
