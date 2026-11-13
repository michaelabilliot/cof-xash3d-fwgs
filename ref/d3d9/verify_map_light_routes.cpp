#include "dx_map_light_route.h"
#include "dx_light_lifetime.h"

#include <cstdio>

int main()
{
    using namespace dx_map_light;
    if (!useApi(true, false, false) || useApi(false, false, false) ||
        useApi(true, true, false) || useApi(true, false, true))
        return 1;

    if (apiResult(false, false) != ApiResult::Unavailable ||
        apiResult(true, true) != ApiResult::Submitted ||
        apiResult(true, false) != ApiResult::Off)
        return 2;

    LifetimeIds lifetimes;
    constexpr uint64_t source = 0x123456789abcdef0ull;
    uint64_t previous = lifetimes.next(source);
    for (int i = 0; i < 1024; ++i)
    {
        const uint64_t next = lifetimes.next(source);
        if (next <= previous || uint32_t(next) != uint32_t(previous))
            return 3;
        previous = next;
    }
    std::puts("PASS: static/API and moving/D3D9 routes, API fallback and off, unique deferred-destruction handles");
}
