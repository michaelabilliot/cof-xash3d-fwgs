// modinfo.h -- liblist.gam information about the running mod

#ifndef MODINFO_H
#define MODINFO_H

typedef enum
{
	BOTH = 0,
	SINGLEPLAYER_ONLY,
	MULTIPLAYER_ONLY,
} MOD_GAMEPLAY_TYPE_E;

// 0x23C bytes in this build (SV_ResetModInfo): the Linux build adds
// num_edicts and clientcrccheck at the end
typedef struct modinfo_s
{
	qboolean	bIsMod;
	char		szInfo[256];
	char		szDL[256];
	char		szHLVersion[32];
	int			version;
	int			size;
	qboolean	svonly;
	qboolean	cldll;
	qboolean	secure;
	MOD_GAMEPLAY_TYPE_E	type;
} modinfo_t;

extern	modinfo_t	gmodinfo;

#endif // MODINFO_H
