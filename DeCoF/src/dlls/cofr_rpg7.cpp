// Source port of the legacy cofr13 RPG-7; behavior recovered from its matching PE.
// Uses DeCoF entity/weapon APIs, with no proxy vtables or binary offsets.
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "player.h"
#include "gamerules.h"
#include "soundent.h"
#include <algorithm>
#include <cmath>
#undef min
#undef max
#include "cofr_rpg7_policy.hpp"
static int explosionSprite,smokeSprite,gunSmokeSprite;
static void softSmoke(const Vector& at,int scale,int brightness){MESSAGE_BEGIN(MSG_PVS,SVC_TEMPENTITY,at);WRITE_BYTE(TE_SPRITE);WRITE_COORD(at.x);WRITE_COORD(at.y);WRITE_COORD(at.z);WRITE_SHORT(gunSmokeSprite);WRITE_BYTE(scale);WRITE_BYTE(brightness);MESSAGE_END();}
static void light(const Vector& at,int radius,int life){MESSAGE_BEGIN(MSG_PVS,SVC_TEMPENTITY,at);WRITE_BYTE(TE_DLIGHT);WRITE_COORD(at.x);WRITE_COORD(at.y);WRITE_COORD(at.z);WRITE_BYTE(radius);WRITE_BYTE(255);WRITE_BYTE(180);WRITE_BYTE(90);WRITE_BYTE(life);WRITE_BYTE(40);MESSAGE_END();}
static const int RPG7_ID=31;
static char RocketAmmo[]="cof8_rpg7_rocket";
class CCoFRRocket:public CBaseEntity {
public:
 void Spawn();void EXPORT Fly();void EXPORT Hit(CBaseEntity* other);void Explode();
 int ObjectCaps(){return CBaseEntity::ObjectCaps()&~FCAP_ACROSS_TRANSITION;}
};
LINK_ENTITY_TO_CLASS(cof8_rpg7_rocket,CCoFRRocket);
void CCoFRRocket::Spawn(){
 pev->movetype=MOVETYPE_TOSS;pev->gravity=.08f;pev->solid=SOLID_BBOX;pev->dmg=250;pev->dmgtime=gpGlobals->time+8;
 SET_MODEL(edict(),"models/weapons/rpg7/rocket.mdl");UTIL_SetSize(pev,Vector(-1,-1,-1),Vector(1,1,1));
 SetThink(&CCoFRRocket::Fly);SetTouch(&CCoFRRocket::Hit);pev->nextthink=gpGlobals->time+.05f;
}
void CCoFRRocket::Fly(){if(gpGlobals->time>=pev->dmgtime){Explode();return;}
 float speed=pev->velocity.Length();if(std::isfinite(speed)&&speed>1){pev->angles=UTIL_VecToAngles(pev->velocity);pev->angles.x=std::remainder(pev->angles.x,360.f);pev->angles.z=0;pev->avelocity=g_vecZero;
 for(int puff=0;puff<3;++puff)softSmoke(pev->origin-pev->velocity*(26.5f/speed+.05f*puff/3.f),3,60);}
 pev->nextthink=gpGlobals->time+.05f;}
