// Source-level restoration of custom-inventory's server-authoritative protocol.
// No retail addresses, injected hooks or DLL forwarding are used.
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <set>
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"
#include "cofr_features.h"
#include "cofr_creative.hpp"
#include "cofr_creative_entities.hpp"
#include "cofr_ammo_policy.hpp"
#include "cofr_catalogue_paths.hpp"
#undef min
#undef max
#include "cofr_rpg7_policy.hpp"

extern int gSpecialItemGive;
extern bool CoFR_SkipIntro(CBasePlayer*);
#undef SET_VIEW
extern void SET_VIEW(edict_t*,edict_t*);
extern bool m_gbIsInCutscene;
namespace {
struct Carry {std::string script[9],model[9],name[9];std::map<std::string,int> clips,ammo;};
std::map<std::string,Carry> cooperativeCarry;
constexpr int Slots=8;
struct Session { bool sent=false,enabled=false; size_t next=0; float last=-100.f; int maxHealth=-1,notarget=-1,health=-1; EHANDLE pendingSyringe{}; float useUntil=0.f; };
std::unordered_map<CBasePlayer*,Session> sessions;
std::vector<cof::CreativeItem> catalogue;
int message=0,grantDepth=0,spawnCount=0;
EHANDLE fixtureEnemy;
std::set<std::string> precachedEntities;
bool allowed() { return gpGlobals && cof::creativeEnabled(CVAR_GET_FLOAT("sv_cheats"),gpGlobals->maxClients,gpGlobals->deathmatch,gpGlobals->coop); }
bool normal(CBasePlayer* p) {
 return p&&p->IsAlive()&&!p->pev->deadflag&&!p->pev->iuser1&&!(p->pev->flags&FL_ONTRAIN)&&
  !p->IsOnLadder()&&!p->m_bInPhoneCall&&!p->m_iUsingComputerOrPhone;
}
bool slot(int n) {return n>=1&&n<=Slots;}
void result(CBasePlayer* p,bool okay,const std::string& text) {
 if(!p)return;
 const std::string bounded=text.substr(0,150);
 if(message){MESSAGE_BEGIN(MSG_ONE,message,NULL,p->pev);WRITE_BYTE(2);WRITE_BYTE(okay?1:0);WRITE_STRING(bounded.c_str());MESSAGE_END();}
 ClientPrint(p->pev,HUD_PRINTCONSOLE,(bounded+"\n").c_str());
}
std::string load(const char* path) {
 int size=0;byte* bytes=LOAD_FILE_FOR_ME(const_cast<char*>(path),&size);std::string data;
 if(bytes){if(size>0&&size<=1024*1024)data.assign(reinterpret_cast<char*>(bytes),size);FREE_FILE(bytes);}return data;
}
bool exists(const char* path){int size=0;byte* b=LOAD_FILE_FOR_ME(const_cast<char*>(path),&size);bool yes=b&&size>0;if(b)FREE_FILE(b);return yes;}
void loadCatalogue() {
 if(!catalogue.empty())return;
 catalogue=cof::creativeCatalogue(CataloguePaths,sizeof CataloguePaths/sizeof *CataloguePaths,load,
  [](const char* name){return g_engfuncs.pfnFunctionFromName(name)!=0;},exists);
 for (const auto& entry : cof::CreativeEntities) {
  if (!precachedEntities.count(entry.key) || catalogue.size() >= cof::CreativeLimit) continue;
  catalogue.push_back({entry.key, entry.classname, entry.name, {}, entry.model, cof::CreativeGroup::Entity});
 }
 ALERT(at_console,"COFR catalogue: %u carried definitions and entities\n",unsigned(catalogue.size()));
}
void sync(CBasePlayer* p,bool force) {
 if(!message)message=REG_USER_MSG(cof::CreativeMessage,-1);
 const bool enabled=allowed();if(enabled)loadCatalogue();auto& s=sessions[p];
 if(force||!s.sent||s.enabled!=enabled){s.sent=true;s.enabled=enabled;s.next=0;
  MESSAGE_BEGIN(MSG_ONE,message,NULL,p->pev);WRITE_BYTE(0);WRITE_BYTE(enabled);WRITE_SHORT(enabled?int(catalogue.size()):0);MESSAGE_END();}
 if(!enabled)return;
 int target=(p->pev->flags&FL_NOTARGET)?1:0;if(force||s.notarget!=target){s.notarget=target;MESSAGE_BEGIN(MSG_ONE,message,NULL,p->pev);WRITE_BYTE(3);WRITE_BYTE(target);MESSAGE_END();}
 for(int batch=0;batch<4&&s.next<catalogue.size();++batch,++s.next){const auto& item=catalogue[s.next];
  MESSAGE_BEGIN(MSG_ONE,message,NULL,p->pev);WRITE_BYTE(1);WRITE_SHORT(int(s.next)+1);WRITE_BYTE(int(item.group));WRITE_STRING(item.script.c_str());MESSAGE_END();}
}
void slots(CBasePlayer* p){for(int i=1;i<=Slots;++i)p->InventoryMessage(i,p->cofrInventorySlot[i],0);}
struct Grant {int old;Grant():old(gSpecialItemGive){gSpecialItemGive=1;++grantDepth;}~Grant(){--grantDepth;gSpecialItemGive=old;}};
bool grant(CBasePlayer* p,const cof::CreativeItem& item,int destination) {
 if(!slot(destination))return false;
 if(p->cofrInventorySlot[destination])return false;
 int before[9];for(int i=0;i<9;++i)before[i]=p->cofrInventorySlot[i];
 if(item.group==cof::CreativeGroup::Weapon){
  for(int i=1;i<=Slots;++i)if(p->cofrInventorySlotClassname[i]&&item.classname==STRING(p->cofrInventorySlotClassname[i]))return false;
  Grant scope;p->GiveNamedItem(item.classname.c_str(),false,false);
 }else{
  if(!p->AddItemToInventory(ALLOC_STRING(item.script.c_str()),ALLOC_STRING(item.model.c_str()),ALLOC_STRING(item.classname.c_str())))return false;
 }
 int created=0;for(int i=1;i<=Slots;++i)if(!before[i]&&p->cofrInventorySlot[i]){created=i;break;}
 if(!created)return false;
 // AddItemToInventory's native notification precedes model/class assignment for quest items.
 if(item.group==cof::CreativeGroup::Item){p->cofrInventorySlotModels[created]=ALLOC_STRING(item.model.c_str());p->cofrInventorySlotClassname[created]=ALLOC_STRING(item.classname.c_str());}
 if(created!=destination){std::swap(p->cofrInventorySlot[created],p->cofrInventorySlot[destination]);std::swap(p->cofrInventorySlotModels[created],p->cofrInventorySlotModels[destination]);std::swap(p->cofrInventorySlotClassname[created],p->cofrInventorySlotClassname[destination]);}
 if(item.classname=="weapon_rpg7"){auto w=p->GetPlayerWeapon(31);if(w){int index=p->GetAmmoIndex("cof8_rpg7_rocket");if(index>0){w->m_iPrimaryAmmoType=index;w->m_iClip=1;w->pev->body=1;p->GiveAmmo(3,const_cast<char*>("cof8_rpg7_rocket"),3,false);}}}
 p->cofrClientinventorySlot[created]=p->cofrClientinventorySlot[destination]=-1;slots(p);return true;
}
CBasePlayerWeapon* ownedWeapon(CBasePlayer* player, const std::string& classname)
{
 for (auto head : player->m_rgpPlayerItems)
  for (auto item = head; item; item = item->m_pNext)
   if (classname == STRING(item->pev->classname))
    return static_cast<CBasePlayerWeapon*>(item->GetWeaponPtr());
 return nullptr;
}

void creativeGrant(CBasePlayer* player, const cof::CreativeItem& item, int destination)
{
 const bool rocketPickup = item.classname == "ammo_rpg7";
 const std::string weaponClass = rocketPickup ? "weapon_rpg7" : item.classname;
 if (destination && player->cofrInventorySlot[destination] &&
     ((item.group != cof::CreativeGroup::Weapon && !rocketPickup) ||
      weaponClass != STRING(player->cofrInventorySlotClassname[destination]))) {
  result(player, false, "That slot is occupied by a different item.");
  return;
 }

 auto weapon = item.group == cof::CreativeGroup::Weapon || rocketPickup ? ownedWeapon(player, weaponClass) : nullptr;
 if (weapon || rocketPickup) {
  if (weapon && cof::hideWeaponAmmo(weapon->m_iId)) {
   result(player, false, "This weapon does not use ammunition.");
   return;
  }
  const char* ammo = rocketPickup ? cof::rpg::AmmoName : weapon->pszAmmo1();
  int limit = rocketPickup ? cof::rpg::TotalRounds : weapon->iMaxAmmo1();
  int amount = weapon ? std::max(1, weapon->iMaxClip()) : 1;
  if (ammo && !strcmp(ammo, cof::rpg::AmmoName)) {
   const int loaded = weapon ? weapon->m_iClip : 0;
   limit = cof::rpg::TotalRounds - loaded;
   amount = 1;
  } else if (ammo && !strcmp(ammo, "buckshot")) amount = 9;
  else if (ammo && !strcmp(ammo, "m16")) amount = 20;
  else if (ammo && !strcmp(ammo, "p345")) amount = 8;

  const int index = ammo ? player->GetAmmoIndex(ammo) : -1;
  if (index < 1 || index >= MAX_AMMO_SLOTS || limit <= 0) {
   result(player, false, "This weapon has no available reserve ammunition.");
   return;
  }
  const int before = player->m_rgAmmo[index];
  if (before >= limit) {
   result(player, false, "Reserve ammunition is full.");
   return;
  }
  player->GiveAmmo(amount, const_cast<char*>(ammo), limit, false);
  const int added = player->m_rgAmmo[index] - before;
  result(player, added > 0, added > 0 ? "Added " + std::to_string(added) + " " + ammo : "Ammunition grant rejected.");
  return;
 }

 if (!destination)
  for (int i = 1; i <= Slots; ++i)
   if (!player->cofrInventorySlot[i]) { destination = i; break; }
 if (!destination) {
  result(player, false, "Inventory is full.");
  return;
 }
 const bool added = grant(player, item, destination);
 result(player, added, added ? item.name + " added to slot " + std::to_string(destination) : "Native pickup restriction.");
}

bool canUseSyringe(CBasePlayer* player, CBasePlayerWeapon* weapon)
{
 return weapon && normal(player) && !(player->pev->flags & FL_FROZEN) &&
  !player->IsInCutscene() && player->pev->fuser3 != 10 && player->pev->waterlevel < 3 &&
  weapon->m_iPrimaryAmmoType > 0 && weapon->m_iPrimaryAmmoType < MAX_AMMO_SLOTS &&
  player->m_rgAmmo[weapon->m_iPrimaryAmmoType] > 0;
}

void activateSlot(CBasePlayer* player, int index)
{
 const char* classname = STRING(player->cofrInventorySlotClassname[index]);
 auto& session = sessions[player];
 session.pendingSyringe = nullptr;
 if (!strncmp(classname, "weapon_", 7)) {
  if (strcmp(classname, "weapon_syringe")) {
   player->InventoryEquip(index);
   return;
  }
  auto syringe = ownedWeapon(player, classname);
  if (!canUseSyringe(player, syringe)) return;
  if (player->m_pActiveItem == syringe && (syringe->m_flNextPrimaryAttack > gpGlobals->time ||
      player->m_flNextAttack > gpGlobals->time || syringe->m_iSprintState)) return;
  if (player->m_pActiveItem != syringe) player->InventoryEquip(index);
  else player->CloseInventory();
  if ((player->m_pActiveItem == syringe && !player->m_bWeaponWaitingToDeploy) ||
      (player->m_bWeaponWaitingToDeploy && player->m_pNextItem == syringe)) {
   session.pendingSyringe = syringe;
   session.useUntil = gpGlobals->time + 2.f;
  }
 } else player->InventoryUse(index);
}

void processSyringe(CBasePlayer* player, Session& session)
{
 auto weapon = static_cast<CBasePlayerWeapon*>(static_cast<CBaseEntity*>(session.pendingSyringe));
 if (!weapon) return;
 if (gpGlobals->time > session.useUntil || !canUseSyringe(player, weapon) ||
     (player->m_bWeaponWaitingToDeploy ? player->m_pNextItem != weapon : player->m_pActiveItem != weapon)) {
  session.pendingSyringe = nullptr;
  return;
 }
 if (player->m_bWeaponWaitingToDeploy || player->m_flNextAttack > gpGlobals->time ||
     weapon->m_flNextPrimaryAttack > gpGlobals->time || weapon->m_iSprintState) return;
 session.pendingSyringe = nullptr;
 weapon->PrimaryAttack();
}

void spawn(CBasePlayer* p,const cof::CreativeItem& item) {
 if(item.group!=cof::CreativeGroup::Entity||spawnCount>=128){result(p,false,"Entity limit or invalid catalogue entry.");return;}
 const auto* definition = cof::creativeEntity(item.script);
 if (!definition || !precachedEntities.count(definition->key)) return;
 UTIL_MakeVectors(p->pev->v_angle);
 const Vector start = p->pev->origin + p->pev->view_ofs;
 // Spitter wakes through its native 128-unit proximity check.
 const Vector target = start + gpGlobals->v_forward * (item.classname == "monster_spitter" ? 112.f : 128.f);
 TraceResult tr;UTIL_TraceHull(start,target,dont_ignore_monsters,human_hull,p->edict(),&tr);
 if(tr.fStartSolid||tr.fAllSolid||tr.flFraction<0.5f){result(p,false,"No clear spawn space in front of you.");return;}
 Vector position=tr.vecEndPos;
 UTIL_TraceLine(position+Vector(0,0,32),position-Vector(0,0,256),ignore_monsters,p->edict(),&tr);
 if(tr.fStartSolid||tr.flFraction==1){result(p,false,"No clear ground for this monster.");return;}
 position = tr.vecEndPos + Vector(0, 0, 2 + definition->floorClearance);
 Vector center=position+Vector(0,0,36);
 UTIL_TraceHull(center,center,dont_ignore_monsters,human_hull,p->edict(),&tr);
 if(tr.fStartSolid||tr.fAllSolid){result(p,false,"Spawn position is obstructed.");return;}
 Vector angles(0,p->pev->v_angle.y+180,0);
 edict_t* edict = CREATE_NAMED_ENTITY(ALLOC_STRING(definition->classname));
 if (FNullEnt(edict)) { result(p, false, "Entity allocation failed."); return; }
 edict->v.origin = position;
 edict->v.angles = angles;
 if (definition->modelOverride) edict->v.model = ALLOC_STRING(definition->model);
 if (DispatchSpawn(edict) < 0 || edict->free || !edict->v.modelindex || edict->v.health <= 0) {
  if (!edict->free) REMOVE_ENTITY(edict);
  result(p, false, "Native entity initialization rejected this spawn.");
  return;
 }
 auto entity = CBaseEntity::Instance(edict);
 if (!entity) { REMOVE_ENTITY(edict); return; }
 // Validate the initialized class hull. Centered hulls must start above the floor.
 position.z += std::max(0.f, -entity->pev->mins.z);
 UTIL_SetOrigin(entity, position);
 g_engfuncs.pfnTraceMonsterHull(edict, position, position, dont_ignore_monsters, edict, &tr);
 if(tr.fStartSolid||tr.fAllSolid){UTIL_Remove(entity);result(p,false,"This monster does not fit here. Choose open ground.");return;}
 ++spawnCount;entity->pev->targetname=ALLOC_STRING("cofr_creative_spawn");
 ALERT(at_console,"COFR spawned index=%d class=%s model=%s modelindex=%d health=%g effects=%d\n",ENTINDEX(entity->edict()),item.classname.c_str(),STRING(entity->pev->model),entity->pev->modelindex,entity->pev->health,entity->pev->effects);
 result(p,true,item.name+" spawned in front of you.");
}
}

