#pragma once

namespace cof {
constexpr int CreditLineHeight = 32;

constexpr float creditPixelsPerSecond(int speed, bool fast)
{
    // Preserve the map parameter's relative pacing with one line/s at speed 1.
    return fast ? 1200.f : CreditLineHeight * 7.f / (5.f + 2.f * (speed > 0 ? speed : 1));
}
}
