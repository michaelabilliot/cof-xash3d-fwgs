// info.h -- key\value info strings (info.c)

#ifndef INFO_H
#define INFO_H

#define MAX_KV_LEN			128		// longest key or value, terminator included
#define MAX_INFO_VALUES		4		// Info_ValueForKey rotates through this many buffers

const char	*Info_ValueForKey( const char *s, const char *key );
void		Info_RemoveKey( char *s, const char *key );
void		Info_RemovePrefixedKeys( char *start, char prefix );
qboolean	Info_IsKeyImportant( const char *key );
char		*Info_FindLargestKey( char *s, int maxsize );
void		Info_SetValueForStarKey( char *s, const char *key, const char *value, int maxsize );
void		Info_SetValueForKey( char *s, const char *key, const char *value, int maxsize );
void		Info_Print( const char *s );
int			Info_IsValid( const char *s );

#define MAX_INFO_STRING		256		// serverinfo (common.c)
#define MAX_SERVERINFO_STRING	512	// limit COM_SetupDirectories passes for serverinfo
#define MAX_LOCALINFO		( MAX_INFO_STRING * 128 )

#endif // INFO_H
