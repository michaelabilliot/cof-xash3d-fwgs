#pragma once

namespace cof {
enum NotargetPreference { NotargetUnset, NotargetOn, NotargetOff };

constexpr int notargetPreferenceForPlayer(int session, int saved)
{
    return session == NotargetUnset ? saved : session;
}

constexpr bool notargetFlag(int preference, bool current)
{
    return preference == NotargetUnset ? current : preference == NotargetOn;
}
}
