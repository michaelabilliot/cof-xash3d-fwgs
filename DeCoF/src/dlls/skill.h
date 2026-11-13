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
//=========================================================
// skill.h - skill level concerns
//=========================================================

struct skilldata_t
{

	int iSkillLevel; // game skill level
	float syringeHeal;
	float flareBurnTime;
	float chainsawbossHealth;
	float spitterHealth;
	float aomPills;
	float DmgAxe;
	float twitcherHealth;
	float twitcherDmgOneSlash;
	float twitcherDmgBothSlash;
	float twitcher2Health;
	float twitcher2DmgOneSlash;
	float twitcher2DmgBothSlash;
	float twitcher3Health;
	float twitcher3DmgOneSlash;
	float twitcher3DmgBothSlash;
	float twitcher4Health;
	float twitcher4DmgOneSlash;
	float twitcher4DmgBothSlash;
	float sewmoHealth;
	float sewmoDmgOneSlash;
	float sewmoDmgBothSlash;
	float slower3Health;
	float slower3DmgOneSlash;
	float slower3DmgBothSlash;
	float sewerbossHealth;
	float sewerbossMelee;
	float roofbossHealth;
	float roofbossHeadHit;
	float roofbossHeadHealth;
	float roofbossAttackTime;
	float suiciderHealth;
	float suiciderBullet;
	float suiciderSuicide;

	float zombieHealth;
	float zombieDmgOneSlash;
	float zombieDmgBothSlash;
	float tallerHealth;
	float tallerDmgPunch;
	float flygareAcid;
	float flygareHealth;
	float flygareMelee;
	float doctorHealth;
	float doctorP345;
	float doctorRevolver;
	float rcrazyHealth;
	float rcrazyDmgOneSlash;
	float rcrazyDmgBothSlash;
	float facelessHealth;
	float facelessDmgOneSlash;
	float facelessDmgBothSlash;
	float childHealth;
	float childDmgOneSlash;
	float childDmgBothSlash;
	float sawrunnerHealth;
	float sawrunnerDmgOneSlash;
	float sawrunnerDmgBothSlash;
	float fasterHealth;
	float fasterDmgOneSlash;
	float fasterDmgBothSlash;
	float krypandeHealth;
	float krypandeDmgOneSlash;
	float krypandeDmgBothSlash;
	float babyHealth;
	float babyDmg;
	float strangerHealth;
	float strangerDmg;
	float DmgWeaponMelee;
	float DmgNightstick;
	float DmgSwitchblade;
	float DmgSwitchblade2;
	float DmgPunch;
	float DmgBranch;
	float DmgSledgehammer;
	float DmgGlock;
	float DmgM16;
	float DmgVP70;
	float DmgShotgun;
	float DmgP345;
	float DmgRifle;
	float DmgRevolver;
	float DmgG43_guess;	// CoF: RefreshSkillData stores "sk_cof_g43" here (+0x144) in 1.55 and 1.6a; the 1.55 PDB type lacks it

// health/suit charge
	float suitchargerCapacity;
	float batteryCapacity;
	float healthchargerCapacity;
	float healthkitCapacity;
	float flashlightCharge;

// monster damage adj
	float monHead;
	float monChest;
	float monStomach;
	float monLeg;
	float monArm;

// player damage adj
	float plrHead;
	float plrChest;
	float plrStomach;
	float plrLeg;
	float plrArm;
	float plrDmgBuckshot; // used in paranoia

// buz: maxspeeds
	float plrPrimaryMaxSpeed;	// in primary weapon mode
	float plrSecondaryMaxSpeed;	// in secondary weapon mode
	float plrSprintMaxSpeed;
	float plrStairMaxSpeed;
};

extern	DLL_GLOBAL	skilldata_t	gSkillData;
float GetSkillCvar( char *pName );

extern DLL_GLOBAL int		g_iSkillLevel;

#define SKILL_EASY		1
#define SKILL_MEDIUM	2
#define SKILL_HARD		3
