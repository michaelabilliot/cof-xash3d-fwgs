#pragma once

namespace dx_map_light {
inline bool useApi(bool enabled, bool attached, bool directional)
{
    return enabled && !attached && !directional;
}

enum class ApiResult { Unavailable, Submitted, Off };

inline ApiResult apiResult(bool handled, bool hasHandle)
{
    if (!handled)
        return ApiResult::Unavailable;
    return hasHandle ? ApiResult::Submitted : ApiResult::Off;
}
}
