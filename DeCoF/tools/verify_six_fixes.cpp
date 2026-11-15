#include "../src/game_shared/cofr_credits_timing.hpp"
#include "../src/game_shared/cofr_coop_roster.hpp"
#include "../src/game_shared/cofr_hud_layout.hpp"
#include "../src/game_shared/cofr_inventory.h"
#include "../src/game_shared/cofr_notarget.hpp"
#include "../../engine/server/sv_save_tokens.h"

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
                assert(sink.boxes[1].width == std::min(health, cof::hud::healthLimit(cap)) * 314 / 100);
                assert(sink.boxes[2].x == 99 + boundary);
                assert(sink.boxes[2].width == 314 - boundary);
                assert(sink.boxes[2].red < 10);
                int left = 999, right = 0, seamRows = 0;
                for (const auto& box : sink.boxes) {
                    if (box.red != 7 || box.width != 7)
                        continue;
                    left = std::min(left, box.x);
                    right = std::max(right, box.x + box.width);
                    ++seamRows;
                }
                assert(seamRows == 30 && left <= 99 + boundary - 10 && right >= 99 + boundary + 10);
            }
        }
    }
}

void coopRoster()
{
    using namespace cof;
    assert(coopPageCount(4) == 1 && coopPageCount(5) == 2);
    assert(coopPageCount(CoopMaxPlayers) == 6);
    int expected = 1;
    for (int page = 0; page < coopPageCount(CoopMaxPlayers); ++page)
        for (int row = 0; row < CoopRowsPerPage; ++row) {
            const int slot = coopRosterSlot(page, row);
            assert(slot == expected++);
            assert(validCoopSlot(slot, CoopMaxPlayers));
        }
    assert(!validCoopSlot(0, CoopMaxPlayers));
    assert(!validCoopSlot(25, CoopMaxPlayers));
}

void saveTokens()
{
    char world[] = "world";
    char player[] = "player";
    char* tokens[] = {world, nullptr, player};
    char output[15] = {};
    output[14] = '!';
    size_t size = 0;
    assert(SV_EncodeSaveTokens(output, 14, tokens, 3, &size));
    assert(size == 14 && !memcmp(output, "world\0\0player", 14));
    assert(output[14] == '!');
    memset(output, '!', sizeof(output));
    assert(!SV_EncodeSaveTokens(output, 13, tokens, 3, &size));
    assert(output[13] == '!' && output[14] == '!');
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
    coopRoster();
    saveTokens();
    credits();
}
