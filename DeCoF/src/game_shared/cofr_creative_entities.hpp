#pragma once
#include <string>

namespace cof {
struct CreativeEntity {
    const char* key;
    const char* classname;
    const char* name;
    const char* model;
    bool modelOverride = false;
    float floorClearance = 0.f;
};

// Model variants are based on authored entity/model pairs from the installed maps.
inline constexpr CreativeEntity CreativeEntities[] = {
    {"monster_baby", "monster_baby", "Baby", "models/baby.mdl"},
    {"monster_child", "monster_child", "Children", "models/children.mdl"},
    {"monster_rcrazy:citalopram", "monster_rcrazy", "Citalopram", "models/citalopram.mdl", true},
    {"monster_crab", "monster_crab", "Crawler", "models/slower2.mdl"},
    {"monster_rcrazy", "monster_rcrazy", "Crazyrunner", "models/runningcrazy.mdl"},
    {"monster_crazybitch", "monster_crazybitch", "Crazy Woman", "models/crazywoman.mdl"},
    {"monster_krypande", "monster_krypande", "Croucher", "models/krypande.mdl"},
    {"monster_slower3:dreamer", "monster_slower3", "Dreamer (Out of It)", "models/outofit/dreamer.mdl", true},
    {"monster_faceless", "monster_faceless", "Faceless", "models/faceless.mdl"},
    {"monster_faceless:second", "monster_faceless", "Faceless 2", "models/Faceless2.mdl", true},
    {"monster_faceless:third", "monster_faceless", "Faceless 3", "models/faceless3.mdl", true},
    {"monster_facelessv", "monster_facelessv", "Faceless (Valve)", "models/twisterv.mdl"},
    {"monster_faster", "monster_faster", "Faster", "models/faster.mdl"},
    {"monster_faster:clown", "monster_faster", "Faster (Clown)", "models/faster_clown.mdl", true},
    {"monster_faster:third", "monster_faster", "Faster 3", "models/faster3.mdl", true},
    {"monster_nerd", "monster_nerd", "Flygare", "models/flygare.mdl", false, 48.f},
    {"monster_faceless:psycho", "monster_faceless", "Psycho", "models/psycho.mdl", true},
    {"monster_faceless:pumpkin", "monster_faceless", "Psycho (Pumpkin)", "models/psychopumpkin.mdl", true},
    {"monster_ruben", "monster_ruben", "Ruben", "models/props/blandat/headcrab.mdl"},
    {"monster_sawcrazy", "monster_sawcrazy", "Sawcrazy", "models/sawcrazy.mdl"},
    {"monster_sawrunner", "monster_sawrunner", "Sawrunner", "models/sawrunner.mdl"},
    {"monster_sawrunner:duo", "monster_sawrunner", "Sawrunner (Duo)", "models/duorunner.mdl", true},
    {"monster_sewmo", "monster_sewmo", "Sewmo", "models/sewmo.mdl"},
    {"monster_slower", "monster_slower", "Slower", "models/slower.mdl", true},
    {"monster_slower3", "monster_slower3", "Slower 3", "models/slower3.mdl"},
    {"monster_slower:tentacled", "monster_slower", "Slower (Tentacled)", "models/tenslower.mdl", true},
    {"monster_slower:unarmed", "monster_slower", "Slower (Unarmed)", "models/slowerno.mdl", true},
    {"monster_slowerstuck", "monster_slowerstuck", "Slower (Stuck)", "models/tenslower2.mdl"},
    {"monster_spitter", "monster_spitter", "Spitter", "models/spitter.mdl"},
    {"monster_stranger", "monster_stranger", "Stranger", "models/stranger.mdl"},
    {"monster_suicider", "monster_suicider", "Suicider", "models/suicider.mdl"},
    {"monster_taller", "monster_taller", "Taller", "models/taller.mdl"},
    {"monster_twitcher", "monster_twitcher", "Twitcher", "models/props/blandat/zombie.mdl"},
    {"monster_twitcher2", "monster_twitcher2", "Twitcher 2", "models/props/blandat/zombie2.mdl"},
    {"monster_twitcher3", "monster_twitcher3", "Twitcher 3", "models/props/blandat/zombie3.mdl"},
    {"monster_twitcher4", "monster_twitcher4", "Twitcher 4", "models/props/blandat/zombie4.mdl"},
    {"monster_slower:upper", "monster_slower", "Upper", "models/upper.mdl", true},
    {"monster_watro", "monster_watro", "Watros", "models/watro.mdl"},
    {"monster_bosschainsaw", "monster_bosschainsaw", "Chainsaw Boss", "models/chainsawguy.mdl"},
    {"monster_sewerboss", "monster_sewerboss", "Sewer Boss", "models/sewer_boss.mdl"},
    {"monster_booksimonsledgehammer", "monster_booksimonsledgehammer", "Book Simon (Sledgehammer)", "models/booksimon.mdl"},
};

inline const CreativeEntity* creativeEntity(const std::string& key)
{
    for (const auto& entry : CreativeEntities)
        if (key == entry.key) return &entry;
    return nullptr;
}
}
