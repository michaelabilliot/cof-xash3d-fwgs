// czmodelcheck.cpp -- recognizes the Condition Zero player models by their CRC

extern "C"
{
#include "quakedef.h"

qboolean IsCZPlayerModel( uint32 crc, const char *filename );
}

qboolean IsCZPlayerModel( uint32 crc, const char *filename )
{
	if ( !filename )
		return FALSE;

	switch ( crc )
	{
	case 0xC37369F6:
	case 0x29FE156C:
		if ( !_stricmp( filename, "models/player/leet/leet.mdl" ) )
			return TRUE;
		break;
	case 0x1AAA3360:
	case 0x35AC6FED:
		if ( !_stricmp( filename, "models/player/gign/gign.mdl" ) )
			return TRUE;
		break;
	case 0x02B95E5F:
	case 0x72DB74E4:
		if ( !_stricmp( filename, "models/player/vip/vip.mdl" ) )
			return TRUE;
		break;
	case 0x0DA8922A:
	case 0x56DD2D02:
		if ( !_stricmp( filename, "models/player/gsg9/gsg9.mdl" ) )
			return TRUE;
		break;
	case 0x1F3CD80B:
	case 0x1B6C4115:
		if ( !_stricmp( filename, "models/player/guerilla/guerilla.mdl" ) )
			return TRUE;
		break;
	case 0xA37D8680:
	case 0x4986827B:
		if ( !_stricmp( filename, "models/player/arctic/arctic.mdl" ) )
			return TRUE;
		break;
	case 0xEC43F76D:
	case 0x270FB2D7:
		if ( !_stricmp( filename, "models/player/terror/terror.mdl" ) )
			return TRUE;
		break;
	case 0xC7F0DBF3:
	case 0x068168DB:
		if ( !_stricmp( filename, "models/player/urban/urban.mdl" ) )
			return TRUE;
		break;
	case 0x43E67FF3:
	case 0xF141AE3F:
		if ( !_stricmp( filename, "models/player/sas/sas.mdl" ) )
			return TRUE;
		break;
	case 0x3BCAA016:
		if ( !_stricmp( filename, "models/player/militia/militia.mdl" ) )
			return TRUE;
		break;
	case 0x27FB4D2F:
		if ( !_stricmp( filename, "models/player/spetsnaz/spetsnaz.mdl" ) )
			return TRUE;
		break;
	}

	return FALSE;
}
