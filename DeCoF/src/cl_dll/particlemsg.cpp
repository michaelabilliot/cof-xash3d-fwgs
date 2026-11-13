//
// particlemsg.cpp
//
// implementation of CHudParticle class
//
#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "particlemgr.h"
#include "particlesys.h"

#include <string.h>
#include <stdio.h>

//extern ParticleSystemManager* g_pParticleSystems; // buz
extern cvar_t *cl_cheapweaponsmoke;

DECLARE_MESSAGE(m_Particle, Particle)
DECLARE_MESSAGE(m_Particle, DelParticle) // buz
DECLARE_MESSAGE(m_Particle, KillWpnPart)
DECLARE_MESSAGE(m_Particle, Ignite)

int CHudParticle::Init(void)
{
//	ConsolePrint("Hooking Particle message\n"); // 30/08/02 November235: Just a debug
	HOOK_MESSAGE(Particle);
	gEngfuncs.pfnHookUserMsg("DelPart", __MsgFunc_DelParticle); // buz
	HOOK_MESSAGE(KillWpnPart);
	HOOK_MESSAGE(Ignite);
	gHUD.AddHudElem(this);

/*	if (g_pParticleSystems)
	{
		delete g_pParticleSystems;
		g_pParticleSystems = NULL;
	}

	g_pParticleSystems = new ParticleSystemManager();*/ // buz

	return 1;
};

int CHudParticle::VidInit(void)
{
//	g_pParticleSystems->ClearSystems();
	g_pParticleSystems.ClearSystems(); // buz
	return 1;
};

int CHudParticle:: MsgFunc_Particle(const char *pszName,  int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
//	int type = READ_BYTE();
	int entindex = READ_SHORT();
//	float x = READ_COORD();
//	float y = READ_COORD();
//	float z = READ_COORD();
//	float ax = 0;//READ_COORD();
//	float ay = 0;//READ_COORD();
//	float az = 0;//READ_COORD();
	char sz[128];
	strncpy( sz, READ_STRING(), sizeof( sz ) );
	int iViewModel = READ_BYTE();
	char szModel[128];
	strncpy( szModel, READ_STRING(), sizeof( szModel ) );
	int iAttachment = READ_SHORT();
//	gEngfuncs.Con_Printf("Message received\n");
	//char sz[255];

	if ( strstr( sz, "gunsmoke.aur" ) && cl_cheapweaponsmoke->value )
		return 1;

	if ( strstr( sz, "blood_shot.aur" ) && CVAR_GET_FLOAT( "cl_bloodeffects" ) == 0 )
		return 1;

	if ( iViewModel )
		entindex = gEngfuncs.GetViewModel()->index;

	ParticleSystem *pSystem = new ParticleSystem(entindex, sz, iViewModel, iAttachment, szModel);//"aurora/smoke.aur");

//	g_pParticleSystems->AddSystem(pSystem);
	g_pParticleSystems.AddSystem(pSystem); // buz

	return 1;
}

// buz
int CHudParticle:: MsgFunc_DelParticle(const char *pszName,  int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	int entindex = READ_SHORT();
	g_pParticleSystems.DeleteSystemWithEntity( entindex );
	return 1;
}

int CHudParticle::MsgFunc_KillWpnPart(const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	bool bNow = READ_BYTE() != 0;
	g_pParticleSystems.DeleteViewModelSystems_guess( bNow );
	return 1;
}

int CHudParticle::MsgFunc_Ignite(const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	return 1;
}

int CHudParticle::Draw(float flTime)
{
	return 1;
}
