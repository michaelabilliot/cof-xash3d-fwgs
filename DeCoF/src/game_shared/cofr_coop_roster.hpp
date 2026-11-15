#pragma once

namespace cof {
constexpr int CoopRowsPerPage = 4;
constexpr int CoopMaxPlayers = 24;

constexpr bool validCoopSlot(int slot, int maxClients)
{
    return slot >= 1 && slot <= maxClients;
}

constexpr int coopPageCount(int maxClients)
{
    return maxClients > 0 ? (maxClients + CoopRowsPerPage - 1) / CoopRowsPerPage : 1;
}

constexpr int coopRosterSlot(int page, int row)
{
    return page * CoopRowsPerPage + row + 1;
}
}
