// hashpak.h -- the .hpk pack of custom resources stored by hash (hashpak.c)

#ifndef HASHPAK_H
#define HASHPAK_H

#define HASHPAK_EXTENSION	".hpk"
#define HASHPAK_VERSION		0x0001

#define MAX_FILE_SIZE		0x20000
#define MAX_FILE_ENTRIES	0x8000

extern	cvar_t	hpk_maxsize;	// sv_main.c

qboolean	HPAK_GetDataPointer( char *pakname, struct resource_s *pResource, unsigned char **pbuffer, int *bufsize );
void		HPAK_AddLump( qboolean bUseQueue, char *pakname, struct resource_s *pResource, void *pData, FileHandle_t fpSource );
void		HPAK_FlushHostQueue( void );
void		HPAK_RemoveLump( char *pakname, struct resource_s *pResource );
qboolean	HPAK_ResourceForIndex( char *pakname, int nIndex, struct resource_s *pResource );
qboolean	HPAK_ResourceForHash( char *pakname, unsigned char *hash, struct resource_s *pResourceEntry );
void		HPAK_CreatePak( char *pakname, struct resource_s *pResource, void *pData, FileHandle_t fpSource );
void		HPAK_Init( void );
char		*HPAK_GetItem( int item );
void		HPAK_CheckSize( char *pakname );
void		HPAK_ValidatePak( char *fullpakname );
void		HPAK_CheckIntegrity( char *pakname );

#endif // HASHPAK_H
