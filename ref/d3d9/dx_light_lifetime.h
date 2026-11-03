#pragma once
#include <cstdint>

namespace dx_map_light {
// Remix CreateLight uses the hash as its handle. DestroyLight is deferred
// until Present, so a handle must never be reused by a replacement instance.
class LifetimeIds {
    uint32_t serial = 0;
public:
    uint64_t next(uint64_t source) {
        if (serial == UINT32_MAX) return 0;
        return (uint64_t(++serial) << 32) | uint32_t(source ^ (source >> 32));
    }
};
}
