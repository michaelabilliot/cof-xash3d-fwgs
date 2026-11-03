/* Audited native provenance and read-only death evidence. No hooks, writes or runtime PDB load.
 * Audited against the shipped, PDB-matched CoF server (SHA in the report). */
#if defined(_WIN32) && defined(_M_IX86)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "sv_ragdoll_native_layout.h"
static qboolean SV_RagdollReadable( const void *pointer, size_t bytes )
{
    MEMORY_BASIC_INFORMATION region;
    uintptr_t address=(uintptr_t)pointer, base;
    DWORD protect;
    if( !pointer || !VirtualQuery(pointer,&region,sizeof(region)) || region.State!=MEM_COMMIT || (region.Protect&PAGE_GUARD)) return false;
    protect=region.Protect&0xff;
    if(protect!=PAGE_READONLY && protect!=PAGE_READWRITE && protect!=PAGE_WRITECOPY && protect!=PAGE_EXECUTE_READ && protect!=PAGE_EXECUTE_READWRITE && protect!=PAGE_EXECUTE_WRITECOPY) return false;
    base=(uintptr_t)region.BaseAddress;
    return address>=base && address-base<=region.RegionSize && bytes<=region.RegionSize-(address-base);
}
static int SV_RagdollNativeKilled( const edict_t *victim )
{
    HMODULE module;
    const IMAGE_DOS_HEADER *dos;
    const IMAGE_NT_HEADERS *nt;
    const byte *object,*image;
    uintptr_t table, killed, relative;
    static const byte memory_read[]={0x8b,0x88,0xe8,0x02,0x00,0x00};
    static const byte killed_bit[]={0x68,0x80,0x00,0x00,0x00};
    if(!SV_IsValidEdict(victim) || Q_strncmp(SV_GetString(victim->v.classname),"monster_",8)) return -1;
    object=(const byte*)victim->pvPrivateData;
    if(!SV_RagdollReadable(object,COF_MONSTER_BYTES)) return -1;
    memcpy(&table,object,sizeof(table));
    if(!SV_RagdollReadable((const void*)table,COF_KILLED_SLOT+sizeof(killed))) return -1;
    if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(LPCSTR)table,&module)) return -1;
    image=(const byte*)module;dos=(const IMAGE_DOS_HEADER*)image;
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 || dos->e_lfanew>4096) return -1;
    nt=(const IMAGE_NT_HEADERS*)(image+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE || nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_I386 || nt->FileHeader.TimeDateStamp!=0x5048ddb2 || nt->OptionalHeader.SizeOfImage!=0x259000) return -1;
    if(table<(uintptr_t)image || table-(uintptr_t)image>0x259000-COF_KILLED_SLOT-sizeof(killed)) return -1;
    if(memcmp(image+0x42df,memory_read,sizeof(memory_read)) || memcmp(image+0x89b4c,killed_bit,sizeof(killed_bit)) || memcmp(image+0x4695c,killed_bit,sizeof(killed_bit))) return -1;
    memcpy(&killed,(const byte*)table+COF_KILLED_SLOT,sizeof(killed));
    relative=killed-(uintptr_t)image;
    /* Only the disassembled CBaseMonster/CZombie/CFaster Killed implementations. */
    if(relative!=0xa73d0 && relative!=0x89a80 && relative!=0x46890) return -1;
    return SV_RagdollDecodeKilled(object,COF_MONSTER_BYTES,(uintptr_t)&victim->v);
}
/* No executable patching: only the verified primary trace frames qualify. */
#include <intrin.h>
typedef struct {
    uintptr_t frame;
    const byte *image;
    int weapon, pellet, total, rejected;
    uint32_t damage_entity;
    float damage_amount;
} ragdoll_native_shot_t;
static qboolean SV_RagdollShotImage(const byte *image)
{
    const IMAGE_DOS_HEADER *dos=(const IMAGE_DOS_HEADER*)image;
    const IMAGE_NT_HEADERS *nt;
    static const byte player[]={0x55,0x8b,0xec,0x81,0xec,0x44,0x01,0,0,0x53,0x56,0x57,0x89,0x4d,0xfc};
    static const byte generic[]={0x55,0x8b,0xec,0x81,0xec,0x84,0x01,0,0,0x53,0x56,0x57,0x89,0x4d,0xfc};
    static const byte player_loop[]={0xc7,0x45,0x94,1,0,0,0,0xeb,9,0x8b,0x45,0x94,0x83,0xc0,1,0x89,0x45,0x94,0x8b,0x45,0x94,0x3b,0x45,0x0c};
    static const byte generic_loop[]={0xc7,0x45,0xa0,1,0,0,0,0xeb,9,0x8b,0x45,0xa0,0x83,0xc0,1,0x89,0x45,0xa0,0x8b,0x45,0xa0,0x3b,0x45,8};
    static const byte player_call[]={0xe8,0x3a,0x76,0x0c,0};
    static const byte generic_call[]={0xe8,0x56,0x80,0x0c,0};
    if(!SV_RagdollReadable(image,4096) || dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 || dos->e_lfanew>4096-(int)sizeof(*nt)) return false;
    nt=(const IMAGE_NT_HEADERS*)(image+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE || nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_I386 || nt->FileHeader.TimeDateStamp!=0x5048ddb2 || nt->OptionalHeader.SizeOfImage!=0x259000) return false;
    return SV_RagdollReadable(image+0xaa250,sizeof(player)) && !memcmp(image+0xaa250,player,sizeof(player)) &&
        SV_RagdollReadable(image+0xa99c0,sizeof(generic)) && !memcmp(image+0xa99c0,generic,sizeof(generic)) &&
        SV_RagdollReadable(image+0xaa3a8,sizeof(player_loop)) && !memcmp(image+0xaa3a8,player_loop,sizeof(player_loop)) &&
        SV_RagdollReadable(image+0xa9abf,sizeof(generic_loop)) && !memcmp(image+0xa9abf,generic_loop,sizeof(generic_loop)) &&
        SV_RagdollReadable(image+0xaa641,5) && !memcmp(image+0xaa641,player_call,5) &&
        SV_RagdollReadable(image+0xa9c25,5) && !memcmp(image+0xa9c25,generic_call,5) && SV_RagdollReadable(image+0x22a700,12);
}
/* Also used by the standalone x86 frame harness. Never follows heap pointers
 * as stack frames; all reads stay inside this thread's committed stack. */
