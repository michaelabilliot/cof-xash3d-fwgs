// Recovered original custom-inventory source, Git blob 3a167d4bb8b19102433a7ada82c90e73ca279fd7.
#pragma once
#include <algorithm>
#include <cmath>
#include <string_view>
namespace cof::rpg {
inline constexpr int Id=31, TotalRounds=4;
inline constexpr const char* Class="weapon_rpg7";
inline constexpr const char* AmmoClass="ammo_rpg7";
inline constexpr const char* Script="inventoryitems/weapons/weapon_rpg7.txt";
inline constexpr const char* AmmoScript="inventoryitems/weapons/ammo_rpg7.txt";
inline constexpr const char* AmmoName="cof8_rpg7_rocket";
inline constexpr const char* View="models/weapons/rpg7/v_rpg7.mdl";
inline constexpr const char* World="models/weapons/rpg7/w_rpg7.mdl";
inline constexpr const char* PlayerModel="models/weapons/rpg7/p_rpg7.mdl";
inline constexpr const char* RocketModel="models/weapons/rpg7/rocket.mdl";
inline constexpr const char* RocketClass="cof8_rpg7_rocket";
inline constexpr const char* Message="Cof8Rpg";
inline bool blocksDual(std::string_view a,std::string_view b) {return a==Script || b==Script;}
inline bool validAmmo(int clip,int reserve) {return clip>=0 && clip<=1 && reserve>=0 && reserve<=TotalRounds-clip;}
inline int grantCount(int clip,int reserve,int requested) {
    return validAmmo(clip,reserve) && requested>0 ? std::min(requested,TotalRounds-clip-reserve):0;
}
inline bool transfer(int& clip,int& reserve) {
    if(!validAmmo(clip,reserve)||clip||!reserve)return false;
    --reserve;++clip;return true;
}
inline bool consume(int& clip,int reserve) {
    if(!validAmmo(clip,reserve)||clip!=1)return false;
    clip=0;return true;
}
}
