// decal.h -- decal names known to the server

#ifndef DECAL_H
#define DECAL_H

#define MAX_DECALS	512

typedef struct
{
	char			name[16];
	unsigned char	ucFlags;
} decalname_t;

extern	decalname_t	sv_decalnames[MAX_DECALS];
extern	int			sv_decalnamecount;

int		Draw_DecalSize( int number );

#endif // DECAL_H
