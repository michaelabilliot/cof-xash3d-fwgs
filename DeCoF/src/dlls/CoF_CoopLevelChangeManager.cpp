#include "cofr_features.h"
//=========================================================
// Coop level change manager: keeps the weapons, ammo and
// statistics of every player across a level change
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "player.h"

extern int gmsgAmmoX;
extern int gmsgSyringe;
extern int gmsgQSlotUpdate;

extern void ApplyCFGSettingToPlayerLate( CBasePlayer *pPlayer );

// CoF: declared in a header in the original source (the 1.55 PDB
// names no header for it); client.cpp and world.cpp use the same layout
struct playervariables
{
	bool m_bUsed;
	int m_iAllowedToUseTMP;
	char *szPlayerName;
	char *szLastUsedWeapon;
	int m_iLastUsedWeaponID;
	bool m_bIsADonator;
	int m_iAmmoInventory[6];
	int m_iAmmoInventoryAmount[6];
	int m_iPlayerHealth;
	int m_iPlayerStamina;
	char szPlayerQuickSlot1[128];
	char szPlayerQuickSlot2[128];
	char szPlayerQuickSlot3[128];
	bool m_bWasDead;
	int suvivalpoints;
	int shots_fired;
	int shots_hit;
	int syringes_used;
	int shots_missed;
	float gunaccuracy;
	int timessaved;
	int weaponscollected;
	int damagetaken;
	float distancetravelled_metres;
	float distancetravelled_kilometres;
	int time_seconds;
	int time_minutes;
	int time_hours;
	int monsters_killed_melee;
	int monsters_killed_shooting;
	bool started_from_the_beginning;
	int chosen_difficulty;
	bool doctor_mode;
	int m_iQuickSlotSlot[4];
	char *szWeapon1;
	int iClip1;
	int iAmmo1;
	char *szWeapon2;
	int iClip2;
	int iAmmo2;
	char *szWeapon3;
	int iClip3;
	int iAmmo3;
	char *szWeapon4;
	int iClip4;
	int iAmmo4;
	char *szWeapon5;
	int iClip5;
	int iAmmo5;
	char *szWeapon6;
	int iClip6;
	int iAmmo6;
	char szQuickSel1[128];
	char szQuickSel2[128];
	char szQuickSel3[128];
	bool m_bFirstDeploy[33];
};

class CCoopLevelChangeManager
{
public:
	CCoopLevelChangeManager( void );
	void GiveWeaponFromInventory( char *weapon, int slot, int i, CBasePlayer *pPlayer );
	void SetPlayerVariables( CBasePlayer *pPlayer );
	bool GivePlayerVariables( CBasePlayer *pPlayer );
	bool m_bJustHadGameOver;
	playervariables playervariablestruct[33];
	playervariables playervariablestructstats[33];
};

CCoopLevelChangeManager *g_pLevelChange;

CCoopLevelChangeManager::CCoopLevelChangeManager( void )
{
	memset( playervariablestruct, 0, sizeof( playervariablestruct ) );
	memset( playervariablestructstats, 0, sizeof( playervariablestructstats ) );
}

