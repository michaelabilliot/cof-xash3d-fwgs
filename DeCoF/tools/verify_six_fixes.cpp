#include "../src/game_shared/cofr_credits_timing.hpp"
#include "../src/game_shared/cofr_hud_layout.hpp"
#include "../src/game_shared/cofr_inventory.h"
#include "../src/game_shared/cofr_notarget.hpp"

#include <cassert>
#include <vector>

namespace {
struct Box {
    int x, width, red;
};

struct HealthSink {
    std::vector<Box> boxes;
    void box(int x, int, int width, int, int red, int, int, int) { boxes.push_back({x, width, red}); }
    void outline(int, int, int, int, int, int, int, int) {}
    void icon(const char*, int, int, int, int, int, int) {}
    void text(int, int, const std::string&, int, int) {}
};

void inventory()
{
    int capacity = cof::inventoryCapacity(false, false);
    assert(capacity == 8 && cof::validSlot(8, capacity));
    capacity = cof::inventoryCapacity(true, false);
    assert(capacity == 4 && cof::validSlot(4, capacity));
    assert(!cof::validSlot(5, capacity) && !cof::validSlot(0, capacity));
    assert(cof::inventoryCapacity(true, true) == 8);
}

void notarget()
{
    using namespace cof;
    assert(notargetFlag(NotargetUnset, true));
    assert(!notargetFlag(NotargetUnset, false));
    assert(notargetFlag(notargetPreferenceForPlayer(NotargetUnset, NotargetOn), false));
    assert(notargetFlag(notargetPreferenceForPlayer(NotargetOn, NotargetOff), false));
    assert(!notargetFlag(notargetPreferenceForPlayer(NotargetOff, NotargetOn), true));
}

void healthCap()
{
    cof::hud::State state;
    cof::hud::View view;
    view.healthVisible = true;
    for (int cap : {1, 2, 0}) {
        const auto msg = static_cast<unsigned char>(cap);
        assert(state.consume("HealthCap", 1, &msg));
        view.cap = state.cap;
        for (int height : {720, 1080, 1440}) {
            // Ordinary damage must not move the crack.
            for (int health : {32, 100}) {
                view.health = health;
                HealthSink sink;
                cof::hud::paint(sink, view, height * 16 / 9, height);
                if (!cap) {
                    assert(sink.boxes.size() == 2);
                    continue;
                }
                const int boundary = (cap == 1 ? 80 : 66) * 314 / 100;
                assert(sink.boxes[1].x == 99 + boundary);
                assert(sink.boxes[1].width == 314 - boundary);
                assert(sink.boxes[2].width <= boundary);
                assert(sink.boxes[3].x == 98 + boundary);
                assert(sink.boxes.size() == 13);
            }
        }
    }
}

void credits()
{
    assert(cof::creditPixelsPerSecond(0, false) == 32.f);
    assert(cof::creditPixelsPerSecond(1, false) == 32.f);
    assert(cof::creditPixelsPerSecond(2, false) < 32.f);
    assert(cof::creditPixelsPerSecond(2, true) == 1200.f);
    for (int fps : {24, 60, 144}) {
        double pixels = 0;
        for (int frame = 0; frame < fps; ++frame)
            pixels += cof::creditPixelsPerSecond(1, false) / double(fps);
        assert(std::abs(pixels - cof::CreditLineHeight) < 1.e-9);
    }
}
}

int main()
{
    inventory();
    notarget();
    healthCap();
    credits();
}
