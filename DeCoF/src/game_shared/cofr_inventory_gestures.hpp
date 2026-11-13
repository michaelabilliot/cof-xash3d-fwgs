// Recovered original Git blob b75f4cb44c9002f57dfa5b1938f0f96a7af163e1.
#pragma once
#include <cmath>
namespace cof {
struct QuickPair {
    int slot=0;
    double time=-1;
    void reset() {slot=0;time=-1;}
    int press(int next,double now) {
        if(next<1||next>4||!std::isfinite(now)){reset();return 0;}
        const int previous=slot;
        const bool pair=previous && previous!=next && now>=time && now-time<=0.45;
        if(pair){reset();return previous;}
        slot=next;time=now;return 0;
    }
};
enum class DragRelease { Cancel, Move, Drop };
inline DragRelease inventoryDragRelease(bool moved,int from,int to,bool unchanged) {
    if(!moved||from<1||from>8||!unchanged||to==from)return DragRelease::Cancel;
    return to>=1&&to<=8?DragRelease::Move:DragRelease::Drop;
}
}