bool CoFR_AuthorizedGrant(){return grantDepth>0&&allowed();}
bool CoFR_CarriedUnlock(CBasePlayer* p,int type){
 if(!p)return false;const char* name=type==64?"weapon_famas":type==524288?"weapon_axe":NULL;if(!name)return false;
 for(int i=1;i<=8;++i)if(p->cofrInventorySlotClassname[i]&&!strcmp(STRING(p->cofrInventorySlotClassname[i]),name))return true;
 auto transfer=cooperativeCarry.find(STRING(p->pev->netname));if(transfer!=cooperativeCarry.end())for(int i=1;i<=8;++i)if(transfer->second.script[i]==name)return true;
 return false;
}
void CoFR_ServerWorldPrecache(){
 sessions.clear();catalogue.clear();spawnCount=0;fixtureEnemy=NULL;message=REG_USER_MSG(cof::CreativeMessage,-1);
 precachedEntities.clear();
 std::set<std::string> factories;
 for (const auto& entry : cof::CreativeEntities) {
  if (!g_engfuncs.pfnFunctionFromName(entry.classname) || !exists(entry.model)) continue;
  if (factories.insert(entry.classname).second) UTIL_PrecacheOther(entry.classname);
  if (entry.modelOverride) PRECACHE_MODEL(const_cast<char*>(entry.model));
  precachedEntities.insert(entry.key);
 }
}
void CoFR_ServerFrame(CBasePlayer* p){if(!p)return;sync(p,false);
 int cap=int(p->pev->max_health);if(cap<=0)cap=100;if(cap>32767)cap=32767;
 int hp=std::max(0,std::min(32767,int(p->pev->health)));
 auto& s=sessions[p];processSyringe(p,s);if(s.maxHealth!=cap||s.health!=hp){s.maxHealth=cap;s.health=hp;static int msg=0;if(!msg)msg=REG_USER_MSG("Cof8MaxHP",4);MESSAGE_BEGIN(MSG_ONE,msg,NULL,p->pev);WRITE_SHORT(cap);WRITE_SHORT(hp);MESSAGE_END();}

}
bool CoFR_ServerCommand(CBasePlayer* p){
 const char* command=CMD_ARGV(0);if(strncmp(command,"cof8_",5))return false;
 if(!p)return true;
 if(!strcmp(command,"cof8_boat")){int mask=0;if(CMD_ARGC()==2&&cof::creativeNumber(CMD_ARGV(1),mask)&&mask>=0&&mask<=15)CoFR_BoatInput(p,unsigned(mask));return true;}
 if(!strcmp(command,"cof8_skipintro")){CoFR_SkipIntro(p);return true;}
 if(!strcmp(command,"cof8_quick")){
  int a=0,b=0;if(!normal(p)||CMD_ARGC()!=3||!cof::creativeNumber(CMD_ARGV(1),a)||!cof::creativeNumber(CMD_ARGV(2),b)||a<1||a>4||b<1||b>4||a==b)return true;
  int ids[2]={};int slots[2]={a,b};
  for(int n=0;n<2;++n)for(int id=1;id<32;++id){auto w=p->GetPlayerWeapon(id);if(w&&p->cofrInventorySlot[slots[n]]&&p->cofrInventorySlotClassname[slots[n]]&&!strcmp(STRING(w->pev->classname),STRING(p->cofrInventorySlotClassname[slots[n]])))ids[n]=id;}
  if(ids[0]&&ids[1]&&ids[0]!=31&&ids[1]!=31&&p->CanBeDualWielded(ids[0])&&p->CanBeDualWielded(ids[1])&&!(p->IsAFlashlight(ids[0])&&p->IsAFlashlight(ids[1])))p->DualWield(a,b);else p->InventoryEquip(b);return true;
 }
 if(!strcmp(command,"cof8_toggle")){
  int n=0;if(!normal(p)||CMD_ARGC()!=2||!cof::creativeNumber(CMD_ARGV(1),n)||!slot(n)||!p->cofrInventorySlot[n])return true;
  const char* name=p->cofrInventorySlotClassname[n]?STRING(p->cofrInventorySlotClassname[n]):"";auto active=p->m_pActiveItem;
  bool same=active&&!strcmp(STRING(active->pev->classname),name);
  if(active&&FClassnameIs(active->pev,"weapon_dualweapon"))for(int id:{p->m_iDualWeaponLeftID,p->m_iDualWeaponRightID}){auto w=p->GetPlayerWeapon(id);if(w&&!strcmp(STRING(w->pev->classname),name))same=true;}
  if(!same){p->InventoryEquip(n);return true;}
  if(p->pev->flags&FL_FROZEN||p->m_fPhoneCallTime>gpGlobals->time||p->m_flNextAttack>gpGlobals->time||!active->CanHolster())return true;
  p->ResetAutoaim();active->Holster();p->SetDualWieldWeapon(0);p->m_iDualWeaponLeftID=p->m_iDualWeaponRightID=0;
  p->m_pActiveItem=p->m_pNextItem=p->m_pLastItem=NULL;p->m_bWeaponWaitingToDeploy=false;p->pev->viewmodel=p->pev->weaponmodel=0;
  extern int gmsgCurWeapon;MESSAGE_BEGIN(MSG_ONE,gmsgCurWeapon,NULL,p->pev);WRITE_BYTE(0);WRITE_BYTE(0);WRITE_BYTE(0);MESSAGE_END();return true;
 }
 if(!strcmp(command,"cof8_cheats")){result(p,allowed(),allowed()?"Offline cheats enabled":"Offline cheats disabled");return true;}
 if(!strcmp(command,"cof8_verify")){
  if(!allowed()||!normal(p))return true;
  if(CMD_ARGC()==2){auto w=p->GetPlayerWeapon(31);const char* action=CMD_ARGV(1);
   if(!strcmp(action,"rpg")){
    loadCatalogue();for(const auto& item:catalogue)if(item.classname=="weapon_rpg7"){grant(p,item,8);p->InventoryEquip(8);break;}
   }
   else if(!strcmp(action,"fire")&&w){w->PrimaryAttack();ALERT(at_console,"COFR RPG verify fire time=%g next=%g clip=%d reserve=%d\n",gpGlobals->time,w->m_flNextPrimaryAttack,w->m_iClip,w->m_iPrimaryAmmoType>0?p->m_rgAmmo[w->m_iPrimaryAmmoType]:-1);}
   else if(!strcmp(action,"reload")&&w){w->Reload();ALERT(at_console,"COFR RPG verify reload pending=%d clip=%d\n",w->m_fInReload,w->m_iClip);}
   else if(!strcmp(action,"control")){
    if(p->m_hCurrentCamera)p->m_hCurrentCamera->Use(p,p,USE_OFF,0);
    p->m_hCurrentCamera=NULL;p->m_pAttachedCamera=NULL;m_gbIsInCutscene=false;p->m_iHideHUD=0;p->EnableControl(TRUE);
    SET_VIEW(p->edict(),p->edict());
   }
   else if(!strcmp(action,"kit"))CoFR_Impulse101(p);
   else if(!strcmp(action,"encounter")){
    fixtureEnemy=NULL;CBaseEntity* monster=NULL;int budget=0;
    while((monster=UTIL_FindEntityByClassname(monster,"monster_slower"))!=NULL&&budget++<32&&!fixtureEnemy){
     if(monster->pev->health<=0||monster->pev->effects&EF_NODRAW)continue;
     for(int direction=0;direction<8&&!fixtureEnemy;++direction){float angle=direction*3.14159265f/4;Vector away(cos(angle),sin(angle),0);Vector point=monster->pev->origin+away*160;TraceResult tr;
      UTIL_TraceLine(point+Vector(0,0,64),point-Vector(0,0,128),ignore_monsters,p->edict(),&tr);if(tr.fStartSolid||tr.flFraction==1)continue;
      Vector center=tr.vecEndPos+Vector(0,0,37);UTIL_TraceHull(center,center,dont_ignore_monsters,human_hull,p->edict(),&tr);if(tr.fStartSolid||tr.fAllSolid)continue;
      UTIL_SetOrigin(p,center);p->pev->velocity=g_vecZero;Vector aim=UTIL_VecToAngles(monster->pev->origin+Vector(0,0,36)-p->GetGunPosition());aim.x=-aim.x;p->pev->v_angle=aim;p->pev->fixangle=1;fixtureEnemy=monster;
     }
    }
    ALERT(at_console,"COFR encounter fixture target=%d origin=%g,%g,%g\n",fixtureEnemy?ENTINDEX(fixtureEnemy->edict()):0,p->pev->origin.x,p->pev->origin.y,p->pev->origin.z);
   }
   else if(!strcmp(action,"enemies")){if(fixtureEnemy){auto e=fixtureEnemy->pev;ALERT(at_console,"COFR fixture enemy index=%d model=%s health=%g seq=%d frame=%g origin=%g,%g,%g effects=%d\n",ENTINDEX(fixtureEnemy->edict()),STRING(e->model),e->health,e->sequence,e->frame,e->origin.x,e->origin.y,e->origin.z,e->effects);}}
   else if(!strcmp(action,"scene")){
    bool placed=false;const Vector home=p->pev->origin;
    for(int ring=0;ring<=8&&!placed;++ring)for(int direction=0;direction<8&&!placed;++direction){
     float angle=direction*3.14159265f/4;Vector forward(cos(angle),sin(angle),0);Vector point=home+forward*(ring*64.f);TraceResult tr;
     UTIL_TraceLine(point+Vector(0,0,128),point-Vector(0,0,512),ignore_monsters,p->edict(),&tr);if(tr.fStartSolid||tr.flFraction==1)continue;
     Vector center=tr.vecEndPos+Vector(0,0,37);UTIL_TraceHull(center,center+forward*170,ignore_monsters,human_hull,p->edict(),&tr);if(tr.fStartSolid||tr.fAllSolid||tr.flFraction<1)continue;
     UTIL_SetOrigin(p,center);p->pev->v_angle=Vector(0,direction*45.f,0);p->pev->angles=p->pev->v_angle;p->pev->fixangle=1;p->pev->velocity=g_vecZero;
     loadCatalogue();for(const auto& item:catalogue)if(item.classname=="monster_slower"){spawn(p,item);break;}placed=true;
    }
    ALERT(at_console,"COFR scene fixture clear=%d origin=%g,%g,%g\n",placed,p->pev->origin.x,p->pev->origin.y,p->pev->origin.z);
   }
   else if(!strcmp(action,"aim")){CBaseEntity* target=fixtureEnemy;if(!target)target=UTIL_FindEntityByTargetname(NULL,"cofr_creative_spawn");if(target){Vector aim=UTIL_VecToAngles(target->pev->origin+Vector(0,0,36)-p->GetGunPosition());aim.x=-aim.x;p->pev->v_angle=aim;p->pev->fixangle=1;}}
   else if(!strcmp(action,"stats")){ALERT(at_console,"COFR fixture time=%g movement=%d flags=%d view=%d health=%g max=%g clip=%d reserve=%d reload=%d next=%g insert=%g\n",gpGlobals->time,p->pev->movetype,p->pev->flags,p->m_iViewEntIndex,p->pev->health,p->pev->max_health,w?w->m_iClip:-1,w&&w->m_iPrimaryAmmoType>0?p->m_rgAmmo[w->m_iPrimaryAmmoType]:-1,w?w->m_fInReload:0,w?w->m_flNextPrimaryAttack:0,w?w->m_flTimeWeaponIdle:0);}
   else if(!strcmp(action,"health")){p->pev->max_health=1000;p->pev->health=1000;}
   else if(!strcmp(action,"damage"))p->TakeDamage(p->pev,p->pev,10,DMG_GENERIC);
  }
  return true;
 }
 if(!strcmp(command,"cof8_catalogue")){
  if(!allowed())return true;loadCatalogue();for(size_t i=0;i<catalogue.size();++i)ALERT(at_console,"COFR catalogue id=%d class=%s script=%s\n",int(i+1),catalogue[i].classname.c_str(),catalogue[i].script.c_str());return true;
 }
 if(!strcmp(command,"cof8_status")){
  ALERT(at_console,"COFR player health=%g active=%s schema=%d map=%s clip=%d\n",p->pev->health,p->m_pActiveItem?STRING(p->m_pActiveItem->pev->classname):"none",p->cofrInventorySchema,STRING(gpGlobals->mapname),p->m_pActiveItem?((CBasePlayerWeapon*)p->m_pActiveItem)->m_iClip:-1);
  for(int i=1;i<=8;++i)ALERT(at_console,"COFR slot %d script=%s model=%s class=%s\n",i,STRING(p->cofrInventorySlot[i]),STRING(p->cofrInventorySlotModels[i]),STRING(p->cofrInventorySlotClassname[i]));return true;
 }
 if(!strcmp(command,"cof8_entities")){
  if(!allowed())return true;CBaseEntity* e=NULL;int n=0;
  while((e=UTIL_FindEntityByTargetname(e,"cofr_creative_spawn"))!=NULL&&n++<128)ALERT(at_console,"COFR entity index=%d class=%s model=%s modelindex=%d seq=%d frame=%g health=%g effects=%d render=%d alpha=%g\n",ENTINDEX(e->edict()),STRING(e->pev->classname),STRING(e->pev->model),e->pev->modelindex,e->pev->sequence,e->pev->frame,e->pev->health,e->pev->effects,e->pev->rendermode,e->pev->renderamt);return true;
 }
 if(!strcmp(command,"cof8_sync")){slots(p);return true;}
 if(!strcmp(command,"cof8_creative")||!strcmp(command,"cof8_creative_reset")){sync(p,true);return true;}
 if(!strcmp(command,"cof8_notarget")){
  if(!allowed()||!normal(p)){result(p,false,"Notarget requires offline single-player, sv_cheats 1 and normal control.");return true;}
  p->pev->flags^=FL_NOTARGET;sync(p,false);result(p,true,(p->pev->flags&FL_NOTARGET)?"Notarget ON":"Notarget OFF");return true;
 }
 if(!normal(p)){result(p,false,"Inventory is unavailable in the current player state.");return true;}
 int a=0,b=0;const bool first=CMD_ARGC()>1&&cof::creativeNumber(CMD_ARGV(1),a);
 if(!strcmp(command,"cof8_create")||!strcmp(command,"cof8_spawn")){
  if(!allowed()){result(p,false,"Creative requires offline single-player and sv_cheats exactly 1.");return true;}
  loadCatalogue();if(!first||a<1||size_t(a)>catalogue.size()){result(p,false,"Invalid catalogue item.");return true;}
  auto& s=sessions[p];if(gpGlobals->time-s.last<0.1f){result(p,false,"Wait before another spawn.");return true;}s.last=gpGlobals->time;
  const auto& item=catalogue[a-1];
  if(!strcmp(command,"cof8_spawn")){spawn(p,item);return true;}
  if (CMD_ARGC() != 3 || !cof::creativeNumber(CMD_ARGV(2), b) || b > Slots ||
      item.group == cof::CreativeGroup::Entity) {
   result(p, false, "Choose a carried item and an inventory destination.");
   return true;
  }
  creativeGrant(p, item, b);
  return true;
 }
 if(!first||!slot(a)){result(p,false,"Invalid inventory slot.");return true;}
 if (!strcmp(command, "cof8_activate")) {
  if (CMD_ARGC() == 2 && p->cofrInventorySlot[a]) activateSlot(p, a);
 }
 else if(!strcmp(command,"cof8_equip"))p->InventoryEquip(a);
 else if(!strcmp(command,"cof8_use"))p->InventoryUse(a);
 else if(!strcmp(command,"cof8_drop"))p->InventoryDrop(a);
 else if(!strcmp(command,"cof8_move")||!strcmp(command,"cof8_combine")||!strcmp(command,"cof8_dual")){
  if(CMD_ARGC()<3||!cof::creativeNumber(CMD_ARGV(2),b)||!slot(b)||a==b){result(p,false,"Invalid destination slot.");return true;}
  if(!strcmp(command,"cof8_move")){
   if (!p->cofrInventorySlot[a] || p->cofrInventorySlot[b]) return true;
   std::swap(p->cofrInventorySlot[a],p->cofrInventorySlot[b]);std::swap(p->cofrInventorySlotModels[a],p->cofrInventorySlotModels[b]);std::swap(p->cofrInventorySlotClassname[a],p->cofrInventorySlotClassname[b]);
   p->cofrClientinventorySlot[a]=p->cofrClientinventorySlot[b]=-1;slots(p);
  }else if(!strcmp(command,"cof8_dual"))p->DualWield(a,b);
  else{
   // The server derives the recipe; an arbitrary client-supplied result is never trusted.
   std::map<std::string,std::string> x,y;
   cof::ItemScript(load(STRING(p->cofrInventorySlot[a]))).parse(x);cof::ItemScript(load(STRING(p->cofrInventorySlot[b]))).parse(y);
   if(x["combined_result"].empty()||x["combined_result"]!=y["combined_result"]){result(p,false,"These items cannot be combined.");return true;}
   const std::string& recipe = x["combined_result"];
   auto pair = [&](const char* first, const char* second) {
    return (x["classname"] == first && y["classname"] == second) ||
           (x["classname"] == second && y["classname"] == first);
   };
   const bool compatible = recipe == "glockwithtaclight" ? pair("weapon_glock", "item_glocktaclight") && ownedWeapon(p, "weapon_glock") :
    recipe == "phonewithbattery" ? pair("weapon_mobile", "item_phonebattery") && ownedWeapon(p, "weapon_mobile") :
    cof::scriptPath(recipe) && exists(recipe.c_str());
   if (!compatible) { result(p, false, "These items cannot be combined."); return true; }
   p->InventoryCombine(a,b,const_cast<char*>(recipe.c_str()));
   slots(p);
  }
 }else result(p,false,"Unknown inventory action.");
 return true;
}