void CCoFRRocket::Hit(CBaseEntity* other){if(other&&other->edict()==pev->owner)return;Explode();}
void CCoFRRocket::Explode(){
 if(pev->solid==SOLID_NOT)return;pev->solid=SOLID_NOT;pev->movetype=MOVETYPE_NONE;
 EMIT_SOUND(edict(),CHAN_STATIC,"weapons/rpg7/explosion.wav",1,.8f);
 MESSAGE_BEGIN(MSG_PVS,SVC_TEMPENTITY,pev->origin);WRITE_BYTE(TE_EXPLOSION);WRITE_COORD(pev->origin.x);WRITE_COORD(pev->origin.y);WRITE_COORD(pev->origin.z);WRITE_SHORT(explosionSprite);WRITE_BYTE(22);WRITE_BYTE(15);WRITE_BYTE(TE_EXPLFLAG_NOSOUND);MESSAGE_END();
 light(pev->origin,40,4);
 float speed=pev->velocity.Length();Vector center=pev->origin-pev->velocity*(speed>1?6.f/speed:0);
 const Vector offsets[]={Vector(0,0,0),Vector(-8,6,5),Vector(8,-6,9)};
 for(int i=0;i<3;++i){Vector at=center+offsets[i];MESSAGE_BEGIN(MSG_PVS,SVC_TEMPENTITY,center);WRITE_BYTE(TE_SMOKE);WRITE_COORD(at.x);WRITE_COORD(at.y);WRITE_COORD(at.z);WRITE_SHORT(smokeSprite);WRITE_BYTE(i?32:45);WRITE_BYTE(8);MESSAGE_END();}
 entvars_t* attacker=pev->owner?VARS(pev->owner):pev;
 ::RadiusDamage(pev->origin,pev,attacker,250,300,CLASS_NONE,DMG_BLAST);
 UTIL_ScreenShake(pev->origin,.8f,8,.35f,600);UTIL_Remove(this);
}
class CCoFRRpg7:public CBasePlayerWeapon {
public:
 void Spawn();void Precache();int GetItemInfo(ItemInfo* info);int iItemSlot(){return 5;}
 BOOL CanDeploy(){return TRUE;}BOOL IsUseable(){return TRUE;}
 int ExtractAmmo(CBasePlayerWeapon*){return 0;}int ExtractClipAmmo(CBasePlayerWeapon*){return 0;}
 int AddDuplicate(CBasePlayerItem*,CBasePlayer*){return 0;}
 int Save(CSave& save);int Restore(CRestore& restore);static TYPEDESCRIPTION m_SaveData[];
 BOOL Deploy();void Holster();void PrimaryAttack();void Reload();void WeaponIdle();void ItemPostFrame();
};
// Preserve the legacy serialized field name; accept the early source-port alias too.
TYPEDESCRIPTION CCoFRRpg7::m_SaveData[]={{FIELD_INTEGER,"cof8_rpg7_reload",offsetof(CCoFRRpg7,m_fInReload),1,0},DEFINE_FIELD(CCoFRRpg7,m_fInReload,FIELD_INTEGER)};
int CCoFRRpg7::Save(CSave& save){return CBasePlayerWeapon::Save(save)&&save.WriteFields(STRING(pev->classname),"Cof8Rpg7",this,m_SaveData,1);}
int CCoFRRpg7::Restore(CRestore& restore){if(!CBasePlayerWeapon::Restore(restore)||!restore.ReadFields("Cof8Rpg7",this,m_SaveData,ARRAYSIZE(m_SaveData)))return 0;m_bCanDualWield=false;return m_iClip>=0&&m_iClip<=1&&m_fInReload>=0&&m_fInReload<=1;}
LINK_ENTITY_TO_CLASS(weapon_rpg7,CCoFRRpg7);
void CCoFRRpg7::Precache(){
 for(const char* path:{"models/weapons/rpg7/v_rpg7.mdl","models/weapons/rpg7/w_rpg7.mdl","models/weapons/rpg7/p_rpg7.mdl","models/weapons/rpg7/rocket.mdl"})PRECACHE_MODEL(const_cast<char*>(path));
 explosionSprite=PRECACHE_MODEL("sprites/fexplo.spr");smokeSprite=PRECACHE_MODEL("sprites/steam1.spr");gunSmokeSprite=PRECACHE_MODEL("sprites/gunsmoke.spr");
 for(const char* path:{"weapons/rpg7/fire.wav","weapons/rpg7/reload.wav","weapons/rpg7/explosion.wav"})PRECACHE_SOUND(const_cast<char*>(path));
}
void CCoFRRpg7::Spawn(){
 Precache();pev->classname=MAKE_STRING("weapon_rpg7");m_iId=RPG7_ID;m_iClip=0;m_iDefaultAmmo=0;m_fInReload=0;m_bCanDualWield=false;
 script=MAKE_STRING("inventoryitems/weapons/weapon_rpg7.txt");strwmodel=MAKE_STRING("models/weapons/rpg7/w_rpg7.mdl");
 SET_MODEL(edict(),STRING(strwmodel));pev->renderfx=99;FallInit();
}
int CCoFRRpg7::GetItemInfo(ItemInfo* i){
 i->pszName="weapon_rpg7";i->pszAmmo1=RocketAmmo;i->iMaxAmmo1=4;i->pszAmmo2=NULL;i->iMaxAmmo2=-1;
 i->iMaxClip=1;i->iSlot=5;i->iPosition=1;i->iId=RPG7_ID;i->iFlags=0;i->iWeight=25;return 1;
}
BOOL CCoFRRpg7::Deploy(){
 Precache();m_fInReload=0;pev->body=m_iClip==1?1:0;
 BOOL result=DefaultDeploy("models/weapons/rpg7/v_rpg7.mdl","models/weapons/rpg7/p_rpg7.mdl",1,"shotgun",0,0);
 if(result)m_flNextPrimaryAttack=m_flTimeWeaponIdle=gpGlobals->time+50.f/30.f;
 return result;
}
void CCoFRRpg7::Holster(){m_fInReload=0;pev->body=m_iClip?1:0;SendWeaponAnim(4,0,0);m_pPlayer->m_flNextAttack=gpGlobals->time+.5f;}
void CCoFRRpg7::PrimaryAttack(){
 if(!m_pPlayer||m_fInReload||m_flNextPrimaryAttack>gpGlobals->time)return;
 if(m_iPrimaryAmmoType<=0||!cof::rpg::validAmmo(m_iClip,m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]))return;
 if(!m_iClip){PlayEmptySound();m_flNextPrimaryAttack=gpGlobals->time+.3f;return;}
 UTIL_MakeVectors(m_pPlayer->pev->v_angle);
 Vector eye=m_pPlayer->GetGunPosition();Vector muzzle=eye+gpGlobals->v_forward*67.82188f+gpGlobals->v_right*12-gpGlobals->v_up*8;
 TraceResult tr;UTIL_TraceLine(eye,muzzle,dont_ignore_monsters,m_pPlayer->edict(),&tr);
 auto rocket=static_cast<CCoFRRocket*>(CBaseEntity::Create("cof8_rpg7_rocket",tr.vecEndPos,m_pPlayer->pev->v_angle,m_pPlayer->edict()));if(!rocket)return;
 rocket->pev->velocity=gpGlobals->v_forward*1100;cof::rpg::consume(m_iClip,m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]);pev->body=0;SendWeaponAnim(2,0,0);
 light(muzzle,16,1);softSmoke(tr.vecEndPos,2,100);if(tr.flFraction==1&&!tr.fStartSolid)softSmoke(tr.vecEndPos+gpGlobals->v_forward*4+gpGlobals->v_up*2,2,60);
 EMIT_SOUND(m_pPlayer->edict(),CHAN_WEAPON,"weapons/rpg7/fire.wav",1,ATTN_NORM);
 m_pPlayer->pev->punchangle.x=std::max(-12.f,m_pPlayer->pev->punchangle.x-6);
 m_pPlayer->pev->punchangle.z=std::min(1.5f,m_pPlayer->pev->punchangle.z+.65f);
 m_flNextPrimaryAttack=m_flTimeWeaponIdle=gpGlobals->time+1.3333334f;
 if(tr.flFraction<1||tr.fStartSolid)rocket->Explode();
}
void CCoFRRpg7::Reload(){
 if(!m_pPlayer||m_iClip||m_fInReload||m_flNextPrimaryAttack>gpGlobals->time||m_iPrimaryAmmoType<0||m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]<=0)return;
 m_fInReload=TRUE;pev->body=1;SendWeaponAnim(3,0,0);EMIT_SOUND(m_pPlayer->edict(),CHAN_WEAPON,"weapons/rpg7/reload.wav",1,ATTN_NORM);
 m_flTimeWeaponIdle=gpGlobals->time+2.2f;m_flNextPrimaryAttack=gpGlobals->time+3;
}
void CCoFRRpg7::WeaponIdle(){if(m_flTimeWeaponIdle<=gpGlobals->time){if(m_pPlayer->pev->weaponanim!=0||pev->body!=(m_iClip?1:0)){pev->body=m_iClip?1:0;SendWeaponAnim(0,0,0);}m_flTimeWeaponIdle=gpGlobals->time+3;}}
void CCoFRRpg7::ItemPostFrame(){
 if(!m_pPlayer||!m_pPlayer->IsAlive()||m_pPlayer->pev->iuser1||m_pPlayer->pev->flags&(FL_FROZEN|FL_ONTRAIN)||m_pPlayer->m_bInPhoneCall)return;
 if(m_iPrimaryAmmoType<=0)m_iPrimaryAmmoType=m_pPlayer->GetAmmoIndex(RocketAmmo);
 if(m_iPrimaryAmmoType<=0)return;int& reserve=m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType];reserve=std::max(0,std::min(reserve,4-m_iClip));
 if(m_fInReload&&m_flTimeWeaponIdle<=gpGlobals->time){cof::rpg::transfer(m_iClip,reserve);m_fInReload=0;}
 if(m_pPlayer->pev->flags&(FL_FROZEN|FL_ONTRAIN)||m_pPlayer->m_bInPhoneCall||m_pPlayer->IsOnLadder())return;
 if(m_pPlayer->pev->button&IN_RELOAD)Reload();else if(m_pPlayer->pev->button&IN_ATTACK)PrimaryAttack();else WeaponIdle();
}
class CCoFRRpgAmmo:public CBasePlayerAmmo {
 void Spawn(){Precache();SET_MODEL(edict(),"models/weapons/rpg7/rocket.mdl");CBasePlayerAmmo::Spawn();}
 void Precache(){PRECACHE_MODEL("models/weapons/rpg7/rocket.mdl");PRECACHE_SOUND("items/9mmclip1.wav");}
 BOOL AddAmmo(CBaseEntity* other){auto p=static_cast<CBasePlayer*>(other);int cap=4;auto w=p->GetPlayerWeapon(RPG7_ID);if(w)cap-=w->m_iClip;if(p->GiveAmmo(1,RocketAmmo,cap,false)<0)return FALSE;EMIT_SOUND(edict(),CHAN_ITEM,"items/9mmclip1.wav",1,ATTN_NORM);return TRUE;}
};
LINK_ENTITY_TO_CLASS(ammo_rpg7,CCoFRRpgAmmo);

bool CoFR_RpgPickupAllowed(CBasePlayer* player,int clip){if(!player)return false;int index=player->GetAmmoIndex(RocketAmmo);return index>0&&cof::rpg::validAmmo(clip,player->m_rgAmmo[index]);}