// remember the clip and ammo of the weapon of quick slot 'slot'
void CCoopLevelChangeManager::GiveWeaponFromInventory( char *weapon, int slot, int i, CBasePlayer *pPlayer )
{
	for ( int j = 0; j < MAX_ITEM_TYPES; j++ )
	{
		if ( pPlayer->m_rgpPlayerItems[j] && !FClassnameIs( pPlayer->m_rgpPlayerItems[j]->pev, "weapon_action" ) )
		{
			CBasePlayerItem *pPlayerItem = pPlayer->m_rgpPlayerItems[j];

			while ( pPlayerItem && STRING( pPlayerItem->pev->classname ) && strlen( STRING( pPlayerItem->pev->classname ) ) > 1 )
			{
				if ( strstr( STRING( pPlayerItem->pev->classname ), weapon ) )
				{
					switch ( slot )
					{
					case 1:
						if ( !FClassnameIs( pPlayer->m_rgpPlayerItems[j]->pev, "weapon_action" ) )
						{
							playervariablestruct[i].szWeapon1 = (char *)STRING( pPlayerItem->pev->classname );
							playervariablestruct[i].iClip1 = ((CBasePlayerWeapon *)pPlayerItem)->m_iClip;
							playervariablestruct[i].iAmmo1 = pPlayer->m_rgAmmo[((CBasePlayerWeapon *)pPlayerItem)->m_iPrimaryAmmoType];
							return;
						}
						break;

					case 2:
						if ( !FClassnameIs( pPlayer->m_rgpPlayerItems[j]->pev, "weapon_action" ) )
						{
							playervariablestruct[i].szWeapon2 = (char *)STRING( pPlayerItem->pev->classname );
							playervariablestruct[i].iClip2 = ((CBasePlayerWeapon *)pPlayerItem)->m_iClip;
							playervariablestruct[i].iAmmo2 = pPlayer->m_rgAmmo[((CBasePlayerWeapon *)pPlayerItem)->m_iPrimaryAmmoType];
							return;
						}
						break;

					case 3:
						if ( !FClassnameIs( pPlayer->m_rgpPlayerItems[j]->pev, "weapon_action" ) )
						{
							playervariablestruct[i].szWeapon3 = (char *)STRING( pPlayerItem->pev->classname );
							playervariablestruct[i].iClip3 = ((CBasePlayerWeapon *)pPlayerItem)->m_iClip;
							playervariablestruct[i].iAmmo3 = pPlayer->m_rgAmmo[((CBasePlayerWeapon *)pPlayerItem)->m_iPrimaryAmmoType];
							return;
						}
						break;

					case 4:
						if ( !FClassnameIs( pPlayer->m_rgpPlayerItems[j]->pev, "weapon_action" ) )
						{
							playervariablestruct[i].szWeapon4 = (char *)STRING( pPlayerItem->pev->classname );
							playervariablestruct[i].iClip4 = ((CBasePlayerWeapon *)pPlayerItem)->m_iClip;
							playervariablestruct[i].iAmmo4 = pPlayer->m_rgAmmo[((CBasePlayerWeapon *)pPlayerItem)->m_iPrimaryAmmoType];
							return;
						}
						break;

					case 5:
						if ( !FClassnameIs( pPlayer->m_rgpPlayerItems[j]->pev, "weapon_action" ) )
						{
							playervariablestruct[i].szWeapon5 = (char *)STRING( pPlayerItem->pev->classname );
							playervariablestruct[i].iClip5 = ((CBasePlayerWeapon *)pPlayerItem)->m_iClip;
							playervariablestruct[i].iAmmo5 = pPlayer->m_rgAmmo[((CBasePlayerWeapon *)pPlayerItem)->m_iPrimaryAmmoType];
							return;
						}
						break;

					case 6:
						if ( !FClassnameIs( pPlayer->m_rgpPlayerItems[j]->pev, "weapon_action" ) )
						{
							playervariablestruct[i].szWeapon6 = (char *)STRING( pPlayerItem->pev->classname );
							playervariablestruct[i].iClip6 = ((CBasePlayerWeapon *)pPlayerItem)->m_iClip;
							playervariablestruct[i].iAmmo6 = pPlayer->m_rgAmmo[((CBasePlayerWeapon *)pPlayerItem)->m_iPrimaryAmmoType];
							return;
						}
						break;
					}
				}

				pPlayerItem = pPlayerItem->m_pNext;
			}
		}
	}
}

