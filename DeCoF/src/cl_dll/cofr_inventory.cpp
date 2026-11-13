// Recovered from custom-inventory at repository HEAD; integrated directly into DeCoF.
// Original project notices are retained in CoFR-Tools/recovered-inventory.
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
#include <map>
#include <cmath>
#include <cstdarg>
#include "hud.h"
#include "cl_util.h"
#include "vgui_TeamFortressViewport.h"
#include "RECBInventory_guess.h"
#include "Padlock_guess.h"
#include "com_model.h"
#include "cofr_inventory.h"
#include "cofr_legacy.h"
#include "cofr_creative.hpp"
#include "cofr_creative_entities.hpp"
#include "cofr_ammo_policy.hpp"
#include "keydefs.h"
#include "studio.h"
#include <VGUI_Panel.h>
#include <VGUI_Font.h>
#include <VGUI_SurfaceBase.h>
#undef min
#undef max
#include "cofr_inventory_gestures.hpp"
using namespace cof;
extern int g_iVisibleMouse;
namespace {
static cl_enginefunc_t engine{};
static void (*toggleInventory)();
static pfnUserMsgHook inventoryHook, noteHook, objectiveHook, resetHook, weaponListHook, currentWeaponHook;
static std::string items[9], objectiveTitle, objectiveBody;
static std::map<std::string, int> weaponIds;
static std::map<int, int> weaponClips;
struct Note { bool collected = false; int number = 0, type = 0; std::string fields[11]; };
static Note notes[17];
static bool quickDown[4]{};static cof::QuickPair quickPair;
static int width = 1920, height = 1080;
static vgui::Panel* stockInventory;
struct CreativeEntry { int id=0; CreativeGroup group=CreativeGroup::Item; std::string script; };
static std::vector<CreativeEntry> creativeEntries;
static bool creativeAuthorized=false, creativeNotarget=false,creativeTargetKnown=false,creativeNeedsRefresh=true;
static double creativeRetryAt=0;
static bool creativeVisible() { return creativeAuthorized && engine.pfnGetCvarFloat("sv_cheats")==1.f; }
static void background() { if(CoFR_BlurBackground())return;engine.pfnFillRGBA(0,0,width,height,0,0,0,140); }
static void log(const char* fmt,...) { char b[1024]; va_list a; va_start(a,fmt); vsnprintf(b,sizeof b,fmt,a); va_end(a); engine.Con_Printf("COFR %s\n",b); }
static void send(const std::string& command) { engine.pfnServerCmd(const_cast<char*>((command + "\n").c_str())); }
static std::string metadata(const char* key, const std::string& script) {
    if (script.empty()) return {};
    if (const auto* entity = creativeEntity(script)) {
        if (!strcmp(key, "classname")) return entity->classname;
        if (!strcmp(key, "nice_name")) return entity->name;
        return {};
    }
    static std::map<std::string, std::map<std::string, std::string>> cache;
    auto found = cache.find(script);
    if (found == cache.end()) {
        std::map<std::string, std::string> values;
        int size = 0;
        auto data = engine.COM_LoadFile(const_cast<char*>(script.c_str()), 5, &size);
        if (data) {
            if (size > 0 && size <= 1024 * 1024 && !ItemScript(std::string_view(reinterpret_cast<char*>(data), size)).parse(values)) {
                values.clear(); log("CLIENT: malformed item script rejected once: %s", script.c_str());
            }
            engine.COM_FreeFile(data);
        }
        found = cache.emplace(script, std::move(values)).first;
    }
    auto property = found->second.find(key);
    return property != found->second.end() ? property->second : "";
}
static void cancelDrag();
static void closeInventory() { cancelDrag(); if (stockInventory && stockInventory->isVisible() && toggleInventory) toggleInventory(); }

class InventoryPanel final : public vgui::Panel {
    float scale = 1.f;
    vgui::Font* titleFont = nullptr; vgui::Font* smallFont = nullptr;
    int tab = 0, selected = 0, hover = 0, drag = 0, note = 0, page = 0;
    int mouseX = 0, mouseY = 0, scroll = 0;
    int dragStartX = 0, dragStartY = 0, dragOffsetX = 0, dragOffsetY = 0;
    bool dragVisible = false;
    std::string dragScript;
    int creativeDrag = 0, creativeFilter = 0, creativeScroll = 0;
    bool searchFocused = false;
    std::string search;
    int lastClick = 0;
    float lastClickTime = -1.f;
    std::string lastClickScript;
    static constexpr int SlotSize = 146, SlotPitch = 180;
    static constexpr int CatalogueX = 1140, CatalogueY = 237;
    static constexpr int CatalogueColumns = 4, CatalogueRows = 4;
    static constexpr int CatalogueRight = CatalogueX + 3 * SlotPitch + SlotSize;
    static constexpr int CatalogueBottom = CatalogueY + 3 * SlotPitch + SlotSize;
    struct Texture { int id = 0, width = 0, height = 0; };
    std::map<std::string, Texture> textures;
    std::map<std::string, std::string> documents;
    std::string document(const std::string& filename) {
        if (filename.empty()) return {};
        auto found = documents.find(filename);
        if (found != documents.end()) return found->second;
        int len = 0;
        auto content = engine.COM_LoadFile(const_cast<char*>(filename.c_str()), 5, &len);
        std::string value;
        if (content) { if (len > 0 && len <= 1024 * 1024) value.assign(reinterpret_cast<char*>(content), len); engine.COM_FreeFile(content); }
        documents.emplace(filename, value); return value;
    }
    std::string noteText(const std::string& filename) {
        if (filename.empty()) return {};
        // Native NoteDoc selects these translations by cof_subtitlelanguage.
        const char* languages[] = {"", "", "dutch", "french", "german", "norwegian", "spanish", "swedish"};
        int language = int(engine.pfnGetCvarFloat("cof_subtitlelanguage"));
        if (language > 1 && language < 8) {
            auto translated = document("notes/languages/" + std::string(languages[language]) + "/" + filename);
            if (!translated.empty()) return translated;
        }
        return document("notes/" + filename);
    }
    void turnPage(int delta) {
        if (!note || !notes[note].collected) return;
        int next = page + delta;
        while (next >= 0 && next < 4) {
            if (!notes[note].fields[next].empty() || !notes[note].fields[next + 4].empty()) { page = next; scroll = 0; break; }
            next += delta;
        }
        repaint();
    }
    int px(int n) const { return int(std::lround(n * scale)); }
    void color(int r, int g, int b, int alpha = 0) { drawSetColor(r, g, b, alpha); }
    void rect(int x, int y, int w, int h) { drawFilledRect(px(x), px(y), px(x + w), px(y + h)); }
    void text(int x, int y, const std::string& value, bool compact = false, int shade = 210) {
        drawSetTextFont(compact ? smallFont : titleFont);
        drawSetTextColor(shade, shade, shade, 0); drawSetTextPos(px(x), px(y));
        drawPrintText(value.c_str(), int(value.size()));
    }
    void separator(int y, int thickness = 2) {
        for (int x = 0; x < 886; ++x) {
            color(238, 238, 238, x < 580 ? 5 : std::min(250, (x - 580) * 250 / 306));
            rect(x, y, 1, thickness);
        }
    }
    void roundedBox(int x, int y, int w, int h, int shade, int alpha) {
        color(shade, shade, shade, alpha);
        rect(x + 4, y, w - 8, h); rect(x, y + 4, w, h - 8);
        rect(x + 2, y + 1, w - 4, h - 2); rect(x + 1, y + 2, w - 2, h - 4);
    }
    Texture texture(const std::string& path) {
        auto found = textures.find(path); if (found != textures.end()) return found->second;
        Texture result;
        if (path.empty()) return result;
        int size = 0; auto bytes = engine.COM_LoadFile(const_cast<char*>(path.c_str()), 5, &size);
        if (!bytes || size < 18) { if (bytes) engine.COM_FreeFile(bytes); textures[path] = result; return result; }
        // True-colour TGA, including the stock RLE images. Decode user's assets at runtime.
        const unsigned char* p = bytes; int type = p[2], bpp = p[16] / 8;
        int w = p[12] | p[13] << 8, h = p[14] | p[15] << 8;
        size_t count = size_t(w) * h, pos = 18 + p[0];
        if (p[1] || (type != 2 && type != 10) || (bpp != 3 && bpp != 4) || w < 1 || h < 1 || w > 4096 || h > 4096 || (p[17] & 0x10)) {
            engine.COM_FreeFile(bytes); textures[path] = result; return result;
        }
        std::vector<unsigned char> rgba(count * 4); size_t out = 0; bool good = true;
        auto pixel = [&](const unsigned char* value) {
            size_t x = out % w, y = out / w;
            if (!(p[17] & 0x20)) y = h - 1 - y;
            size_t dest = (y * w + x) * 4;
            rgba[dest] = value[2]; rgba[dest + 1] = value[1]; rgba[dest + 2] = value[0]; rgba[dest + 3] = bpp == 4 ? value[3] : 255;
            ++out;
        };
        while (out < count && good) {
            int run = 1; bool repeat = false;
            if (type == 10) { if (pos >= size_t(size)) { good = false; break; } int code = p[pos++]; run = (code & 127) + 1; repeat = (code & 128) != 0; }
            if (out + run > count || pos + size_t(bpp) * (repeat ? 1 : run) > size_t(size)) { good = false; break; }
            for (int i = 0; i < run; ++i) { pixel(p + pos); if (!repeat) pos += bpp; }
            if (repeat) pos += bpp;
        }
        if (good) {
            result = {getSurfaceBase()->createNewTextureID(), w, h};
            drawSetTextureRGBA(result.id, reinterpret_cast<const char*>(rgba.data()), w, h);
        } else log("CLIENT: malformed TGA rejected: %s", path.c_str());
        engine.COM_FreeFile(bytes); textures[path] = result; return result;
    }
    // Studio textures supply the original enemy artwork where no inventory TGA exists.
    Texture studioArtwork(const std::string& path) {
        auto found = textures.find(path);
        if (found != textures.end()) return found->second;
        Texture result;
        auto load = [&](const std::string& filename) {
            int length = 0;
            const auto* data = engine.COM_LoadFile(const_cast<char*>(filename.c_str()), 5, &length);
            if (!data) return;
            if (length >= int(sizeof(studiohdr_t))) {
                studiohdr_t header;
                memcpy(&header, data, sizeof(header));
                if (!memcmp(data, "IDST", 4) && header.version == 10 &&
                    header.numtextures > 0 && header.numtextures <= 256 &&
                    header.textureindex >= int(sizeof(header)) &&
                    size_t(header.textureindex) + size_t(header.numtextures) * sizeof(mstudiotexture_t) <= size_t(length)) {
                    mstudiotexture_t best{};
                    for (int i = 0; i < header.numtextures; ++i) {
                        mstudiotexture_t candidate;
                        memcpy(&candidate, data + header.textureindex + i * sizeof(candidate), sizeof(candidate));
                        if (candidate.width < 1 || candidate.width > 2048 ||
                            candidate.height < 1 || candidate.height > 2048 || candidate.index < 0 ||
                            (candidate.flags & STUDIO_NF_CHROME)) continue;
                        const int pixels = candidate.width * candidate.height;
                        if (size_t(candidate.index) + pixels + 768 > size_t(length)) continue;
                        if (pixels > best.width * best.height) best = candidate;
                    }
                    if (best.width) {
                        const int pixels = best.width * best.height;
                        const auto* indices = data + best.index;
                        const auto* palette = indices + pixels;
                        std::vector<unsigned char> rgba(size_t(pixels) * 4);
                        for (int i = 0; i < pixels; ++i) {
                            memcpy(&rgba[4 * i], palette + 3 * indices[i], 3);
                            rgba[4 * i + 3] = 255;
                        }
                        result = {getSurfaceBase()->createNewTextureID(), best.width, best.height};
                        drawSetTextureRGBA(result.id, reinterpret_cast<const char*>(rgba.data()), best.width, best.height);
                    }
                }
            }
            engine.COM_FreeFile(const_cast<unsigned char*>(data));
        };
        load(path);
        if (!result.id) load(path.substr(0, path.size() - 4) + "T.mdl");
        textures[path] = result;
        return result;
    }
    void picture(const std::string& path, int x, int y, int w, int h, int alpha = 0) {
        auto t = path.size() > 4 && path.compare(path.size() - 4, 4, ".mdl") == 0 ? studioArtwork(path) : texture(path);
        if (!t.id) return;
        float ratio = std::min(float(w) / t.width, float(h) / t.height);
        int iw = int(t.width * ratio), ih = int(t.height * ratio);
        x += (w - iw) / 2; y += (h - ih) / 2;
        color(255, 255, 255, alpha); drawSetTexture(t.id); drawTexturedRect(px(x), px(y), px(x + iw), px(y + ih));
    }
    int hitSlot(int x, int y) const {
        if (tab != 0) return 0;
        for (int i = 1; i <= Slots; ++i) {
            int sx = 46 + ((i - 1) % 4) * 180, sy = i <= 4 ? 237 : 507;
            if (x >= sx && x < sx + 146 && y >= sy && y < sy + 146) return i;
        }
        return 0;
    }
    static bool inside(int x, int y, int left, int top, int wide, int tall) {
        return x >= left && x < left + wide && y >= top && y < top + tall;
    }
    static std::string folded(std::string value) {
        for (char& c : value) {
            if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
            if (c == '_' || c == '-') c = ' ';
        }
        return value;
    }
    std::vector<int> creativeFiltered() const {
        std::vector<int> filtered;
        const std::string query = folded(search);
        for (int i = 0; i < int(creativeEntries.size()); ++i) {
            const auto& entry = creativeEntries[i];
            if (entry.script.empty() || int(entry.group) != creativeFilter) continue;
            if (!query.empty() && folded(metadata("nice_name", entry.script)).find(query) == std::string::npos &&
                folded(metadata("classname", entry.script)).find(query) == std::string::npos &&
                folded(entry.script).find(query) == std::string::npos) continue;
            filtered.push_back(i);
        }
        return filtered;
    }
    int maxCreativeScroll(size_t count) const {
        return std::max(0, (int(count) + CatalogueColumns - 1) / CatalogueColumns - CatalogueRows);
    }
    bool inCreative(int x, int y) const {
        return tab == 0 && creativeVisible() &&
            inside(x, y, CatalogueX, 20, CatalogueRight - CatalogueX + 14, CatalogueBottom - 20);
    }
    int hitCreative(int x, int y, const std::vector<int>& filtered) const {
        if (!inCreative(x, y) || y < CatalogueY) return -1;
        const int column = (x - CatalogueX) / SlotPitch;
        const int row = (y - CatalogueY) / SlotPitch;
        if (column >= CatalogueColumns || row >= CatalogueRows ||
            (x - CatalogueX) % SlotPitch >= SlotSize || (y - CatalogueY) % SlotPitch >= SlotSize) return -1;
        const int offset = (std::min(creativeScroll, maxCreativeScroll(filtered.size())) + row) * CatalogueColumns + column;
        return offset < int(filtered.size()) ? filtered[offset] : -1;
    }
    std::string clipped(std::string value, int maximum) {
        int wide = 0, tall = 0;
        smallFont->getTextSize(value.c_str(), wide, tall);
        if (wide <= px(maximum)) return value;
        do {
            if (value.empty()) return "...";
            value.pop_back();
            const auto preview = value + "...";
            smallFont->getTextSize(preview.c_str(), wide, tall);
        } while (wide > px(maximum));
        return value + "...";
    }
    void tabBox(int x, const char* label, bool active) {
        color(0, 0, 0, 30); rect(x, 68, 178, 42);
        color(65, 65, 65, 10); drawOutlinedRect(px(x), px(68), px(x + 178), px(110));
        int wide, tall;
        titleFont->getTextSize(label, wide, tall);
        text(x + (178 - int(wide / scale)) / 2, 73, label, false, active ? 238 : 100);
    }
    void slotBox(int x, int y, bool highlighted) {
        roundedBox(x, y, SlotSize, SlotSize, highlighted ? 125 : 90, 15);
        roundedBox(x + 1, y + 1, SlotSize - 2, SlotSize - 2, 33, 65);
    }
    std::string artwork(const std::string& script) {
        if (const auto* entity = creativeEntity(script)) return entity->model;
        return metadata("inventory_picture", script);
    }
    void paintCreative() {
        if (!creativeVisible() || tab != 0) return;
        const auto filtered = creativeFiltered();
        const int maxScroll = maxCreativeScroll(filtered.size());
        creativeScroll = std::min(creativeScroll, maxScroll);
        const char* filters[] = {"WEAPONS", "ITEMS", "ENTITIES"};
        for (int i = 0; i < 3; ++i)
            tabBox(CatalogueX + i * 254, filters[i], i == creativeFilter);
        color(0, 0, 0, 65); rect(CatalogueX, 20, 142, 32);
        color(95, 95, 95, 10); drawOutlinedRect(px(CatalogueX), px(20), px(CatalogueX + 142), px(52));
        text(CatalogueX + 10, 29, creativeNotarget ? "NOTARGET: ON" : "NOTARGET: OFF", true, creativeTargetKnown ? 230 : 130);
        const int searchLeft = CatalogueRight - 204;
        color(0, 0, 0, 65); rect(searchLeft, 20, 204, 32);
        const int border = searchFocused ? 155 : 95;
        color(border, border, border, 10);
        drawOutlinedRect(px(searchLeft), px(20), px(CatalogueRight), px(52));
        text(searchLeft + 9, 29, search.empty() && !searchFocused ? "Search" : clipped(search + (searchFocused ? "|" : ""), 160), true, 225);
        if (!search.empty()) text(CatalogueRight - 20, 29, "X", true);
        const int highlighted = hitCreative(mouseX, mouseY, filtered);
        for (int cell = 0; cell < CatalogueRows * CatalogueColumns; ++cell) {
            const int offset = creativeScroll * CatalogueColumns + cell;
            if (offset >= int(filtered.size())) break;
            const int index = filtered[offset];
            const auto& entry = creativeEntries[index];
            const int x = CatalogueX + (cell % CatalogueColumns) * SlotPitch;
            const int y = CatalogueY + (cell / CatalogueColumns) * SlotPitch;
            slotBox(x, y, index == highlighted);
            picture(artwork(entry.script), x + 4, y + 4, 138, 138,
                    dragVisible && creativeDrag == entry.id ? 155 : 0);
        }
        if (maxScroll) {
            const int track = CatalogueBottom - CatalogueY;
            const int thumb = std::max(30, track * CatalogueRows / (CatalogueRows + maxScroll));
            color(110, 110, 110, 75);
            rect(CatalogueRight + 9, CatalogueY + (track - thumb) * creativeScroll / maxScroll, 3, thumb);
        }
        if (highlighted >= 0 && !dragVisible) {
            const auto name = clipped(metadata("nice_name", creativeEntries[highlighted].script), 245);
            const int x = std::min(mouseX + 16, CatalogueRight - 255);
            const int y = std::min(mouseY + 16, 940);
            color(0, 0, 0, 30); rect(x, y, 255, 24);
            text(x + 6, y + 4, name, true, 240);
        }
    }
    void activate(int slot) {
        if (validSlot(slot) && !items[slot].empty())
            send("cof8_activate " + std::to_string(slot));
    }
    void resetClick() { lastClick = 0; lastClickScript.clear(); }
    bool doubleClick(int target, const std::string& script) {
        const float now = engine.GetClientTime();
        const bool matched = lastClick == target && lastClickScript == script &&
            now >= lastClickTime && now - lastClickTime <= 0.35f;
        if (matched) resetClick();
        else { lastClick = target; lastClickTime = now; lastClickScript = script; }
        return matched;
    }
    bool worldDrop() const {
        if (tab != 0 || mouseX < 0 || mouseY < 0 ||
            mouseX >= width / scale || mouseY >= height / scale || inCreative(mouseX, mouseY)) return false;
        if (inside(mouseX, mouseY, 40, 68, 554, 44) ||
            inside(mouseX, mouseY, 40, 149, 700, 37) ||
            inside(mouseX, mouseY, 40, 420, 700, 38) ||
            inside(mouseX, mouseY, 46, 237, 686, SlotSize) ||
            inside(mouseX, mouseY, 46, 507, 686, SlotSize) ||
            inside(mouseX, mouseY, 46, 981, 124, 28)) return false;
        return true;
    }
    void wrappedText(int x, int y, int maxWidth, const std::string& value, int maxLines = 20) {
        std::string line, word; int printed = 0;
        auto emit = [&]() { if (printed >= scroll && printed < scroll + maxLines) text(x, y + (printed - scroll) * 25, line, true); ++printed; line.clear(); };
        auto addWord = [&]() {
            auto candidate = line.empty() ? word : line + " " + word;
            int wide = 0, tall = 0; smallFont->getTextSize(candidate.c_str(), wide, tall);
            if (!line.empty() && wide > px(maxWidth)) emit();
            if (!line.empty()) line += " "; line += word; word.clear();
        };
        for (char c : value) {
            if (c == '\r') continue;
            if (c == '\n') { addWord(); emit(); }
            else if (c == ' ' || c == '\t') addWord();
            else word += c;
        }
        addWord(); if (!line.empty()) emit();
    }
protected:
    void paint() override {
        if (drag && items[drag] != dragScript) cancelDragging();
        if (creativeDrag && !creativeVisible()) cancelDragging();
        background();
        const char* tabs[] = {"INVENTORY", "NOTES", "OBJECTIVE"};
        const int tx[] = {40, 230, 415};
        for (int i = 0; i < 3; ++i) tabBox(tx[i], tabs[i], tab == i);
        separator(108, 4);
        if (tab == 0) {
            text(40, 149, "QUICK SLOTS"); separator(182);
            text(40, 420, "EXTRA SLOTS"); separator(454);
            for (int i = 1; i <= Slots; ++i) {
                int x = 46 + ((i - 1) % 4) * 180, y = i <= 4 ? 237 : 507;
                slotBox(x, y, i == selected || i == hover);
                if (i <= 4) text(x + 10, y + 11, std::to_string(i), true);
                if (!items[i].empty()) {
                    picture(metadata("inventory_picture", items[i]), x + 4, y + 4, 138, 138, dragVisible && drag == i ? 155 : 0);
                    auto weapon = weaponIds.find(metadata("classname", items[i]));
                    if (weapon != weaponIds.end() && !cof::hideWeaponAmmo(weapon->second)) {
                        auto clip = weaponClips.find(weapon->second);
                        if (clip != weaponClips.end() && clip->second >= 0) text(x + 120, y + 124, std::to_string(clip->second), true, 238);
                    }
                }
            }
            const int detail = hover && !items[hover].empty() ? hover : selected;
            if (!dragVisible && validSlot(detail) && !items[detail].empty()) {
                if (hover == detail) text(46, 682, metadata("nice_name", items[detail]), true);
                wrappedText(46, 710, 690, metadata("description", items[detail]), 4);
            }
        } else if (tab == 1) {
            text(40, 149, "NOTES"); separator(182);
            int row = 0;
            for (int i = 1; i <= 16; ++i) if (notes[i].collected) {
                std::string name = !notes[i].fields[9].empty() ? notes[i].fields[9] : "Note " + std::to_string(i);
                text(46, 215 + row * 40, name, true, i == note ? 245 : 170); ++row;
            }
            if (!row) text(46, 215, "No notes collected", true);
            if (note && notes[note].collected) {
                auto& n = notes[note];
                if (!n.fields[page + 4].empty()) picture("gfx/vgui/" + std::string(width < 640 ? "320_" : "640_") + n.fields[page + 4] + ".tga", 300, 215, 520, 640);
                auto content = noteText(n.fields[page]);
                if (!content.empty()) wrappedText(310, 225, 490, content, 25);
                color(60, 60, 60, 50); rect(310, 866, 120, 35); rect(690, 866, 120, 35);
                text(320, 875, "PREVIOUS", true); text(710, 875, "NEXT", true);
                text(510, 875, "PAGE " + std::to_string(page + 1), true);
                if (n.number) text(315, 907, "Code: " + std::to_string(n.number), true);
            }
        } else {
            text(40, 149, "OBJECTIVE"); separator(182);
            text(46, 215, objectiveTitle);
            wrappedText(46, 270, 740, objectiveBody, 25);
            if (objectiveTitle.empty() && objectiveBody.empty()) text(46, 215, "No current objective", true);
        }
        separator(979, 4);
        color(65, 65, 65, 0); drawOutlinedRect(px(46), px(981), px(170), px(1008));
        text(58, 989, "PRESS TAB TO EXIT", true, 240);
        paintCreative();
        // Paint last so the real item icon follows the grab point above all UI.
        if (tab == 0 && (drag || creativeDrag) && dragVisible)
            picture(artwork(dragScript), mouseX - dragOffsetX, mouseY - dragOffsetY, 138, 138);
    }
    void internalCursorMoved(int x, int y) override {
        mouseX = int(x / scale);
        mouseY = int(y / scale);
        hover = hitSlot(mouseX, mouseY);
        if ((drag || creativeDrag) &&
            (std::abs(mouseX - dragStartX) > 3 || std::abs(mouseY - dragStartY) > 3)) dragVisible = true;
        repaint();
    }
    void internalMousePressed(vgui::MouseCode code) override {
        if (code != vgui::MOUSE_LEFT) return;
        requestFocus();
        cancelDragging();
        searchFocused = false;
        const int tabs[] = {40, 230, 415};
        for (int i = 0; i < 3; ++i) {
            if (inside(mouseX, mouseY, tabs[i], 68, 178, 44)) {
                resetClick(); tab = i; scroll = 0; repaint(); return;
            }
        }
        if (inside(mouseX, mouseY, 46, 981, 124, 28)) { closeInventory(); return; }
        if (tab == 0 && inCreative(mouseX, mouseY)) {
            if (inside(mouseX, mouseY, CatalogueX, 20, 142, 32)) {
                resetClick(); if (creativeTargetKnown) send("cof8_notarget"); return;
            }
            if (inside(mouseX, mouseY, CatalogueRight - 204, 20, 204, 32)) {
                resetClick();
                if (mouseX >= CatalogueRight - 28) search.clear();
                searchFocused = true; creativeScroll = 0; repaint(); return;
            }
            for (int i = 0; i < 3; ++i) {
                if (inside(mouseX, mouseY, CatalogueX + i * 254, 68, 178, 42)) {
                    resetClick(); creativeFilter = i; creativeScroll = 0; repaint(); return;
                }
            }
            const int clicked = hitCreative(mouseX, mouseY, creativeFiltered());
            if (clicked >= 0) {
                const auto& entry = creativeEntries[clicked];
                creativeDrag = entry.id; dragScript = entry.script;
                dragOffsetX = (mouseX - CatalogueX) % SlotPitch - 4;
                dragOffsetY = (mouseY - CatalogueY) % SlotPitch - 4;
            } else resetClick();
        } else if (tab == 0) {
            selected = hitSlot(mouseX, mouseY);
            if (selected && !items[selected].empty()) {
                drag = selected; dragScript = items[drag];
                dragOffsetX = mouseX - (50 + ((drag - 1) % 4) * SlotPitch);
                dragOffsetY = mouseY - (drag <= 4 ? 241 : 511);
            } else resetClick();
        } else if (tab == 1) {
            if (mouseX < 300 && mouseY >= 215) {
                const int target = (mouseY - 215) / 40;
                int row = 0;
                for (int i = 1; i <= 16; ++i)
                    if (notes[i].collected && row++ == target) { note = i; page = scroll = 0; break; }
            }
            if (inside(mouseX, mouseY, 310, 866, 120, 35)) turnPage(-1);
            else if (inside(mouseX, mouseY, 690, 866, 120, 35)) turnPage(1);
        }
        if (drag || creativeDrag) {
            dragStartX = mouseX; dragStartY = mouseY;
            setAsMouseCapture(true);
        }
        repaint();
    }
    void internalMouseReleased(vgui::MouseCode code) override {
        if (code != vgui::MOUSE_LEFT) return;
        const int to = hitSlot(mouseX, mouseY);
        if (creativeDrag && creativeVisible() && creativeDrag <= int(creativeEntries.size())) {
            const auto& entry = creativeEntries[creativeDrag - 1];
            if (entry.id == creativeDrag && entry.script == dragScript) {
                if (dragVisible) {
                    resetClick();
                    if (to && entry.group != CreativeGroup::Entity)
                        send("cof8_create " + std::to_string(entry.id) + " " + std::to_string(to));
                } else {
                    const int hovered = hitCreative(mouseX, mouseY, creativeFiltered());
                    if (hovered >= 0 && creativeEntries[hovered].id == entry.id) {
                        if (entry.group == CreativeGroup::Entity) send("cof8_spawn " + std::to_string(entry.id));
                        else if (doubleClick(-entry.id, entry.script)) send("cof8_create " + std::to_string(entry.id) + " 0");
                    } else resetClick();
                }
            }
        } else if (drag && items[drag] == dragScript) {
            if (dragVisible) {
                resetClick();
                if (to && to != drag) {
                    const char* command = items[to].empty() ? "cof8_move " : "cof8_combine ";
                    send(command + std::to_string(drag) + " " + std::to_string(to));
                } else if (!to && worldDrop()) send("cof8_drop " + std::to_string(drag));
            } else if (to == drag && doubleClick(drag, dragScript)) activate(drag);
            else if (to != drag) resetClick();
        }
        cancelDragging(); repaint();
    }
    void internalMouseDoublePressed(vgui::MouseCode code) override { internalMousePressed(code); }
    void internalMouseWheeled(int delta) override {
        if (inCreative(mouseX, mouseY) && !drag && !creativeDrag) {
            resetClick();
            creativeScroll = std::clamp(creativeScroll - delta, 0, maxCreativeScroll(creativeFiltered().size()));
        } else if (tab != 0) scroll = std::max(0, scroll - delta * 3);
        repaint();
    }
    void internalKeyPressed(vgui::KeyCode key) override {
        if (key == vgui::KEY_TAB || key == vgui::KEY_ESCAPE) closeInventory();
        else if (searchFocused) return;
        else if (key == vgui::KEY_ENTER) activate(selected);
        else if (key == vgui::KEY_DELETE && validSlot(selected) && !items[selected].empty())
            send("cof8_drop " + std::to_string(selected));
        else if (tab == 1 && key == vgui::KEY_LEFT) turnPage(-1);
        else if (tab == 1 && key == vgui::KEY_RIGHT) turnPage(1);
    }
public:
    bool searchKey(int down, int key) {
        if (!searchFocused || !creativeVisible() || key == K_TAB || key == K_ESCAPE) return false;
        if (down) {
            if (key == K_BACKSPACE && !search.empty()) search.pop_back();
            else if (key == K_DEL) search.clear();
            else if (key == K_ENTER) return true;
            else if (key >= 32 && key <= 126 && search.size() < 48) search += char(key);
            creativeScroll = 0; repaint();
        }
        return true;
    }
    void cancelDragging() {
        if (drag || creativeDrag) setAsMouseCapture(false);
        drag = creativeDrag = 0; dragVisible = false; dragScript.clear();
    }
    void closeInteraction() {
        cancelDragging(); resetClick(); searchFocused = false;
    }
    InventoryPanel(): vgui::Panel(0, 0, width, height) {
        setPaintBackgroundEnabled(false); setCursor(vgui::Scheme::scu_arrow); resize();
    }
    void resize() {
        float next = std::min(float(height) / 1080.f, float(width) / 1920.f);
        if (!titleFont || scale != next) {
            cancelDragging();
            scale = next;
            // VGUI retains references to font objects. Retain old fonts until DLL unload.
            titleFont = new vgui::Font("Arial", std::max(12, px(25)), 0, 0, 400, false, false, false, false);
            smallFont = new vgui::Font("Arial", std::max(8, px(13)), 0, 0, 400, false, false, false, false);
        }
        setBounds(0, 0, width, height);
    }
};
static InventoryPanel* panel;
static void cancelDrag() { if (panel) panel->closeInteraction(); }
static int onCreative(const char*,int size,void* data) {
    CreativePacket packet;
    if (size<0 || !creativePacket(static_cast<const unsigned char*>(data),size_t(size),packet)) return 1;
    if (packet.kind==0) {
        creativeNeedsRefresh=false;
        cancelDrag(); creativeAuthorized=packet.enabled; creativeEntries.clear();
        creativeEntries.resize(packet.enabled?packet.count:0);
    } else if (packet.kind==1) {
        if (creativeAuthorized && packet.id<=int(creativeEntries.size()))
            creativeEntries[packet.id-1]={packet.id,packet.group,std::move(packet.value)};
    } else if (packet.kind == 3) { creativeNotarget = packet.notarget; creativeTargetKnown = true; }
    if (panel) panel->repaint();
    return 1;
}
static void updatePanel() {
    if (!gViewPort) return;
    vgui::Panel* parent = gViewPort->m_pRECBInventory_guess;
    if (!parent) return;
    if (!panel || stockInventory != parent) {
        stockInventory = parent;
        panel = new InventoryPanel; panel->setParent(parent);
        log("CLIENT: custom VGUI panel attached to native inventory %p", parent);
    }
    SCREENINFO info{}; info.iSize = sizeof info;
    if (engine.pfnGetScreenInfo(&info)) { width = info.iWidth; height = info.iHeight; }
    if (parent->isVisible()) {
        parent->setBounds(0, 0, width, height);
        parent->setPaintEnabled(false); parent->setPaintBackgroundEnabled(false); parent->setPaintBorderEnabled(false);
        for (int i = 0; i < parent->getChildCount(); ++i) { auto child = parent->getChild(i); if (child != panel) child->setVisible(false); }
        panel->resize(); panel->setVisible(true); panel->repaint();
    } else panel->closeInteraction();
}
static int onInventory(const char* name, int size, void* data) {
    Reader r(data, size); int slot = r.byte(); r.byte(); auto script = r.string(); r.byte(); r.byte(); r.byte();
    if (r.ok && validSlot(slot)) items[slot] = script;
    // The original client accessor has no storage for 7/8 and would overwrite memory.
    if (slot > 6) return 1;
    return inventoryHook ? inventoryHook(name, size, data) : 1;
}
static int onObjectives(const char* name, int size, void* data) {
    Reader r(data, size); auto title = r.string(), body = r.string(); r.byte();
    if (r.ok) { objectiveTitle = title; objectiveBody = body; }
    return objectiveHook ? objectiveHook(name, size, data) : 1;
}
static int onNote(const char* name, int size, void* data) {
    Reader r(data, size); int index = r.byte(); Note value;
    r.byte(); value.number = r.shortValue(); value.type = r.byte();
    for (auto& field : value.fields) field = r.string();
    for (const auto& field : value.fields) if (!field.empty()) value.collected = true;
    if (r.ok && index >= 1 && index <= 16) notes[index] = value;
    return noteHook ? noteHook(name, size, data) : 1;
}
static int onWeaponList(const char* name, int size, void* data) {
    // Ghidra client 0x10008620: name, six byte properties, signed id, flags.
    Reader r(data, size); auto classname = r.string();
    for (int i = 0; i < 6; ++i) r.byte();
    int id = static_cast<signed char>(r.byte()); r.byte();
    if (r.ok && id > 0) weaponIds[classname] = id;
    return weaponListHook ? weaponListHook(name, size, data) : 1;
}
static int onCurrentWeapon(const char* name, int size, void* data) {
    // Ghidra client 0x10006ff0: unsigned state, signed id and signed clip.
    Reader r(data, size); r.byte(); int id = static_cast<signed char>(r.byte()), clip = static_cast<signed char>(r.byte());
    if (r.ok && id > 0) weaponClips[id] = clip < -1 ? -clip : clip;
    return currentWeaponHook ? currentWeaponHook(name, size, data) : 1;
}
static int onReset(const char* name, int size, void* data) {
    cancelDrag();quickPair.reset();creativeNeedsRefresh=true;creativeRetryAt=0;creativeTargetKnown=false;
    creativeAuthorized=false; creativeEntries.clear();
    for (auto& item : items) item.clear();
    for (auto& n : notes) n = Note{};
    objectiveTitle.clear(); objectiveBody.clear(); memset(quickDown, 0, sizeof quickDown);
    weaponClips.clear();
    return resetHook ? resetHook(name, size, data) : 1;
}
static int hookMessage(char* name, pfnUserMsgHook callback) {
    if (!strcmp(name, "Inventory")) { inventoryHook = callback; callback = onInventory; }
    else if (!strcmp(name, "Objectives")) { objectiveHook = callback; callback = onObjectives; }
    else if (!strcmp(name, "AddNote")) { noteHook = callback; callback = onNote; }
    else if (!strcmp(name, "ResetHUD")) { resetHook = callback; callback = onReset; }
    else if (!strcmp(name, "WeaponList")) { weaponListHook = callback; callback = onWeaponList; }
    else if (!strcmp(name, "CurWeapon")) { currentWeaponHook = callback; callback = onCurrentWeapon; }
    return engine.pfnHookUserMsg(name, callback);
}
static void inventoryStatus() {
    log("inventory visible=%d parent=%p panel=%p", stockInventory ? stockInventory->isVisible() : -1, stockInventory, panel);
}
static void openInventory() {
    quickPair.reset();
    if (toggleInventory) toggleInventory();
    updatePanel(); if (stockInventory && stockInventory->isVisible()) { send("cof8_sync"); send("cof8_creative"); }
}
static int addCommand(char* name, void(*callback)()) {
    if (!strcmp(name, "+inventory")) { toggleInventory = callback; callback = openInventory; }
    return engine.pfnAddCommand(name, callback);
}

}
void CoFR_ClientInitialize() { engine=gEngfuncs; gEngfuncs.pfnHookUserMsg=hookMessage; gEngfuncs.pfnAddCommand=addCommand; }
void CoFR_ClientInit() {CoFR_LegacyInit(); engine.pfnRegisterVariable("cofr_render_trace","0",0); engine.pfnAddCommand(const_cast<char*>("cofr_inventory_status"),inventoryStatus); engine.pfnHookUserMsg(const_cast<char*>(CreativeMessage),onCreative); }
void CoFR_ClientFrame() {
 double time=engine.GetClientTime();if(creativeNeedsRefresh&&engine.pfnGetCvarFloat("sv_cheats")==1.f&&engine.pfnGetLevelName&&*engine.pfnGetLevelName()&&(time<creativeRetryAt-2||time>=creativeRetryAt)){creativeRetryAt=time+1;send("cof8_creative_reset");}
 updatePanel();CoFR_LegacyFrame(); }
