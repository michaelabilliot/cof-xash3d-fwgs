// sv_secure.c -- file names of the secure modules

#include "quakedef.h"

char *SV_GetFileNameModuleS( void )
{
	static char	moduleFileName[MAX_PATH];
	char		gamedir[MAX_PATH];
	char		szModule[20];

	szModule[0] = 'M';
	szModule[1] = 'o';
	szModule[2] = 'd';
	szModule[3] = 'u';
	szModule[4] = 'l';
	szModule[5] = 'e';
	szModule[6] = 'S';
	szModule[7] = '.';
	szModule[8] = 'd';
	szModule[9] = 'l';
	szModule[10] = 'l';
	szModule[11] = '\0';

	COM_FileBase( com_gamedir, gamedir );
	_snprintf( moduleFileName, sizeof( moduleFileName ) - 1, "%s/%s", gamedir, szModule );
	moduleFileName[sizeof( moduleFileName ) - 1] = 0;
	return moduleFileName;
}

char *SV_GetFileNameModuleC( void )
{
	static char	moduleFileName[MAX_PATH];
	char		gamedir[MAX_PATH];
	char		szModule[20];

	szModule[0] = 'M';
	szModule[1] = 'o';
	szModule[2] = 'd';
	szModule[3] = 'u';
	szModule[4] = 'l';
	szModule[5] = 'e';
	szModule[6] = 'C';
	szModule[7] = '.';
	szModule[8] = 'd';
	szModule[9] = 'l';
	szModule[10] = 'l';
	szModule[11] = '\0';

	COM_FileBase( com_gamedir, gamedir );
	_snprintf( moduleFileName, sizeof( moduleFileName ) - 1, "%s/%s", gamedir, szModule );
	moduleFileName[sizeof( moduleFileName ) - 1] = 0;
	return moduleFileName;
}