// store the player before the level change
void CCoopLevelChangeManager::SetPlayerVariables( CBasePlayer *pPlayer )
{
	if ( !pPlayer )
		return;

	m_bJustHadGameOver = false;
	CoFR_CoopStore(pPlayer);

	for ( int i = 1; i <= 32; i++ )
	{
		if ( !playervariablestruct[i].m_bUsed )
		{
			if ( !pPlayer->IsAlive() )
			{
				playervariablestruct[i].m_bIsADonator = pPlayer->IsDonator();

				playervariablestruct[i].m_bWasDead = true;
				playervariablestruct[i].shots_fired = pPlayer->playerstats.shots_fired;
				playervariablestruct[i].shots_hit = pPlayer->playerstats.shots_hit;
				playervariablestruct[i].syringes_used = pPlayer->playerstats.syringes_used;
				playervariablestruct[i].shots_missed = pPlayer->playerstats.shots_missed;
				playervariablestruct[i].gunaccuracy = pPlayer->playerstats.gunaccuracy;
				playervariablestruct[i].timessaved = pPlayer->playerstats.timessaved;
				playervariablestruct[i].weaponscollected = pPlayer->playerstats.weaponscollected;
				playervariablestruct[i].damagetaken = pPlayer->playerstats.damagetaken;
				playervariablestruct[i].distancetravelled_metres = pPlayer->playerstats.distancetravelled_metres;
				playervariablestruct[i].distancetravelled_kilometres = pPlayer->playerstats.distancetravelled_kilometres;
				playervariablestruct[i].time_seconds = pPlayer->playerstats.time_seconds;
				playervariablestruct[i].time_minutes = pPlayer->playerstats.time_minutes;
				playervariablestruct[i].time_hours = pPlayer->playerstats.time_hours;
				playervariablestruct[i].monsters_killed_melee = pPlayer->playerstats.monsters_killed_melee;
				playervariablestruct[i].monsters_killed_shooting = pPlayer->playerstats.monsters_killed_shooting;
				playervariablestruct[i].started_from_the_beginning = pPlayer->playerstats.started_from_the_beginning;
				playervariablestruct[i].chosen_difficulty = pPlayer->playerstats.chosen_difficulty;
				playervariablestruct[i].doctor_mode = pPlayer->playerstats.doctor_mode;

				playervariablestruct[i].m_bUsed = true;
				playervariablestruct[i].szPlayerName = (char *)STRING( pPlayer->pev->netname );

				playervariablestructstats[i] = playervariablestruct[i];
				return;
			}

			playervariablestruct[i].m_bIsADonator = pPlayer->IsDonator();

			playervariablestruct[i].m_bWasDead = false;
			playervariablestruct[i].szPlayerName = (char *)STRING( pPlayer->pev->netname );
			playervariablestruct[i].m_iPlayerHealth = pPlayer->pev->health;
			playervariablestruct[i].m_iPlayerStamina = pPlayer->m_fStamina;
			playervariablestruct[i].m_bUsed = true;

			playervariablestruct[i].m_iAllowedToUseTMP = pPlayer->m_iAllowedToUseTMP;

			playervariablestruct[i].shots_fired = pPlayer->playerstats.shots_fired;
			playervariablestruct[i].shots_hit = pPlayer->playerstats.shots_hit;
			playervariablestruct[i].syringes_used = pPlayer->playerstats.syringes_used;
			playervariablestruct[i].shots_missed = pPlayer->playerstats.shots_missed;
			playervariablestruct[i].gunaccuracy = pPlayer->playerstats.gunaccuracy;
			playervariablestruct[i].timessaved = pPlayer->playerstats.timessaved;
			playervariablestruct[i].weaponscollected = pPlayer->playerstats.weaponscollected;
			playervariablestruct[i].damagetaken = pPlayer->playerstats.damagetaken;
			playervariablestruct[i].distancetravelled_metres = pPlayer->playerstats.distancetravelled_metres;
			playervariablestruct[i].distancetravelled_kilometres = pPlayer->playerstats.distancetravelled_kilometres;
			playervariablestruct[i].time_seconds = pPlayer->playerstats.time_seconds;
			playervariablestruct[i].time_minutes = pPlayer->playerstats.time_minutes;
			playervariablestruct[i].time_hours = pPlayer->playerstats.time_hours;
			playervariablestruct[i].monsters_killed_melee = pPlayer->playerstats.monsters_killed_melee;
			playervariablestruct[i].monsters_killed_shooting = pPlayer->playerstats.monsters_killed_shooting;
			playervariablestruct[i].started_from_the_beginning = pPlayer->playerstats.started_from_the_beginning;
			playervariablestruct[i].chosen_difficulty = pPlayer->playerstats.chosen_difficulty;
			playervariablestruct[i].doctor_mode = pPlayer->playerstats.doctor_mode;

			playervariablestruct[i].m_iQuickSlotSlot[1] = pPlayer->m_iQuickSlotSlot[1];
			playervariablestruct[i].m_iQuickSlotSlot[2] = pPlayer->m_iQuickSlotSlot[2];
			playervariablestruct[i].m_iQuickSlotSlot[3] = pPlayer->m_iQuickSlotSlot[3];

			if ( pPlayer->m_pActiveItem )
			{
				playervariablestruct[i].szLastUsedWeapon = (char *)STRING( pPlayer->m_pActiveItem->pev->classname );
				playervariablestruct[i].m_iLastUsedWeaponID = pPlayer->m_pActiveItem->m_iId;
			}

			for ( int k = 1; k <= 32; k++ )
				playervariablestruct[i].m_bFirstDeploy[k] = pPlayer->m_bFirstDeploy[k];

			strcpy( playervariablestruct[i].szQuickSel1, pPlayer->m_sQuickSel1 );
			strcpy( playervariablestruct[i].szQuickSel2, pPlayer->m_sQuickSel2 );
			strcpy( playervariablestruct[i].szQuickSel3, pPlayer->m_sQuickSel3 );

			// the weapons of the inventory
			for ( int p = 1; p <= 8; p++ )
			{
				if ( !pPlayer->IsSlotFree( pPlayer->cofrInventorySlot[p] ) )
				{
					char *weapon = pPlayer->GetDescriptionFromItem( "classname", (char *)STRING( pPlayer->cofrInventorySlot[p] ) );

					if ( weapon && !strncmp( weapon, "weapon_", 7 ) )
						GiveWeaponFromInventory( weapon, p, i, pPlayer );
				}
			}

			return;
		}
	}
}