bool CoFR_ClientKey(int down, int key) {
    const bool inventoryOpen = stockInventory && stockInventory->isVisible();
    if (inventoryOpen) {
        quickPair.reset();
        if (key >= '1' && key <= '4') quickDown[key - '1'] = down != 0;
        if (key == K_MWHEELUP || key == K_MWHEELDOWN) return true;
        if (panel && panel->searchKey(down, key)) return true;
        if (key >= '1' && key <= '4') return true;
    }
    if (key < '1' || key > '4') return false;
    const int i = key - '1';
    const bool wasDown = quickDown[i];
    quickDown[i] = down != 0;
    if (g_iVisibleMouse || engine.Con_IsVisible()) {
        quickPair.reset();
        return false;
    }
    if (down && !wasDown) {
        if (items[i + 1].empty()) quickPair.reset();
        else {
            const int other = quickPair.press(i + 1, engine.GetClientTime());
            if (other && !items[other].empty())
                send("cof8_quick " + std::to_string(other) + " " + std::to_string(i + 1));
            else send("cof8_toggle " + std::to_string(i + 1));
        }
    }
    return true;
}
void CoFR_ClientShutdown() {CoFR_LegacyShutdown(); cancelDrag(); if(panel){panel->setParent(nullptr);delete panel;} panel=nullptr;stockInventory=nullptr; }

void CoFR_EntityTrace(cl_entity_s* entity,int accepted,const char* reason) {
 if(!entity||!engine.pfnGetCvarFloat||engine.pfnGetCvarFloat("cofr_render_trace")==0)return;
 static float last[8192]={};int index=entity->index;if(index<1||index>=8192)return;
 float now=engine.GetClientTime();if(now>=last[index]&&now-last[index]<1.f)return;last[index]=now;
 const auto& s=entity->curstate;
 engine.Con_Printf("COFR CLIENT entity=%d accepted=%d reason=%s model=%s modelindex=%d type=%d sequence=%d frame=%g body=%d skin=%d fx=%d effects=%d mode=%d alpha=%d origin=%g,%g,%g\n",index,accepted,reason,entity->model?entity->model->name:"none",s.modelindex,entity->model?int(entity->model->type):-1,s.sequence,s.frame,s.body,s.skin,s.renderfx,s.effects,s.rendermode,s.renderamt,entity->origin[0],entity->origin[1],entity->origin[2]);
}