static qboolean SV_RagdollDecodeShot(uintptr_t cursor,uintptr_t low,uintptr_t high,const byte *image,uintptr_t expected_pev,ragdoll_native_shot_t *out)
{
    for(int depth=0;depth<32;++depth) {
        uintptr_t next,ret,frame,object,pev;int pellet,total;
        if((cursor&3) || cursor<low || cursor>high || high-cursor<8) return false;
        memcpy(&next,(const void*)cursor,4);memcpy(&ret,(const void*)(cursor+4),4);
        if(next<=cursor || next>high || high-next<0x60) return false;
        if(ret==(uintptr_t)image+0xaa646 || ret==(uintptr_t)image+0xa9c2a) {
            const qboolean player=ret==(uintptr_t)image+0xaa646;
            frame=next;
            if(frame-low<(uintptr_t)(player?0x6c:0x60)) return false;
            memcpy(&pellet,(const void*)(frame-(player?0x6c:0x60)),4);
            memcpy(&total,(const void*)(frame+(player?12:8)),4);
            memcpy(&object,(const void*)(frame-4),4);
            if(!SV_RagdollReadable((const void*)object,8)) return false;
            memcpy(&pev,(const void*)(object+4),4);
            if(pev!=expected_pev || total<1 || total>24 || pellet<1 || pellet>total) return false;
            memset(out,0,sizeof(*out));out->frame=frame;out->image=image;out->pellet=pellet;out->total=total;
            memcpy(&out->damage_entity,image+0x22a700,4);memcpy(&out->damage_amount,image+0x22a704,4);
            out->weapon=REF_RAGDOLL_RIFLE;
            return true;
        }
        cursor=next;
    }
    return false;
}
static qboolean SV_RagdollBulletContext(const edict_t *shooter,uintptr_t frame,ragdoll_native_shot_t *out)
{
    static const byte *image;static int generation=-1;
    NT_TIB *tib=(NT_TIB*)NtCurrentTeb();
    uintptr_t low=(uintptr_t)tib->StackLimit,high=(uintptr_t)tib->StackBase,cursor=frame;
    memset(out,0,sizeof(*out));
    if(!SV_IsValidEdict(shooter) || !FBitSet(shooter->v.flags,FL_CLIENT)) {out->rejected=1;return false;}
    if(generation!=svs.spawncount) {generation=svs.spawncount;image=NULL;}
    if(!image) for(int depth=0;depth<32;++depth) {
        uintptr_t next,ret;HMODULE module;
        if((cursor&3) || cursor<low || cursor>high || high-cursor<8) break;
        memcpy(&next,(const void*)cursor,4);memcpy(&ret,(const void*)(cursor+4),4);
        if(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(LPCSTR)ret,&module) && SV_RagdollShotImage((const byte*)module)) {image=(const byte*)module;break;}
        if(next<=cursor) break;cursor=next;
    }
    if(!image) {out->rejected=2;return false;}
    if(!SV_RagdollDecodeShot(frame,low,high,image,(uintptr_t)&shooter->v,out)) {out->rejected=3;return false;}
    if(Q_stristr(SV_GetString(shooter->v.viewmodel),"shotgun")) out->weapon=REF_RAGDOLL_SHOTGUN;
    else if(Q_stristr(SV_GetString(shooter->v.viewmodel),"glock")) out->weapon=REF_RAGDOLL_PISTOL;
    return true;
}

#else
static int SV_RagdollNativeKilled( const edict_t *victim ) { (void)victim; return -1; }
typedef struct { uintptr_t frame; const byte *image; int weapon,pellet,total,rejected; uint32_t damage_entity; float damage_amount; } ragdoll_native_shot_t;
static qboolean SV_RagdollBulletContext(const edict_t *shooter,uintptr_t frame,ragdoll_native_shot_t *out) { (void)shooter;(void)frame;memset(out,0,sizeof(*out));out->rejected=2;return false; }
#endif
