/* Read-only x86 CoF layout. PDB offsets confirmed by native instructions.
 * Caller must validate allocation readability, module identity and class vtable. */
#include <stdint.h>
enum { COF_MONSTER_PEV=4, COF_MONSTER_MEMORY=0x2e8, COF_MONSTER_BYTES=0x2ec, COF_KILLED_SLOT=0x6c };
static int SV_RagdollDecodeKilled( const unsigned char *object, size_t bytes, uintptr_t expected_pev )
{
    uint32_t pev, memory;
    if( !object || bytes<COF_MONSTER_BYTES || expected_pev>UINT32_MAX ) return -1;
    memcpy(&pev,object+COF_MONSTER_PEV,sizeof(pev));
    if( pev!=(uint32_t)expected_pev ) return -1;
    memcpy(&memory,object+COF_MONSTER_MEMORY,sizeof(memory));
    return (memory&0x80)!=0;
}