bool CoFR_OfflineCheats(){return allowed();}
void CoFR_Impulse101(CBasePlayer* p){
 if(!allowed()||!normal(p)){result(p,false,"impulse 101 requires offline single-player, sv_cheats 1 and normal control.");return;}
 loadCatalogue();
 const char* kit[]={"weapon_switchblade","weapon_mobile","weapon_glock","weapon_shotgun","weapon_rifle","weapon_m16","weapon_revolver","weapon_syringe"};
 for(const char* name:kit){
  bool owned=false;for(int i=1;i<=8;++i)if(p->cofrInventorySlot[i]&&p->cofrInventorySlotClassname[i]&&!strcmp(STRING(p->cofrInventorySlotClassname[i]),name))owned=true;
  if(owned)continue;int free=0;for(int i=1;i<=8&&!free;++i)if(!p->cofrInventorySlot[i])free=i;
  if(!free)break;for(const auto& item:catalogue)if(item.classname==name){grant(p,item,free);break;}
 }
 for(int i=0;i<MAX_ITEM_TYPES;++i)for(CBasePlayerItem* item=p->m_rgpPlayerItems[i];item;item=item->m_pNext){
  auto w=static_cast<CBasePlayerWeapon*>(item->GetWeaponPtr());if(!w)continue;ItemInfo info={};if(!w->GetItemInfo(&info))continue;
  if(info.pszAmmo1&&info.iMaxAmmo1>0){int cap=w->m_iId==31?4-w->m_iClip:info.iMaxAmmo1;p->GiveAmmo(cap,const_cast<char*>(info.pszAmmo1),cap,false);}
 }
 slots(p);result(p,true,"Missing kit items added to free slots; owned reserve ammunition replenished.");
}

