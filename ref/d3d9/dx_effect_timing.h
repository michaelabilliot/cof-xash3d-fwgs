#pragma once
#include <cmath>

inline bool DX_LightAlive(float radius,float deadline,float now) {
    return std::isfinite(radius) && std::isfinite(deadline) && std::isfinite(now) &&
        radius>1.f && deadline>=now;
}
inline bool DX_FlashAlive(float start,float deadline,float now) {
    return std::isfinite(start) && std::isfinite(deadline) && std::isfinite(now) &&
        now>=start && now<deadline;
}
inline bool DX_MuzzleCrossed(float last,float current,float event,float end,
    bool looping,bool changed,bool restart,float window) {
    constexpr float eps=.001f;
    if(!std::isfinite(last) || !std::isfinite(current) || last<-.5f) return false;
    // A newly observed sequence may already be well past its firing events.
    if(changed) return event>current-window-eps && event<=current+eps;
    if(current+eps<last) {
        if(looping) return last>=end*.75f && current<=end*.25f &&
            (event>last || event<=current+eps);
        return restart && event>current-window-eps && event<=current+eps;
    }
    return event>last+eps && event<=current+eps;
}
