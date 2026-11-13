#pragma once
class CBasePlayer;
void CoFR_ServerWorldPrecache();
void CoFR_ServerFrame(CBasePlayer* player);
bool CoFR_ServerCommand(CBasePlayer* player);
bool CoFR_AuthorizedGrant();
bool CoFR_CarriedUnlock(CBasePlayer* player,int type);

bool CoFR_OfflineCheats();
void CoFR_Impulse101(CBasePlayer* player);

void CoFR_CoopStore(CBasePlayer* player);
void CoFR_CoopRestore(CBasePlayer* player);

void CoFR_BoatInput(CBasePlayer* player,unsigned mask);

bool CoFR_RpgPickupAllowed(CBasePlayer*,int);