void CoFR_CoopStore(CBasePlayer* p){
 if(!p)return;std::string key=STRING(p->pev->netname);cooperativeCarry.erase(key);if(!p->IsAlive())return;Carry saved;
 for(int i=1;i<=8;++i){saved.script[i]=STRING(p->cofrInventorySlot[i]);saved.model[i]=STRING(p->cofrInventorySlotModels[i]);saved.name[i]=STRING(p->cofrInventorySlotClassname[i]);}
 for(int i=0;i<MAX_ITEM_TYPES;++i)for(auto item=p->m_rgpPlayerItems[i];item;item=item->m_pNext){auto w=static_cast<CBasePlayerWeapon*>(item->GetWeaponPtr());if(w){saved.clips[STRING(w->pev->classname)]=w->m_iClip;ItemInfo info={};if(w->GetItemInfo(&info)){if(info.pszAmmo1&&w->m_iPrimaryAmmoType>0)saved.ammo[info.pszAmmo1]=p->m_rgAmmo[w->m_iPrimaryAmmoType];if(info.pszAmmo2&&w->m_iSecondaryAmmoType>0)saved.ammo[info.pszAmmo2]=p->m_rgAmmo[w->m_iSecondaryAmmoType];}}}
 cooperativeCarry[key]=std::move(saved);
}
void CoFR_CoopRestore(CBasePlayer* p){
 if(!p||!p->IsAlive())return;auto it=cooperativeCarry.find(STRING(p->pev->netname));if(it==cooperativeCarry.end())return;auto saved=it->second;
 for(int i=1;i<=8;++i)if(saved.name[i].compare(0,7,"weapon_")==0&&!p->HasNamedPlayerItem(saved.name[i].c_str()))p->GiveNamedItem(saved.name[i].c_str(),false,p->IsDonator());
 for(int i=1;i<=8;++i){p->cofrInventorySlot[i]=ALLOC_STRING(saved.script[i].c_str());p->cofrInventorySlotModels[i]=ALLOC_STRING(saved.model[i].c_str());p->cofrInventorySlotClassname[i]=ALLOC_STRING(saved.name[i].c_str());p->cofrClientinventorySlot[i]=-1;}
 for(int i=0;i<MAX_ITEM_TYPES;++i)for(auto item=p->m_rgpPlayerItems[i];item;item=item->m_pNext){auto w=static_cast<CBasePlayerWeapon*>(item->GetWeaponPtr());if(w){auto clip=saved.clips.find(STRING(w->pev->classname));if(clip!=saved.clips.end())w->m_iClip=clip->second;}}
 for(const auto& ammo:saved.ammo){int index=p->GetAmmoIndex(ammo.first.c_str());if(index>0&&index<MAX_AMMO_SLOTS)p->m_rgAmmo[index]=std::max(0,std::min(32767,ammo.second));}
 cooperativeCarry.erase(STRING(p->pev->netname));p->cofrInventorySchema=1;slots(p);
}
