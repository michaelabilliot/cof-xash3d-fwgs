#pragma once
#include <cmath>

namespace cof {
// Only a recently observed upward step qualifies. Going down clears the grace
// period immediately; discontinuous time/position changes cannot charge stamina.
struct StairSprintTracker {
    bool primed = false;
    float x = 0, y = 0, z = 0, time = 0, uphillUntil = 0;
    bool update(float now, float nx, float ny, float nz, bool stairs, bool ground) {
        float dt = now - time, dz = nz - z, dx = nx - x, dy = ny - y;
        bool continuous = primed && dt > 0 && dt <= .25f && std::fabs(dz) <= 32.f && dx * dx + dy * dy <= 128.f * 128.f;
        if (!continuous || !stairs || !ground || dz < -.01f) uphillUntil = 0;
        else if (dz > .01f) uphillUntil = now + .2f;
        x = nx; y = ny; z = nz; time = now; primed = true;
        return continuous && stairs && ground && uphillUntil > now;
    }
};
}
