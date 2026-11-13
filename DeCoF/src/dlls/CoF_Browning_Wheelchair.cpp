/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "gamerules.h"
#include "animation.h"
#include "soundent.h"

extern int gmsgParticle;

// cof_importantclassdefs.h
class CWheelchair : public CBaseMonster
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	void EXPORT WheelchairUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void MoveForward( void );
	void MoveBack( void );
	void TurnLeft( void );
	void TurnRight( void );
	bool m_bDead;
	virtual void Killed( entvars_t *pevAttacker, int iGib );
	virtual int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	virtual int Classify( void );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	float wheelchairtime1;
	float wheelchairtime2;
	float wheelchairtime3;
	float wheelchairtime4;
	void EXPORT Embark( void );
	void ExitCheck( void );
	void EXPORT Disembark( void );
	CBaseEntity *pExitEntity;
	CBaseEntity *pExitTeller;
	void SetCollision( void );
	virtual void HandleAnimEvent( MonsterEvent_t *pEvent );
	void EXPORT WheelchairThink( void );
	void SetSequenceByName( char *szSequence );
	virtual int ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() | FCAP_IMPULSE_USE; }
	CBasePlayer *m_pPlayer;
};

// sequences of models/weapons/browning/v_wheelchair_browning.mdl
enum browning_e
{
	BROWNING_DRAW = 0,
	BROWNING_IDLE,
	BROWNING_SHOOT,
	BROWNING_HOLSTER,
};

LINK_ENTITY_TO_CLASS( weapon_browning_wheelchair, CBrowningWheelchair );


void CBrowningWheelchair::Spawn( )
{
	pev->classname = MAKE_STRING( "weapon_browning_wheelchair" );
	Precache( );

	if ( custom_w_model )
		SET_MODEL( ENT( pev ), STRING( custom_w_model ) );
	else
		SET_MODEL( ENT( pev ), "models/null.mdl" );

	m_iId = 20;

	m_iSprintIdleAnim1 = 0;
	m_iSprintIdleAnim2 = 0;

	if ( pev->frags > 0 )
	{
		m_iDefaultAmmo = pev->frags;
	}
	else
	{
		if ( pev->frags == -2 )
			m_iClip = -666;

		m_iDefaultAmmo = 13;
	}

	m_bCanDualWield = false;

	while ( !script )
		script = MAKE_STRING( "inventoryitems/weapons/weapon_browning_wheelchair.txt" );

	if ( custom_script )
		script = custom_script;

	pev->renderfx = 99;

	m_bCanSuicide = false;

	strwmodel = MAKE_STRING( "models/null.mdl" );

	FallInit();// get ready to fall down.
}

void CBrowningWheelchair::Precache( void )
{
	PRECACHE_MODEL( "models/weapons/browning/v_wheelchair_browning.mdl" );
	PRECACHE_MODEL( "models/null.mdl" );
	PRECACHE_MODEL( "models/null.mdl" );

	if ( custom_v_model )
		PRECACHE_MODEL( custom_v_model );

	if ( custom_w_model )
		PRECACHE_MODEL( custom_w_model );

	m_iShell = PRECACHE_MODEL( "models/shell.mdl" );// brass shell
	PRECACHE_SOUND( "weapons/item_get.wav" );

	PRECACHE_SOUND( "weapons/browning/browning_fire.wav" );
	PRECACHE_SOUND( "weapons/browning/browning_magin.wav" );
	PRECACHE_SOUND( "weapons/browning/browning_magout.wav" );
}

int CBrowningWheelchair::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "browningwheelchair";
	p->iMaxAmmo1 = 130;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 13;
	p->iSlot = 4;
	p->iPosition = 2;
	p->iFlags = 0;
	p->iId = m_iId = 20;
	p->iWeight = 5;

	return 1;
}

BOOL CBrowningWheelchair::Deploy( )
{
	m_iSprintState = 0;
	m_iSuicideState = 0;

	if ( m_iClip == -666 )
		m_iClip = 0;

	return DefaultDeploy( "models/weapons/browning/v_wheelchair_browning.mdl", "models/null.mdl", BROWNING_DRAW, "onehanded" );
}

void CBrowningWheelchair::Holster( )
{
	SendWeaponAnim( BROWNING_HOLSTER );
	m_pPlayer->m_flNextAttack = gpGlobals->time + 1;
}


void CBrowningWheelchair::SecondaryAttack( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time )
		return;
}