// give the stored player back after the level change
bool CCoopLevelChangeManager::GivePlayerVariables( CBasePlayer *pPlayer )
{
	if ( !pPlayer )
		return false;

	bool hasfoundme = false;

	int i;

	for ( i = 1; i <= 32; i++ )
	{
		// after a game over only the statistics are kept
		if ( m_bJustHadGameOver && playervariablestructstats[i].m_bUsed && playervariablestructstats[i].szPlayerName == STRING( pPlayer->pev->netname ) )
		{
			pPlayer->playerstats.shots_fired = playervariablestructstats[i].shots_fired;
			pPlayer->playerstats.shots_hit = playervariablestructstats[i].shots_hit;
			pPlayer->playerstats.syringes_used = playervariablestructstats[i].syringes_used;
			pPlayer->playerstats.shots_missed = playervariablestructstats[i].shots_missed;
			pPlayer->playerstats.gunaccuracy = playervariablestructstats[i].gunaccuracy;
			pPlayer->playerstats.timessaved = playervariablestructstats[i].timessaved;
			pPlayer->playerstats.weaponscollected = playervariablestructstats[i].weaponscollected;
			pPlayer->playerstats.damagetaken = playervariablestructstats[i].damagetaken;
			pPlayer->playerstats.distancetravelled_metres = playervariablestructstats[i].distancetravelled_metres;
			pPlayer->playerstats.distancetravelled_kilometres = playervariablestructstats[i].distancetravelled_kilometres;
			pPlayer->playerstats.time_seconds = playervariablestructstats[i].time_seconds;
			pPlayer->playerstats.time_minutes = playervariablestructstats[i].time_minutes;
			pPlayer->playerstats.time_hours = playervariablestructstats[i].time_hours;
			pPlayer->playerstats.monsters_killed_melee = playervariablestructstats[i].monsters_killed_melee;
			pPlayer->playerstats.monsters_killed_shooting = playervariablestructstats[i].monsters_killed_shooting;
			pPlayer->playerstats.started_from_the_beginning = playervariablestructstats[i].started_from_the_beginning;
			pPlayer->playerstats.chosen_difficulty = playervariablestructstats[i].chosen_difficulty;
			pPlayer->playerstats.doctor_mode = playervariablestructstats[i].doctor_mode;

			memset( &playervariablestructstats[i], 0, sizeof( playervariables ) );

			memset( &playervariablestruct[i], 0, sizeof( playervariables ) );
			return true;
		}

		if ( playervariablestruct[i].m_bUsed && playervariablestruct[i].szPlayerName == STRING( pPlayer->pev->netname ) && playervariablestruct[i].m_bWasDead == true )
		{
			hasfoundme = true;

			pPlayer->playerstats.shots_fired = playervariablestruct[i].shots_fired;
			pPlayer->playerstats.shots_hit = playervariablestruct[i].shots_hit;
			pPlayer->playerstats.syringes_used = playervariablestruct[i].syringes_used;
			pPlayer->playerstats.shots_missed = playervariablestruct[i].shots_missed;
			pPlayer->playerstats.gunaccuracy = playervariablestruct[i].gunaccuracy;
			pPlayer->playerstats.timessaved = playervariablestruct[i].timessaved;
			pPlayer->playerstats.weaponscollected = playervariablestruct[i].weaponscollected;
			pPlayer->playerstats.damagetaken = playervariablestruct[i].damagetaken;
			pPlayer->playerstats.distancetravelled_metres = playervariablestruct[i].distancetravelled_metres;
			pPlayer->playerstats.distancetravelled_kilometres = playervariablestruct[i].distancetravelled_kilometres;
			pPlayer->playerstats.time_seconds = playervariablestruct[i].time_seconds;
			pPlayer->playerstats.time_minutes = playervariablestruct[i].time_minutes;
			pPlayer->playerstats.time_hours = playervariablestruct[i].time_hours;
			pPlayer->playerstats.monsters_killed_melee = playervariablestruct[i].monsters_killed_melee;
			pPlayer->playerstats.monsters_killed_shooting = playervariablestruct[i].monsters_killed_shooting;
			pPlayer->playerstats.started_from_the_beginning = playervariablestruct[i].started_from_the_beginning;
			pPlayer->playerstats.chosen_difficulty = playervariablestruct[i].chosen_difficulty;
			pPlayer->playerstats.doctor_mode = playervariablestruct[i].doctor_mode;

			ApplyCFGSettingToPlayerLate( pPlayer );

			for ( i = 0; i < MAX_AMMO_SLOTS; i++ )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgAmmoX, NULL, pPlayer->pev );
					WRITE_BYTE( i );
					WRITE_SHORT( pPlayer->m_rgAmmo[i] );
				MESSAGE_END();
			}

			// CoF: i is MAX_AMMO_SLOTS here, the entry of the player is not cleared (original bug)
			memset( &playervariablestruct[i], 0, sizeof( playervariables ) );
			return true;
		}

		if ( playervariablestruct[i].m_bUsed && playervariablestruct[i].szPlayerName == STRING( pPlayer->pev->netname ) && playervariablestruct[i].m_bWasDead == false )
		{
			hasfoundme = true;

			pPlayer->pev->health = playervariablestruct[i].m_iPlayerHealth;
			pPlayer->m_fStamina = playervariablestruct[i].m_iPlayerStamina;

			pPlayer->m_iAllowedToUseTMP = playervariablestruct[i].m_iAllowedToUseTMP;

			pPlayer->m_iQuickSlotSlot[1] = playervariablestruct[i].m_iQuickSlotSlot[1];
			pPlayer->m_iQuickSlotSlot[2] = playervariablestruct[i].m_iQuickSlotSlot[2];
			pPlayer->m_iQuickSlotSlot[3] = playervariablestruct[i].m_iQuickSlotSlot[3];

			pPlayer->playerstats.shots_fired = playervariablestruct[i].shots_fired;
			pPlayer->playerstats.shots_hit = playervariablestruct[i].shots_hit;
			pPlayer->playerstats.syringes_used = playervariablestruct[i].syringes_used;
			pPlayer->playerstats.shots_missed = playervariablestruct[i].shots_missed;
			pPlayer->playerstats.gunaccuracy = playervariablestruct[i].gunaccuracy;
			pPlayer->playerstats.timessaved = playervariablestruct[i].timessaved;
			pPlayer->playerstats.weaponscollected = playervariablestruct[i].weaponscollected;
			pPlayer->playerstats.damagetaken = playervariablestruct[i].damagetaken;
			pPlayer->playerstats.distancetravelled_metres = playervariablestruct[i].distancetravelled_metres;
			pPlayer->playerstats.distancetravelled_kilometres = playervariablestruct[i].distancetravelled_kilometres;
			pPlayer->playerstats.time_seconds = playervariablestruct[i].time_seconds;
			pPlayer->playerstats.time_minutes = playervariablestruct[i].time_minutes;
			pPlayer->playerstats.time_hours = playervariablestruct[i].time_hours;
			pPlayer->playerstats.monsters_killed_melee = playervariablestruct[i].monsters_killed_melee;
			pPlayer->playerstats.monsters_killed_shooting = playervariablestruct[i].monsters_killed_shooting;
			pPlayer->playerstats.started_from_the_beginning = playervariablestruct[i].started_from_the_beginning;
			pPlayer->playerstats.chosen_difficulty = playervariablestruct[i].chosen_difficulty;
			pPlayer->playerstats.doctor_mode = playervariablestruct[i].doctor_mode;

			for ( int k = 1; k <= 32; k++ )
				pPlayer->m_bFirstDeploy[k] = playervariablestruct[i].m_bFirstDeploy[k];

			strcpy( pPlayer->m_sQuickSel1, playervariablestruct[i].szQuickSel1 );
			strcpy( pPlayer->m_sQuickSel2, playervariablestruct[i].szQuickSel2 );
			strcpy( pPlayer->m_sQuickSel3, playervariablestruct[i].szQuickSel3 );

			if ( playervariablestruct[i].szWeapon1 )
			{
				pPlayer->GiveNamedItem( playervariablestruct[i].szWeapon1, false, playervariablestruct[i].m_bIsADonator );

				if ( !strcmp( playervariablestruct[i].szWeapon1, "weapon_syringe" ) )
				{
					MESSAGE_BEGIN( MSG_ALL, gmsgSyringe );
						WRITE_BYTE( ENTINDEX( pPlayer->edict() ) );
						WRITE_BYTE( 1 );
					MESSAGE_END();
				}

				for ( int p = 0; p < MAX_ITEM_TYPES; p++ )
				{
					if ( pPlayer->m_rgpPlayerItems[p] && !FClassnameIs( pPlayer->m_rgpPlayerItems[p]->pev, "weapon_action" ) )
					{
						CBasePlayerItem *pPlayerItem = pPlayer->m_rgpPlayerItems[p];

						while ( pPlayerItem )
						{
							if ( FClassnameIs( pPlayerItem->pev, playervariablestruct[i].szWeapon1 ) )
							{
								((CBasePlayerWeapon *)pPlayerItem)->m_iClip = playervariablestruct[i].iClip1;
								pPlayer->m_rgAmmo[((CBasePlayerWeapon *)pPlayerItem)->m_iPrimaryAmmoType] = playervariablestruct[i].iAmmo1;
							}

							pPlayerItem = pPlayerItem->m_pNext;
						}
					}
				}
			}

			if ( playervariablestruct[i].szWeapon2 )
			{
				pPlayer->GiveNamedItem( playervariablestruct[i].szWeapon2, false, playervariablestruct[i].m_bIsADonator );

				if ( !strcmp( playervariablestruct[i].szWeapon2, "weapon_syringe" ) )
				{
					MESSAGE_BEGIN( MSG_ALL, gmsgSyringe );
						WRITE_BYTE( ENTINDEX( pPlayer->edict() ) );
						WRITE_BYTE( 1 );
					MESSAGE_END();
				}

				for ( int p = 0; p < MAX_ITEM_TYPES; p++ )
				{
					if ( pPlayer->m_rgpPlayerItems[p] && !FClassnameIs( pPlayer->m_rgpPlayerItems[p]->pev, "weapon_action" ) )
					{
						CBasePlayerItem *pPlayerItem = pPlayer->m_rgpPlayerItems[p];

						while ( pPlayerItem )
						{
							if ( FClassnameIs( pPlayerItem->pev, playervariablestruct[i].szWeapon2 ) )
							{
								((CBasePlayerWeapon *)pPlayerItem)->m_iClip = playervariablestruct[i].iClip2;
								pPlayer->m_rgAmmo[((CBasePlayerWeapon *)pPlayerItem)->m_iPrimaryAmmoType] = playervariablestruct[i].iAmmo2;
							}

							pPlayerItem = pPlayerItem->m_pNext;
						}
					}
				}
			}

			if ( playervariablestruct[i].szWeapon3 )
			{
				pPlayer->GiveNamedItem( playervariablestruct[i].szWeapon3, false, playervariablestruct[i].m_bIsADonator );

				if ( !strcmp( playervariablestruct[i].szWeapon3, "weapon_syringe" ) )
				{
					MESSAGE_BEGIN( MSG_ALL, gmsgSyringe );
						WRITE_BYTE( ENTINDEX( pPlayer->edict() ) );
						WRITE_BYTE( 1 );
					MESSAGE_END();
				}

				for ( int p = 0; p < MAX_ITEM_TYPES; p++ )
				{
					if ( pPlayer->m_rgpPlayerItems[p] && !FClassnameIs( pPlayer->m_rgpPlayerItems[p]->pev, "weapon_action" ) )
					{
						CBasePlayerItem *pPlayerItem = pPlayer->m_rgpPlayerItems[p];

						while ( pPlayerItem )
						{
							if ( FClassnameIs( pPlayerItem->pev, playervariablestruct[i].szWeapon3 ) )
							{
								((CBasePlayerWeapon *)pPlayerItem)->m_iClip = playervariablestruct[i].iClip3;
								pPlayer->m_rgAmmo[((CBasePlayerWeapon *)pPlayerItem)->m_iPrimaryAmmoType] = playervariablestruct[i].iAmmo3;
							}

							pPlayerItem = pPlayerItem->m_pNext;
						}
					}
				}
			}

			if ( playervariablestruct[i].szWeapon4 )
			{
				pPlayer->GiveNamedItem( playervariablestruct[i].szWeapon4, false, playervariablestruct[i].m_bIsADonator );

				if ( !strcmp( playervariablestruct[i].szWeapon4, "weapon_syringe" ) )
				{
					MESSAGE_BEGIN( MSG_ALL, gmsgSyringe );
						WRITE_BYTE( ENTINDEX( pPlayer->edict() ) );
						WRITE_BYTE( 1 );
					MESSAGE_END();
				}

				for ( int p = 0; p < MAX_ITEM_TYPES; p++ )
				{
					if ( pPlayer->m_rgpPlayerItems[p] && !FClassnameIs( pPlayer->m_rgpPlayerItems[p]->pev, "weapon_action" ) )
					{
						CBasePlayerItem *pPlayerItem = pPlayer->m_rgpPlayerItems[p];

						while ( pPlayerItem )
						{
							if ( FClassnameIs( pPlayerItem->pev, playervariablestruct[i].szWeapon4 ) )
							{
								((CBasePlayerWeapon *)pPlayerItem)->m_iClip = playervariablestruct[i].iClip4;
								pPlayer->m_rgAmmo[((CBasePlayerWeapon *)pPlayerItem)->m_iPrimaryAmmoType] = playervariablestruct[i].iAmmo4;
							}

							pPlayerItem = pPlayerItem->m_pNext;
						}
					}
				}
			}

			if ( playervariablestruct[i].szWeapon5 )
			{
				pPlayer->GiveNamedItem( playervariablestruct[i].szWeapon5, false, playervariablestruct[i].m_bIsADonator );

				if ( !strcmp( playervariablestruct[i].szWeapon5, "weapon_syringe" ) )
				{
					MESSAGE_BEGIN( MSG_ALL, gmsgSyringe );
						WRITE_BYTE( ENTINDEX( pPlayer->edict() ) );
						WRITE_BYTE( 1 );
					MESSAGE_END();
				}

				for ( int p = 0; p < MAX_ITEM_TYPES; p++ )
				{
					if ( pPlayer->m_rgpPlayerItems[p] && !FClassnameIs( pPlayer->m_rgpPlayerItems[p]->pev, "weapon_action" ) )
					{
						CBasePlayerItem *pPlayerItem = pPlayer->m_rgpPlayerItems[p];

						while ( pPlayerItem )
						{
							if ( FClassnameIs( pPlayerItem->pev, playervariablestruct[i].szWeapon5 ) )
							{
								((CBasePlayerWeapon *)pPlayerItem)->m_iClip = playervariablestruct[i].iClip5;
								pPlayer->m_rgAmmo[((CBasePlayerWeapon *)pPlayerItem)->m_iPrimaryAmmoType] = playervariablestruct[i].iAmmo5;
							}

							pPlayerItem = pPlayerItem->m_pNext;
						}
					}
				}
			}

			if ( playervariablestruct[i].szWeapon6 )
			{
				pPlayer->GiveNamedItem( playervariablestruct[i].szWeapon6, false, playervariablestruct[i].m_bIsADonator );

				if ( !strcmp( playervariablestruct[i].szWeapon6, "weapon_syringe" ) )
				{
					MESSAGE_BEGIN( MSG_ALL, gmsgSyringe );
						WRITE_BYTE( ENTINDEX( pPlayer->edict() ) );
						WRITE_BYTE( 1 );
					MESSAGE_END();
				}

				for ( int p = 0; p < MAX_ITEM_TYPES; p++ )
				{
					if ( pPlayer->m_rgpPlayerItems[p] && !FClassnameIs( pPlayer->m_rgpPlayerItems[p]->pev, "weapon_action" ) )
					{
						CBasePlayerItem *pPlayerItem = pPlayer->m_rgpPlayerItems[p];

						while ( pPlayerItem )
						{
							if ( FClassnameIs( pPlayerItem->pev, playervariablestruct[i].szWeapon6 ) )
							{
								((CBasePlayerWeapon *)pPlayerItem)->m_iClip = playervariablestruct[i].iClip6;
								pPlayer->m_rgAmmo[((CBasePlayerWeapon *)pPlayerItem)->m_iPrimaryAmmoType] = playervariablestruct[i].iAmmo6;
							}

							pPlayerItem = pPlayerItem->m_pNext;
						}
					}
				}

				// CoF: only reached when the player had a sixth weapon (original bug)
				if ( playervariablestruct[i].szLastUsedWeapon && pPlayer->HasNamedPlayerItem( playervariablestruct[i].szLastUsedWeapon ) )
					pPlayer->SelectItem( playervariablestruct[i].szLastUsedWeapon );
			}

			if ( pPlayer->m_iQuickSlotSlot[1] )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgQSlotUpdate, NULL, pPlayer->pev );
					WRITE_BYTE( 1 );
					WRITE_BYTE( pPlayer->m_iQuickSlotSlot[1] );
				MESSAGE_END();
			}

			if ( pPlayer->m_iQuickSlotSlot[2] )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgQSlotUpdate, NULL, pPlayer->pev );
					WRITE_BYTE( 2 );
					WRITE_BYTE( pPlayer->m_iQuickSlotSlot[2] );
				MESSAGE_END();
			}

			if ( pPlayer->m_iQuickSlotSlot[3] )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgQSlotUpdate, NULL, pPlayer->pev );
					WRITE_BYTE( 3 );
					WRITE_BYTE( pPlayer->m_iQuickSlotSlot[3] );
				MESSAGE_END();
			}

			CoFR_CoopRestore(pPlayer);
			memset( &playervariablestruct[i], 0, sizeof( playervariables ) );
			return true;
		}
	}

	// a new player
	if ( !hasfoundme && CVAR_GET_FLOAT( "sv_coop_nosave" ) == 0 )
	{
		ApplyCFGSettingToPlayerLate( pPlayer );

		for ( i = 0; i < MAX_AMMO_SLOTS; i++ )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgAmmoX, NULL, pPlayer->pev );
				WRITE_BYTE( i );
				WRITE_SHORT( pPlayer->m_rgAmmo[i] );
			MESSAGE_END();
		}
	}

	return false;
}
