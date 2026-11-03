#pragma once
#include <string>
#include <cstdint>
#include <cstring>

namespace dx_map_sky {
class Selection {
    std::string map;
    std::string observedPath;
    uint32_t generation = 0;
    uintptr_t module = 0;
    bool pending = true;
    unsigned submissionsLeft = 3;
    static std::string normalize(const char* path) {
        std::string normalized;
        // model_t::name is bounded, but do not accept an unbounded external name.
        if (path) {
            size_t length = 0;
            while (length < 256 && path[length]) ++length;
            if (length < 256) normalized.assign(path, length);
        }
        const auto slash = normalized.find_last_of("/\\");
        if (slash != std::string::npos) normalized.erase(0, slash + 1);
        const auto dot = normalized.find_last_of('.');
        if (dot != std::string::npos) normalized.resize(dot);
        if (normalized.size() > 63) normalized.clear();
        bool valid = true;
        for (char& c : normalized) {
            if (c >= 'A' && c <= 'Z') c = char(c + ('a' - 'A'));
            if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) {
                valid = false;
                break;
            }
        }
        if (!valid) normalized.clear();
        return normalized;
    }
public:
    explicit Selection(uint32_t initialGeneration = 0) : generation(initialGeneration) {}
    void loaded(const char* path) {
        observedPath.clear();
        map=normalize(path);
        invalidate();
    }
    bool observeActive(const char* path) {
        if(!path)return false;
        size_t length=0;while(length<256 && path[length])++length;
        if(length==256)return false;
        if(!map.empty() && observedPath.size()==length && !std::memcmp(observedPath.data(),path,length))return false;
        observedPath.assign(path,length);
        const auto active=normalize(path);
        if(active.empty() || active==map)return false;
        loaded(path);return true;
    }
    void invalidate() { ++generation; pending = true;submissionsLeft=3; }
    void disconnected() { if (!map.empty()) loaded(nullptr); }
    const std::string& mapName() const { return map; }
    const char* look() const {
        return map == "c_forest1" ? "COF Forest Black Night" : map == "c_lake" ? "COF Lake Sunrise" :
            (map == "c_ending1" || map == "c_village3") ? "COF Ending Morning" : "COF Night";
    }
    template<class Sink> bool update(uintptr_t loadedModule, Sink&& set) {
        if (module != loadedModule) { module = loadedModule; invalidate(); }
        if (!module || !pending) return false;
        const std::string weather = map.empty() ? "cof_night" : map + ", cof_night";
        const std::string revision = std::to_string(generation);
        // NoSave map IDs select file-backed overrides without overwriting the user's sky sliders.
        // Retry the complete selection if the bridge is not ready yet.
        if (!set("rtx.skyMode", "1") ||
            !set("rtx.weather.followGame", "True") ||
            !set("rtx.weather.gameWeatherOutgoing", "") ||
            !set("rtx.weather.gameWeatherBlend", "-1") ||
            !set("rtx.weather.gameWeather", weather.c_str()) ||
            !set("rtx.weather.preset",look()) ||
            !set("rtx.skyProvider.worldChangeCount", revision.c_str())) return false;
        // Bridge acceptance can precede the server's map/config initialization.
        // Reaffirm twice at the caller's one-second cadence, then remain idle.
        // Reuse the revision so retries do not invalidate sky history again.
        pending = --submissionsLeft!=0;
        return true;
    }
};
}