void CBrowningWheelchair::PrimaryAttack( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time )
		return;

	if ( !m_pPlayer->m_pWheelchair )
	{
		if ( !( m_pPlayer->m_afButtonPressed & IN_ATTACK ) )
			return;
	}

	if ( m_bWaitingToRedeploy )
		return;

	// don't fire underwater
	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		if ( m_fEmptyClickTime < gpGlobals->time )
		{
			PlayEmptySound( );
			m_fEmptyClickTime = gpGlobals->time + 0.25;
		}
		return;
	}

	if ( m_iClip <= 0 )
	{
		if ( !m_fFireOnEmpty )
			Reload( );
		else
		{
			if ( m_fEmptyClickTime < gpGlobals->time )
			{
				PlayEmptySound( );
				m_fEmptyClickTime = gpGlobals->time + 0.25;
			}
		}

		m_flNextPrimaryAttack = gpGlobals->time + 0.1;
		m_flNextSecondaryAttack = gpGlobals->time + 1;
		return;
	}

	Vector vecShellVelocity = gpGlobals->v_right * 220 + gpGlobals->v_up * 200 + gpGlobals->v_forward * 40;
	Vector vecShellOrigin = pev->origin + pev->view_ofs + gpGlobals->v_forward * 80 + gpGlobals->v_right * 20;
	EjectBrass( vecShellOrigin, vecShellVelocity, pev->angles.y, m_iShell, TE_BOUNCE_SHELL );

	MESSAGE_BEGIN( MSG_ONE, gmsgParticle, NULL, m_pPlayer->pev );
		WRITE_SHORT( entindex() );
		WRITE_STRING( "aurora/gunsmoke.aur" );
		WRITE_BYTE( 1 );
		WRITE_STRING( "models/weapons/browning/v_browning_wheelchair.mdl" );
		WRITE_SHORT( 0 );
	MESSAGE_END();

	if ( !m_pPlayer->m_bInfiniteAmmo )
		m_iClip--;

	m_pPlayer->ViewPunch( RANDOM_LONG( -2, 2 ), 1, 1 );
	UTIL_ScreenShake( m_pPlayer->pev->origin, 5.0, 0.25, 0.15, 2.0 );

	m_Accuracy += 0.75;

	if ( m_Accuracy < 1 )
		m_AccuracyVector = Vector( 0, 0, 0 );
	else if ( m_Accuracy >= 1 && m_Accuracy < 2 )
		m_AccuracyVector = VECTOR_CONE_1DEGREES;
	else if ( m_Accuracy >= 2 && m_Accuracy < 3 )
		m_AccuracyVector = VECTOR_CONE_3DEGREES;
	else if ( m_Accuracy >= 3 && m_Accuracy < 4 )
		m_AccuracyVector = VECTOR_CONE_6DEGREES;
	else if ( m_Accuracy >= 4 )
		m_AccuracyVector = VECTOR_CONE_10DEGREES;
	else
		m_AccuracyVector = Vector( 0, 0, 0 );

	SendWeaponAnim( BROWNING_SHOOT );

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	Vector vecDir;

	if ( m_pPlayer->m_pWheelchair )
		vecDir = m_pPlayer->FireBulletsPlayer( 1, m_vAimingVector, m_pPlayer->pev->v_angle, VECTOR_CONE_4DEGREES + m_AccuracyVector, 8192, BULLET_PLAYER_357, 2, 10, m_pPlayer->pev, m_pPlayer->random_seed );
	else
		vecDir = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecAiming, m_AccuracyVector, 8192, BULLET_PLAYER_357, 2, 10, m_pPlayer->pev, m_pPlayer->random_seed );

	m_flTimeWeaponIdle = gpGlobals->time + 1;

	m_pPlayer->ViewKnock( 3.0 );

	PlayWeaponSound_guess( "weapons_new/browning/browning_fire.wav", 0, true, m_pPlayer->EarPosition(), m_pPlayer, true );

	m_pPlayer->m_iExtraSoundTypes = bits_SOUND_COMBAT;
	m_pPlayer->m_flStopExtraSoundTime = gpGlobals->time + 0.2;
	m_flNextPrimaryAttack = gpGlobals->time + 0.25;
	m_flNextSecondaryAttack = gpGlobals->time + 1;

	if ( m_iClip <= 0 )
		m_flNextPrimaryAttack = gpGlobals->time + 1;
}

void CBrowningWheelchair::Reload( void )
{
	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		return;

	if ( m_flNextPrimaryAttack > gpGlobals->time )
		return;

	if ( m_iSprintState == 1 || m_iSprintState > 2 )
		return;

	if ( m_iSuicideState )
		return;

	int j = min( 13 - m_iClip, m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] );

	if ( j == 0 )
		return;

	m_fReloadSpeed = 0;

	m_iSprintState = 2;

	if ( m_pPlayer && m_pPlayer->m_pWheelchair )
		GetClassPtr( (CWheelchair *)m_pPlayer->m_pWheelchair->pev )->SetSequenceByName( "reload" );

	m_iClip = 0;
	DefaultReload( 13, BROWNING_HOLSTER, 3, 0, false );
	m_bWaitingToRedeploy = true;

	EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/browning/browning_magout.wav", 1, 0.8, 0, 100 );
}

void CBrowningWheelchair::WeaponIdle( void )
{
	m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	if ( m_flTimeWeaponIdle > gpGlobals->time )
		return;

	if ( m_bWaitingToRedeploy )
		return;

	if ( m_iSprintState || m_iSuicideState )
		return;

	SendWeaponAnim( BROWNING_IDLE );
	m_flTimeWeaponIdle = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
}

void CBrowningWheelchair::TertiaryAttack( void )
{
}

void CBrowningWheelchair::ItemPostFrame( void )
{
	if ( m_bWaitingToRedeploy && !m_fInReload )
	{
		SendWeaponAnim( BROWNING_DRAW );
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1;
		m_bWaitingToRedeploy = false;
		EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/browning/browning_magin.wav", 1, 0.8, 0, 100 );
	}

	if ( m_fNextAccuracyTime < gpGlobals->time )
	{
		if ( m_Accuracy > 0 )
			m_Accuracy -= 0.25;

		if ( m_Accuracy < 0 )
			m_Accuracy = 0;

		m_fNextAccuracyTime = gpGlobals->time + 0.1;
	}

	CBasePlayerWeapon::ItemPostFrame();
}
