// delta.c -- delta compression of entity, client, user command, weapon and
// event structures, driven by the descriptions in delta.lst

#include "quakedef.h"
#include "mathlib.h"
#include "usercmd.h"
#include "entity_state.h"
#include "event_args.h"
#include "cvardef.h"
#include "cmd.h"
#include "mem.h"
#include "delta.h"
#include "custom.h"
#include "server.h"
#include "client.h"

#include <ctype.h>
#include <stddef.h>

typedef struct delta_link_s
{
	struct delta_link_s		*next;
	delta_description_t		*delta;
} delta_link_t;

typedef struct delta_definition_s
{
	char	*fieldName;
	int		fieldOffset;
} delta_definition_t;

typedef struct delta_definition_list_s
{
	struct delta_definition_list_s	*next;
	char							*ptypename;
	int								numelements;
	delta_definition_t				*pdefinition;
} delta_definition_list_t;

typedef struct delta_registry_s
{
	struct delta_registry_s		*next;
	char						*name;
	delta_t						*pdesc;
} delta_registry_t;

static delta_definition_list_t	*g_defs;
static delta_encoder_t			*g_encoders;
static delta_registry_t			*g_deltaregistry;

#define DELTA_DEF( structname, fieldname )	{ #fieldname, offsetof( structname, fieldname ) }

delta_definition_t g_WeaponDataDefinition[] =
{
	DELTA_DEF( weapon_data_t, m_iId ),
	DELTA_DEF( weapon_data_t, m_iClip ),
	DELTA_DEF( weapon_data_t, m_flNextPrimaryAttack ),
	DELTA_DEF( weapon_data_t, m_flNextSecondaryAttack ),
	DELTA_DEF( weapon_data_t, m_flTimeWeaponIdle ),
	DELTA_DEF( weapon_data_t, m_fInReload ),
	DELTA_DEF( weapon_data_t, m_fInSpecialReload ),
	DELTA_DEF( weapon_data_t, m_flNextReload ),
	DELTA_DEF( weapon_data_t, m_flPumpTime ),
	DELTA_DEF( weapon_data_t, m_fReloadTime ),
	DELTA_DEF( weapon_data_t, m_fAimedDamage ),
	DELTA_DEF( weapon_data_t, m_fNextAimBonus ),
	DELTA_DEF( weapon_data_t, m_fInZoom ),
	DELTA_DEF( weapon_data_t, m_iWeaponState ),
	DELTA_DEF( weapon_data_t, iuser1 ),
	DELTA_DEF( weapon_data_t, iuser2 ),
	DELTA_DEF( weapon_data_t, iuser3 ),
	DELTA_DEF( weapon_data_t, iuser4 ),
	DELTA_DEF( weapon_data_t, fuser1 ),
	DELTA_DEF( weapon_data_t, fuser2 ),
	DELTA_DEF( weapon_data_t, fuser3 ),
	DELTA_DEF( weapon_data_t, fuser4 ),
};

delta_definition_t g_EventDataDefinition[] =
{
	DELTA_DEF( event_args_t, entindex ),
	DELTA_DEF( event_args_t, origin[0] ),
	DELTA_DEF( event_args_t, origin[1] ),
	DELTA_DEF( event_args_t, origin[2] ),
	DELTA_DEF( event_args_t, angles[0] ),
	DELTA_DEF( event_args_t, angles[1] ),
	DELTA_DEF( event_args_t, angles[2] ),
	DELTA_DEF( event_args_t, fparam1 ),
	DELTA_DEF( event_args_t, fparam2 ),
	DELTA_DEF( event_args_t, iparam1 ),
	DELTA_DEF( event_args_t, iparam2 ),
	DELTA_DEF( event_args_t, bparam1 ),
	DELTA_DEF( event_args_t, bparam2 ),
	DELTA_DEF( event_args_t, ducking ),
};

struct delta_description_s g_MetaDescription[] =
{
	{ DT_INTEGER, "fieldType", offsetof( delta_description_t, fieldType ), 1, 32, 1.0, 1.0, 0, { 0, 0 } },
	{ DT_STRING, "fieldName", offsetof( delta_description_t, fieldName ), 1, 1, 1.0, 1.0, 0, { 0, 0 } },
	{ DT_INTEGER, "fieldOffset", offsetof( delta_description_t, fieldOffset ), 1, 16, 1.0, 1.0, 0, { 0, 0 } },
	{ DT_INTEGER, "fieldSize", offsetof( delta_description_t, fieldSize ), 1, 8, 1.0, 1.0, 0, { 0, 0 } },
	{ DT_INTEGER, "significant_bits", offsetof( delta_description_t, significant_bits ), 1, 8, 1.0, 1.0, 0, { 0, 0 } },
	{ DT_FLOAT, "premultiply", offsetof( delta_description_t, premultiply ), 1, 32, 4000.0, 1.0, 0, { 0, 0 } },
	{ DT_FLOAT, "postmultiply", offsetof( delta_description_t, postmultiply ), 1, 32, 4000.0, 1.0, 0, { 0, 0 } },
};

delta_t g_MetaDelta =
{
	0, ARRAYSIZE( g_MetaDescription ), "", NULL, g_MetaDescription
};

delta_definition_t g_DeltaDataDefinition[] =
{
	DELTA_DEF( delta_description_t, fieldType ),
	DELTA_DEF( delta_description_t, fieldName ),
	DELTA_DEF( delta_description_t, fieldOffset ),
	DELTA_DEF( delta_description_t, fieldSize ),
	DELTA_DEF( delta_description_t, significant_bits ),
	DELTA_DEF( delta_description_t, premultiply ),
	DELTA_DEF( delta_description_t, postmultiply ),
	DELTA_DEF( delta_description_t, flags ),
};

delta_definition_t g_UsercmdDataDefinition[] =
{
	DELTA_DEF( usercmd_t, lerp_msec ),
	DELTA_DEF( usercmd_t, msec ),
	DELTA_DEF( usercmd_t, lightlevel ),
	DELTA_DEF( usercmd_t, viewangles[0] ),
	DELTA_DEF( usercmd_t, viewangles[1] ),
	DELTA_DEF( usercmd_t, viewangles[2] ),
	DELTA_DEF( usercmd_t, buttons ),
	DELTA_DEF( usercmd_t, forwardmove ),
	DELTA_DEF( usercmd_t, sidemove ),
	DELTA_DEF( usercmd_t, upmove ),
	DELTA_DEF( usercmd_t, impulse ),
	DELTA_DEF( usercmd_t, weaponselect ),
	DELTA_DEF( usercmd_t, impact_index ),
	DELTA_DEF( usercmd_t, impact_position[0] ),
	DELTA_DEF( usercmd_t, impact_position[1] ),
	DELTA_DEF( usercmd_t, impact_position[2] ),
	DELTA_DEF( usercmd_t, is_donator ),
};

delta_definition_t g_ClientDataDefinition[] =
{
	DELTA_DEF( clientdata_t, origin[0] ),
	DELTA_DEF( clientdata_t, origin[1] ),
	DELTA_DEF( clientdata_t, origin[2] ),
	DELTA_DEF( clientdata_t, velocity[0] ),
	DELTA_DEF( clientdata_t, velocity[1] ),
	DELTA_DEF( clientdata_t, velocity[2] ),
	DELTA_DEF( clientdata_t, viewmodel ),
	DELTA_DEF( clientdata_t, punchangle[0] ),
	DELTA_DEF( clientdata_t, punchangle[1] ),
	DELTA_DEF( clientdata_t, punchangle[2] ),
	DELTA_DEF( clientdata_t, flags ),
	DELTA_DEF( clientdata_t, waterlevel ),
	DELTA_DEF( clientdata_t, watertype ),
	DELTA_DEF( clientdata_t, view_ofs[0] ),
	DELTA_DEF( clientdata_t, view_ofs[1] ),
	DELTA_DEF( clientdata_t, view_ofs[2] ),
	DELTA_DEF( clientdata_t, health ),
	DELTA_DEF( clientdata_t, bInDuck ),
	DELTA_DEF( clientdata_t, weapons ),
	DELTA_DEF( clientdata_t, flTimeStepSound ),
	DELTA_DEF( clientdata_t, flDuckTime ),
	DELTA_DEF( clientdata_t, flSwimTime ),
	DELTA_DEF( clientdata_t, waterjumptime ),
	DELTA_DEF( clientdata_t, maxspeed ),
	DELTA_DEF( clientdata_t, m_iId ),
	DELTA_DEF( clientdata_t, ammo_nails ),
	DELTA_DEF( clientdata_t, ammo_shells ),
	DELTA_DEF( clientdata_t, ammo_cells ),
	DELTA_DEF( clientdata_t, ammo_rockets ),
	DELTA_DEF( clientdata_t, m_flNextAttack ),
	DELTA_DEF( clientdata_t, physinfo ),
	DELTA_DEF( clientdata_t, fov ),
	DELTA_DEF( clientdata_t, weaponanim ),
	DELTA_DEF( clientdata_t, tfstate ),
	DELTA_DEF( clientdata_t, pushmsec ),
	DELTA_DEF( clientdata_t, deadflag ),
	DELTA_DEF( clientdata_t, iuser1 ),
	DELTA_DEF( clientdata_t, iuser2 ),
	DELTA_DEF( clientdata_t, iuser3 ),
	DELTA_DEF( clientdata_t, iuser4 ),
	DELTA_DEF( clientdata_t, fuser1 ),
	DELTA_DEF( clientdata_t, fuser2 ),
	DELTA_DEF( clientdata_t, fuser3 ),
	DELTA_DEF( clientdata_t, fuser4 ),
	DELTA_DEF( clientdata_t, vuser1[0] ),
	DELTA_DEF( clientdata_t, vuser1[1] ),
	DELTA_DEF( clientdata_t, vuser1[2] ),
	DELTA_DEF( clientdata_t, vuser2[0] ),
	DELTA_DEF( clientdata_t, vuser2[1] ),
	DELTA_DEF( clientdata_t, vuser2[2] ),
	DELTA_DEF( clientdata_t, vuser3[0] ),
	DELTA_DEF( clientdata_t, vuser3[1] ),
	DELTA_DEF( clientdata_t, vuser3[2] ),
	DELTA_DEF( clientdata_t, vuser4[0] ),
	DELTA_DEF( clientdata_t, vuser4[1] ),
	DELTA_DEF( clientdata_t, vuser4[2] ),
	DELTA_DEF( clientdata_t, isdonator ),
};

delta_definition_t g_EntityDataDefinition[] =
{
	DELTA_DEF( entity_state_t, startpos[0] ),
	DELTA_DEF( entity_state_t, startpos[1] ),
	DELTA_DEF( entity_state_t, startpos[2] ),
	DELTA_DEF( entity_state_t, endpos[0] ),
	DELTA_DEF( entity_state_t, endpos[1] ),
	DELTA_DEF( entity_state_t, endpos[2] ),
	DELTA_DEF( entity_state_t, impacttime ),
	DELTA_DEF( entity_state_t, starttime ),
	DELTA_DEF( entity_state_t, origin[0] ),
	DELTA_DEF( entity_state_t, origin[1] ),
	DELTA_DEF( entity_state_t, origin[2] ),
	DELTA_DEF( entity_state_t, angles[0] ),
	DELTA_DEF( entity_state_t, angles[1] ),
	DELTA_DEF( entity_state_t, angles[2] ),
	DELTA_DEF( entity_state_t, modelindex ),
	DELTA_DEF( entity_state_t, frame ),
	DELTA_DEF( entity_state_t, movetype ),
	DELTA_DEF( entity_state_t, colormap ),
	DELTA_DEF( entity_state_t, skin ),
	DELTA_DEF( entity_state_t, solid ),
	DELTA_DEF( entity_state_t, scale ),
	DELTA_DEF( entity_state_t, effects ),
	DELTA_DEF( entity_state_t, sequence ),
	DELTA_DEF( entity_state_t, animtime ),
	DELTA_DEF( entity_state_t, framerate ),
	DELTA_DEF( entity_state_t, controller[0] ),
	DELTA_DEF( entity_state_t, controller[1] ),
	DELTA_DEF( entity_state_t, controller[2] ),
	DELTA_DEF( entity_state_t, controller[3] ),
	DELTA_DEF( entity_state_t, blending[0] ),
	DELTA_DEF( entity_state_t, blending[1] ),
	DELTA_DEF( entity_state_t, body ),
	DELTA_DEF( entity_state_t, owner ),
	DELTA_DEF( entity_state_t, rendermode ),
	DELTA_DEF( entity_state_t, renderamt ),
	DELTA_DEF( entity_state_t, renderfx ),
	DELTA_DEF( entity_state_t, rendercolor.r ),
	DELTA_DEF( entity_state_t, rendercolor.g ),
	DELTA_DEF( entity_state_t, rendercolor.b ),
	DELTA_DEF( entity_state_t, weaponmodel ),
	DELTA_DEF( entity_state_t, gaitsequence ),
	DELTA_DEF( entity_state_t, mins[0] ),
	DELTA_DEF( entity_state_t, mins[1] ),
	DELTA_DEF( entity_state_t, mins[2] ),
	DELTA_DEF( entity_state_t, maxs[0] ),
	DELTA_DEF( entity_state_t, maxs[1] ),
	DELTA_DEF( entity_state_t, maxs[2] ),
	DELTA_DEF( entity_state_t, aiment ),
	DELTA_DEF( entity_state_t, basevelocity[0] ),
	DELTA_DEF( entity_state_t, basevelocity[1] ),
	DELTA_DEF( entity_state_t, basevelocity[2] ),
	DELTA_DEF( entity_state_t, friction ),
	DELTA_DEF( entity_state_t, gravity ),
	DELTA_DEF( entity_state_t, spectator ),
	DELTA_DEF( entity_state_t, velocity[0] ),
	DELTA_DEF( entity_state_t, velocity[1] ),
	DELTA_DEF( entity_state_t, velocity[2] ),
	DELTA_DEF( entity_state_t, team ),
	DELTA_DEF( entity_state_t, playerclass ),
	DELTA_DEF( entity_state_t, health ),
	DELTA_DEF( entity_state_t, usehull ),
	DELTA_DEF( entity_state_t, oldbuttons ),
	DELTA_DEF( entity_state_t, onground ),
	DELTA_DEF( entity_state_t, iStepLeft ),
	DELTA_DEF( entity_state_t, flFallVelocity ),
	DELTA_DEF( entity_state_t, weaponanim ),
	DELTA_DEF( entity_state_t, eflags ),
	DELTA_DEF( entity_state_t, iuser1 ),
	DELTA_DEF( entity_state_t, iuser2 ),
	DELTA_DEF( entity_state_t, iuser3 ),
	DELTA_DEF( entity_state_t, iuser4 ),
	DELTA_DEF( entity_state_t, fuser1 ),
	DELTA_DEF( entity_state_t, fuser2 ),
	DELTA_DEF( entity_state_t, fuser3 ),
	DELTA_DEF( entity_state_t, fuser4 ),
	DELTA_DEF( entity_state_t, vuser1[0] ),
	DELTA_DEF( entity_state_t, vuser1[1] ),
	DELTA_DEF( entity_state_t, vuser1[2] ),
	DELTA_DEF( entity_state_t, vuser2[0] ),
	DELTA_DEF( entity_state_t, vuser2[1] ),
	DELTA_DEF( entity_state_t, vuser2[2] ),
	DELTA_DEF( entity_state_t, vuser3[0] ),
	DELTA_DEF( entity_state_t, vuser3[1] ),
	DELTA_DEF( entity_state_t, vuser3[2] ),
	DELTA_DEF( entity_state_t, vuser4[0] ),
	DELTA_DEF( entity_state_t, vuser4[1] ),
	DELTA_DEF( entity_state_t, vuser4[2] ),
};

/*
==================
DELTA_FindField

==================
*/
delta_description_t *DELTA_FindField( delta_t *pFields, const char *pszField )
{
	int						i;
	delta_description_t		*pitem;

	for ( i = 0; i < pFields->fieldCount; i++ )
	{
		pitem = &pFields->pdd[i];
		if ( !Q_strcasecmp( pitem->fieldName, pszField ) )
			return pitem;
	}

	Con_Printf( "DELTA_FindField:  Warning, couldn't find %s\n", pszField );
	return NULL;
}

/*
==================
DELTA_FindFieldIndex

==================
*/
int DELTA_FindFieldIndex( struct delta_s *pFields, const char *fieldname )
{
	int						i;
	delta_description_t		*pitem;

	for ( i = 0; i < pFields->fieldCount; i++ )
	{
		pitem = &pFields->pdd[i];
		if ( !Q_strcasecmp( pitem->fieldName, fieldname ) )
			return i;
	}

	Con_Printf( "DELTA_FindFieldIndex:  Warning, couldn't find %s\n", fieldname );
	return -1;
}

/*
==================
DELTA_SetField

==================
*/
void DELTA_SetField( struct delta_s *pFields, const char *fieldname )
{
	delta_description_t		*pTest;

	pTest = DELTA_FindField( pFields, fieldname );
	if ( pTest )
	{
		pTest->flags |= FDT_MARK;
	}
}

/*
==================
DELTA_UnsetField

==================
*/
void DELTA_UnsetField( struct delta_s *pFields, const char *fieldname )
{
	delta_description_t		*pTest;

	pTest = DELTA_FindField( pFields, fieldname );
	if ( pTest )
	{
		pTest->flags &= ~FDT_MARK;
	}
}

/*
==================
DELTA_SetFieldByIndex

==================
*/
void DELTA_SetFieldByIndex( struct delta_s *pFields, int fieldNumber )
{
	pFields->pdd[fieldNumber].flags |= FDT_MARK;
}

/*
==================
DELTA_UnsetFieldByIndex

==================
*/
void DELTA_UnsetFieldByIndex( struct delta_s *pFields, int fieldNumber )
{
	pFields->pdd[fieldNumber].flags &= ~FDT_MARK;
}

/*
==================
DELTA_ClearFlags

==================
*/
void DELTA_ClearFlags( delta_t *pFields )
{
	int						i;
	delta_description_t		*pitem;

	for ( i = 0; i < pFields->fieldCount; i++ )
	{
		pitem = &pFields->pdd[i];
		pitem->flags = 0;
	}
}

/*
==================
DELTA_TestDelta

Returns the number of bits the delta between from and to would need
==================
*/
int DELTA_TestDelta( unsigned char *from, unsigned char *to, delta_t *pFields )
{
	int						i;
	float					f1, f2;
	char					*st1, *st2;
	delta_description_t		*pTest;
	int						fieldType;
	int						fieldCount;
	int						length;
	int						different;
	int						neededBits;
	int						highestBit;

	neededBits = 0;
	highestBit = -1;
	fieldCount = pFields->fieldCount;

	for ( i = 0; i < fieldCount; i++ )
	{
		pTest = &pFields->pdd[i];
		fieldType = pTest->fieldType & ~DT_SIGNED;

		switch ( fieldType )
		{
		case DT_TIMEWINDOW_8:
			f1 = *(float *)&from[pTest->fieldOffset];
			f2 = *(float *)&to[pTest->fieldOffset];
			if ( (int)( f1 * 100.0 ) != (int)( f2 * 100.0 ) )
			{
				neededBits += pTest->significant_bits;
				highestBit = i;
			}
			break;
		case DT_TIMEWINDOW_BIG:
			f1 = *(float *)&from[pTest->fieldOffset];
			f2 = *(float *)&to[pTest->fieldOffset];
			if ( (int)( f1 * 1000.0 ) != (int)( f2 * 1000.0 ) )
			{
				neededBits += pTest->significant_bits;
				highestBit = i;
			}
			break;
		case DT_FLOAT:
		case DT_INTEGER:
		case DT_ANGLE:
			if ( *(int *)&from[pTest->fieldOffset] != *(int *)&to[pTest->fieldOffset] )
			{
				neededBits += pTest->significant_bits;
				highestBit = i;
			}
			break;
		case DT_BYTE:
			if ( from[pTest->fieldOffset] != to[pTest->fieldOffset] )
			{
				neededBits += pTest->significant_bits;
				highestBit = i;
			}
			break;
		case DT_SHORT:
			if ( *(unsigned short *)&from[pTest->fieldOffset] != *(unsigned short *)&to[pTest->fieldOffset] )
			{
				neededBits += pTest->significant_bits;
				highestBit = i;
			}
			break;
		case DT_STRING:
			st1 = (char *)&from[pTest->fieldOffset];
			st2 = (char *)&to[pTest->fieldOffset];

			// Not sure if the strings are the same or not
			length = different = false;
			while ( *st2 )
			{
				if ( !different )
				{
					if ( tolower( *st1 ) != tolower( *st2 ) )
					{
						pTest->flags |= FDT_MARK;
						different = true;
					}
				}

				st1++;
				st2++;
				length += 8;
			}

			// trailing 0
			length += 8;

			if ( different )
				neededBits += length;
			break;
		default:
			Con_Printf( "Bad field type %i\n", fieldType );
			break;
		}
	}

	// the field bits: 3 bits for the byte count, 8 per byte
	if ( highestBit != -1 )
		neededBits += ( highestBit >> 3 ) * 8 + 8;

	return neededBits;
}

/*
==================
DELTA_CountSendFields

==================
*/
int DELTA_CountSendFields( delta_t *pFields )
{
	int						i, c;
	delta_description_t		*pitem;

	c = 0;
	for ( i = 0; i < pFields->fieldCount; i++ )
	{
		pitem = &pFields->pdd[i];
		if ( !( pitem->flags & FDT_MARK ) )
			continue;

		pitem->stats.sendcount++;
		c++;
	}

	return c;
}

/*
==================
DELTA_MarkSendFields

==================
*/
void DELTA_MarkSendFields( unsigned char *from, unsigned char *to, delta_t *pFields )
{
	int						i;
	float					f1, f2;
	char					*st1, *st2;
	delta_description_t		*pTest;
	int						fieldType;
	int						fieldCount;
	int						done;

	fieldCount = pFields->fieldCount;

	for ( i = 0; i < fieldCount; i++ )
	{
		pTest = &pFields->pdd[i];
		fieldType = pTest->fieldType & ~DT_SIGNED;

		switch ( fieldType )
		{
		case DT_TIMEWINDOW_8:
			f1 = *(float *)&from[pTest->fieldOffset];
			f2 = *(float *)&to[pTest->fieldOffset];
			if ( (int)( f1 * 100.0 ) != (int)( f2 * 100.0 ) )
				pTest->flags |= FDT_MARK;
			break;
		case DT_TIMEWINDOW_BIG:
			f1 = *(float *)&from[pTest->fieldOffset];
			f2 = *(float *)&to[pTest->fieldOffset];
			if ( (int)( f1 * 1000.0 ) != (int)( f2 * 1000.0 ) )
				pTest->flags |= FDT_MARK;
			break;
		case DT_FLOAT:
		case DT_INTEGER:
		case DT_ANGLE:
			if ( *(int *)&from[pTest->fieldOffset] != *(int *)&to[pTest->fieldOffset] )
				pTest->flags |= FDT_MARK;
			break;
		case DT_BYTE:
			if ( from[pTest->fieldOffset] != to[pTest->fieldOffset] )
				pTest->flags |= FDT_MARK;
			break;
		case DT_SHORT:
			if ( *(unsigned short *)&from[pTest->fieldOffset] != *(unsigned short *)&to[pTest->fieldOffset] )
				pTest->flags |= FDT_MARK;
			break;
		case DT_STRING:
			st1 = (char *)&from[pTest->fieldOffset];
			st2 = (char *)&to[pTest->fieldOffset];

			// Not sure if the strings are the same or not
			done = false;
			while ( *st1 && *st2 )
			{
				if ( tolower( *st1 ) != tolower( *st2 ) )
				{
					pTest->flags |= FDT_MARK;
					done = true;
					break;
				}
				st1++;
				st2++;
			}

			// one string is longer than the other
			if ( !done && ( *st1 || *st2 ) )
				pTest->flags |= FDT_MARK;
			break;
		default:
			Con_Printf( "Bad field type %i\n", fieldType );
			break;
		}
	}

	if ( pFields->conditionalencode )
		pFields->conditionalencode( pFields, from, to );
}

/*
==================
DELTA_SetSendFlagBits

==================
*/
void DELTA_SetSendFlagBits( delta_t *pFields, int *bits, int *bytecount )
{
	delta_description_t		*pTest;
	int						i;
	int						lastbit;
	int						bytenumber;
	int						bitnumber;
	int						fieldCount;

	Q_memset( bits, 0, 8 );

	fieldCount = pFields->fieldCount;
	lastbit = -1;

	for ( i = fieldCount - 1; i >= 0; i-- )
	{
		pTest = &pFields->pdd[i];
		if ( !( pTest->flags & FDT_MARK ) )
			continue;

		if ( lastbit == -1 )
			lastbit = i;

		bytenumber = i > 31;
		bitnumber = i & 31;

		bits[bytenumber] |= ( 1 << bitnumber );
	}

	// Write how many bytes of bit fields there are
	*bytecount = ( lastbit >> 3 ) + 1;
}

/*
==================
DELTA_WriteMarkedFields

==================
*/
void DELTA_WriteMarkedFields( unsigned char *from, unsigned char *to, delta_t *pFields )
{
	int						i;
	delta_description_t		*pTest;
	int						fieldSign;
	int						fieldType;
	float					f2;
	unsigned char			b2;
	unsigned short			s2;
	unsigned int			i2;
	char					*st2;
	char					sb2;
	short					ss2;
	int						si2;
	int						fieldCount;

	fieldCount = pFields->fieldCount;

	for ( i = 0; i < fieldCount; i++ )
	{
		pTest = &pFields->pdd[i];
		if ( !( pTest->flags & FDT_MARK ) )
			continue;

		fieldType = pTest->fieldType & ~DT_SIGNED;
		fieldSign = pTest->fieldType & DT_SIGNED;

		switch ( fieldType )
		{
		case DT_FLOAT:
			f2 = *(float *)&to[pTest->fieldOffset];
			i2 = (int)( f2 *= pTest->premultiply );
			if ( fieldSign )
				MSG_WriteSBits( i2, pTest->significant_bits );
			else
				MSG_WriteBits( i2, pTest->significant_bits );
			break;
		case DT_ANGLE:
			f2 = *(float *)&to[pTest->fieldOffset];
			MSG_WriteBitAngle( f2, pTest->significant_bits );
			break;
		case DT_TIMEWINDOW_8:
			f2 = *(float *)&to[pTest->fieldOffset] * 100;
			i2 = (int)( sv.time * 100.0 ) - (int)f2;
			MSG_WriteSBits( i2, 8 );
			break;
		case DT_TIMEWINDOW_BIG:
			f2 = *(float *)&to[pTest->fieldOffset];
			i2 = (int)( pTest->premultiply * sv.time );
			i2 -= (int)( f2 *= pTest->premultiply );
			MSG_WriteSBits( i2, pTest->significant_bits );
			break;
		case DT_BYTE:
			if ( fieldSign )
			{
				sb2 = *(char *)&to[pTest->fieldOffset];
				sb2 = (char)( sb2 * pTest->premultiply );
				MSG_WriteSBits( sb2, pTest->significant_bits );
			}
			else
			{
				b2 = to[pTest->fieldOffset];
				b2 = (unsigned char)( b2 * pTest->premultiply );
				MSG_WriteBits( b2, pTest->significant_bits );
			}
			break;
		case DT_SHORT:
			if ( fieldSign )
			{
				ss2 = *(short *)&to[pTest->fieldOffset];
				ss2 = (short)( ss2 * pTest->premultiply );
				MSG_WriteSBits( ss2, pTest->significant_bits );
			}
			else
			{
				s2 = *(unsigned short *)&to[pTest->fieldOffset];
				s2 = (unsigned short)( s2 * pTest->premultiply );
				MSG_WriteBits( s2, pTest->significant_bits );
			}
			break;
		case DT_INTEGER:
			if ( fieldSign )
			{
				si2 = *(int *)&to[pTest->fieldOffset];
				if ( pTest->premultiply < 0.9999f || pTest->premultiply > 1.0001f )
					si2 = (int)( si2 * pTest->premultiply );
				MSG_WriteSBits( si2, pTest->significant_bits );
			}
			else
			{
				i2 = *(unsigned int *)&to[pTest->fieldOffset];
				if ( pTest->premultiply < 0.9999f || pTest->premultiply > 1.0001f )
					i2 = (unsigned int)( i2 * pTest->premultiply );
				MSG_WriteBits( i2, pTest->significant_bits );
			}
			break;
		case DT_STRING:
			st2 = (char *)&to[pTest->fieldOffset];
			while ( st2 && *st2 )
			{
				MSG_WriteBits( *st2, 8 );
				st2++;
			}
			MSG_WriteBits( 0, 8 );
			break;
		default:
			Con_Printf( "unknown send field type\n" );
			break;
		}
	}
}

/*
==================
DELTA_CheckDelta

==================
*/
int DELTA_CheckDelta( unsigned char *from, unsigned char *to, delta_t *pFields )
{
	int		fieldCount;
	int		sendfields;

	sendfields = 0;
	fieldCount = pFields->fieldCount;

	DELTA_ClearFlags( pFields );
	DELTA_MarkSendFields( from, to, pFields );
	sendfields = DELTA_CountSendFields( pFields );

	return sendfields;
}

/*
==================
DELTA_WriteDelta

==================
*/
int DELTA_WriteDelta( unsigned char *from, unsigned char *to, qboolean force, delta_t *pFields, void ( *callback )( void ) )
{
	int		sendfields = 0;

	DELTA_ClearFlags( pFields );
	DELTA_MarkSendFields( from, to, pFields );
	sendfields = DELTA_CountSendFields( pFields );

	return _DELTA_WriteDelta( from, to, force, pFields, callback, sendfields );
}

/*
==================
_DELTA_WriteDelta

==================
*/
int _DELTA_WriteDelta( unsigned char *from, unsigned char *to, qboolean force, delta_t *pFields, void ( *callback )( void ), int sendfields )
{
	int				i;
	int				bytecount;
	int				fieldCount;
	int				bits[2];
	unsigned char	*pdest;

	fieldCount = pFields->fieldCount;

	if ( !sendfields && !force )
		return 1;

	DELTA_SetSendFlagBits( pFields, bits, &bytecount );

	if ( callback )
		callback();

	MSG_WriteBits( bytecount, 3 );

	pdest = (unsigned char *)bits;
	for ( i = 0; i < bytecount; i++ )
	{
		MSG_WriteBits( *pdest++, 8 );
	}

	DELTA_WriteMarkedFields( from, to, pFields );

	return 1;
}

/*
==================
DELTA_ParseDelta

==================
*/
int DELTA_ParseDelta( unsigned char *from, unsigned char *to, delta_t *pFields )
{
	delta_description_t		*pTest;
	int						i;
	int						bits[2];
	int						nbytes;
	int						bitfieldnumber;
	int						bit;
	unsigned char			*pdest;
	int						fieldCount;
	int						fieldType;
	int						fieldSign;
	unsigned char			b2;
	char					sb2;
	unsigned short			s2;
	short					ss2;
	unsigned int			si2;
	int						i2;
	float					f2;
	float					t;
	int						addt;
	char					*st2;
	char					*st1;
	char					c;
	int						startbit;

	startbit = MSG_CurrentBit();

	Q_memset( bits, 0, sizeof( bits ) );

	nbytes = MSG_ReadBits( 3 );

	pdest = (unsigned char *)bits;
	for ( i = 0; i < nbytes; i++ )
	{
		*pdest++ = MSG_ReadBits( 8 );
	}

	fieldCount = pFields->fieldCount;

	for ( i = 0; i < fieldCount; i++ )
	{
		pTest = &pFields->pdd[i];

		bitfieldnumber = i > 31;
		bit = bits[bitfieldnumber] & ( 1 << ( i & 31 ) );

		fieldType = pTest->fieldType & ~DT_SIGNED;

		if ( !bit )
		{
			// not in the message, copy the old value
			switch ( fieldType )
			{
			case DT_FLOAT:
			case DT_ANGLE:
			case DT_TIMEWINDOW_8:
			case DT_TIMEWINDOW_BIG:
				*(float *)&to[pTest->fieldOffset] = *(float *)&from[pTest->fieldOffset];
				break;
			case DT_BYTE:
				to[pTest->fieldOffset] = from[pTest->fieldOffset];
				break;
			case DT_SHORT:
				*(short *)&to[pTest->fieldOffset] = *(short *)&from[pTest->fieldOffset];
				break;
			case DT_INTEGER:
				*(int *)&to[pTest->fieldOffset] = *(int *)&from[pTest->fieldOffset];
				break;
			case DT_STRING:
				st1 = (char *)&from[pTest->fieldOffset];
				st2 = (char *)&to[pTest->fieldOffset];
				do
				{
					*st2++ = *st1++;
				} while ( *st1 );
				break;
			default:
				Con_Printf( "unparseable field type %i\n", fieldType );
				break;
			}
			continue;
		}

		pTest->stats.receivedcount++;

		fieldSign = pTest->fieldType & DT_SIGNED;

		switch ( fieldType )
		{
		case DT_FLOAT:
			if ( fieldSign )
				f2 = (float)MSG_ReadSBits( pTest->significant_bits );
			else
				f2 = (float)MSG_ReadBits( pTest->significant_bits );

			if ( pTest->premultiply < 0.9999f || pTest->premultiply > 1.0001f )
				f2 = f2 / pTest->premultiply;

			if ( pTest->postmultiply < 0.9999f || pTest->postmultiply > 1.0001f )
				f2 = f2 * pTest->postmultiply;

			pdest = &to[pTest->fieldOffset];
			*(float *)pdest = f2;
			break;
		case DT_ANGLE:
			f2 = MSG_ReadBitAngle( pTest->significant_bits );
			pdest = &to[pTest->fieldOffset];
			*(float *)pdest = f2;
			break;
		case DT_TIMEWINDOW_8:
			addt = MSG_ReadSBits( 8 );
			t = (int)( 100.0 * cl.mtime[0] );
			t -= addt;
			t /= 100;
			pdest = &to[pTest->fieldOffset];
			*(float *)pdest = t;
			break;
		case DT_TIMEWINDOW_BIG:
			addt = MSG_ReadSBits( pTest->significant_bits );
			t = pTest->premultiply * cl.mtime[0];
			t -= addt;

			if ( pTest->premultiply < 0.9999f || pTest->premultiply > 1.0001f )
				t = t / pTest->premultiply;

			pdest = &to[pTest->fieldOffset];
			*(float *)pdest = t;
			break;
		case DT_BYTE:
			pdest = &to[pTest->fieldOffset];
			if ( fieldSign )
			{
				sb2 = MSG_ReadSBits( pTest->significant_bits );
				if ( pTest->premultiply < 0.9999f || pTest->premultiply > 1.0001f )
					sb2 = (char)( sb2 / pTest->premultiply );
				if ( pTest->postmultiply < 0.9999f || pTest->postmultiply > 1.0001f )
					sb2 = (char)( sb2 * pTest->postmultiply );
				*(char *)pdest = sb2;
			}
			else
			{
				b2 = MSG_ReadBits( pTest->significant_bits );
				if ( pTest->premultiply < 0.9999f || pTest->premultiply > 1.0001f )
					b2 = (unsigned char)( b2 / pTest->premultiply );
				if ( pTest->postmultiply < 0.9999f || pTest->postmultiply > 1.0001f )
					b2 = (unsigned char)( b2 * pTest->postmultiply );
				*pdest = b2;
			}
			break;
		case DT_SHORT:
			pdest = &to[pTest->fieldOffset];
			if ( fieldSign )
			{
				ss2 = MSG_ReadSBits( pTest->significant_bits );
				if ( pTest->premultiply < 0.9999f || pTest->premultiply > 1.0001f )
					ss2 = (short)( ss2 / pTest->premultiply );
				if ( pTest->postmultiply < 0.9999f || pTest->postmultiply > 1.0001f )
					ss2 = (short)( ss2 * pTest->postmultiply );
				*(short *)pdest = ss2;
			}
			else
			{
				s2 = MSG_ReadBits( pTest->significant_bits );
				if ( pTest->premultiply < 0.9999f || pTest->premultiply > 1.0001f )
					s2 = (unsigned short)( s2 / pTest->premultiply );
				if ( pTest->postmultiply < 0.9999f || pTest->postmultiply > 1.0001f )
					s2 = (unsigned short)( s2 * pTest->postmultiply );
				*(unsigned short *)pdest = s2;
			}
			break;
		case DT_INTEGER:
			pdest = &to[pTest->fieldOffset];
			if ( fieldSign )
			{
				si2 = MSG_ReadSBits( pTest->significant_bits );
				if ( pTest->premultiply < 0.9999f || pTest->premultiply > 1.0001f )
					si2 = (unsigned int)( si2 / pTest->premultiply );
				if ( pTest->postmultiply < 0.9999f || pTest->postmultiply > 1.0001f )
					si2 = (unsigned int)( si2 * pTest->postmultiply );
				*(unsigned int *)pdest = si2;
			}
			else
			{
				i2 = MSG_ReadBits( pTest->significant_bits );
				if ( pTest->premultiply < 0.9999f || pTest->premultiply > 1.0001f )
					i2 = (int)( i2 / pTest->premultiply );
				if ( pTest->postmultiply < 0.9999f || pTest->postmultiply > 1.0001f )
					i2 = (int)( i2 * pTest->postmultiply );
				*(int *)pdest = i2;
			}
			break;
		case DT_STRING:
			st2 = (char *)&to[pTest->fieldOffset];
			do
			{
				c = MSG_ReadBits( 8 );
				*st2++ = c;
			} while ( c );
			break;
		default:
			Con_Printf( "unparseable field type %i\n", fieldType );
			break;
		}
	}

	return MSG_CurrentBit() - startbit;
}

/*
==================
DELTA_AddEncoder

==================
*/
void DELTA_AddEncoder( char *name, encoder_t conditionalencode )
{
	delta_encoder_t		*p;

	p = (delta_encoder_t *)Mem_ZeroMalloc( sizeof( delta_encoder_t ) );
	p->name = Mem_Strdup( name );
	p->conditionalencode = conditionalencode;
	p->next = g_encoders;
	g_encoders = p;
}

/*
==================
DELTA_ClearEncoders

==================
*/
void DELTA_ClearEncoders( void )
{
	delta_encoder_t		*p, *n;

	p = g_encoders;
	while ( p )
	{
		n = p->next;
		Mem_Free( p->name );
		Mem_Free( p );
		p = n;
	}
	g_encoders = NULL;
}

/*
==================
DELTA_LookupEncoder

==================
*/
encoder_t DELTA_LookupEncoder( char *name )
{
	delta_encoder_t		*p;

	p = g_encoders;
	while ( p )
	{
		if ( !Q_stricmp( p->name, name ) )
			return p->conditionalencode;
		p = p->next;
	}

	return NULL;
}

/*
==================
DELTA_CountLinks

==================
*/
int DELTA_CountLinks( delta_link_t *plinks )
{
	delta_link_t	*p;
	int				c;

	c = 0;
	p = plinks;
	while ( p )
	{
		c++;
		p = p->next;
	}

	return c;
}

/*
==================
DELTA_ReverseLinks

==================
*/
void DELTA_ReverseLinks( delta_link_t **plinks )
{
	delta_link_t	*p, *n;
	delta_link_t	*newlist;

	newlist = NULL;

	p = *plinks;
	while ( p )
	{
		n = p->next;
		p->next = newlist;
		newlist = p;
		p = n;
	}

	*plinks = newlist;
}

/*
==================
DELTA_ClearLinks

==================
*/
void DELTA_ClearLinks( delta_link_t **plinks )
{
	delta_link_t	*p, *n;

	p = *plinks;
	while ( p )
	{
		n = p->next;
		Mem_Free( p );
		p = n;
	}

	*plinks = NULL;
}

/*
==================
DELTA_BuildFromLinks

==================
*/
delta_t *DELTA_BuildFromLinks( delta_link_t **pplinks )
{
	delta_description_t		*pdesc = NULL, *pcur;
	delta_t					*pdelta;
	delta_link_t			*p;
	int						count = 0;
	int						i = 0;

	pdelta = (delta_t *)Mem_ZeroMalloc( sizeof( delta_t ) );

	DELTA_ReverseLinks( pplinks );

	count = DELTA_CountLinks( *pplinks );

	pdesc = (delta_description_t *)Mem_ZeroMalloc( count * sizeof( delta_description_t ) );

	p = *pplinks;
	while ( p )
	{
		pcur = &pdesc[i++];

		Q_memcpy( pcur, p->delta, sizeof( delta_description_t ) );
		Mem_Free( p->delta );
		p->delta = NULL;

		p = p->next;
	}

	DELTA_ClearLinks( pplinks );

	pdelta->dynamic = 1;
	pdelta->fieldCount = count;
	pdelta->pdd = pdesc;

	return pdelta;
}

/*
==================
DELTA_FindOffset

==================
*/
int DELTA_FindOffset( int count, delta_definition_t *pdef, char *fieldname )
{
	int		i;

	for ( i = 0; i < count; i++ )
	{
		if ( !Q_stricmp( fieldname, pdef[i].fieldName ) )
			return pdef[i].fieldOffset;
	}

	Sys_Error( "Couldn't find offset for %s!!!\n", fieldname );
	return 0;
}

/*
==================
DELTA_ParseType

==================
*/
qboolean DELTA_ParseType( delta_description_t *pdelta, char **pstream )
{
	// Read until ','
	while ( 1 )
	{
		*pstream = COM_Parse( *pstream );
		if ( Q_strlen( com_token ) <= 0 )
		{
			Con_Printf( "DELTA_ParseField:  Expecting fieldtype info\n" );
			return false;
		}

		if ( !Q_stricmp( com_token, "|" ) )
			continue;

		if ( !Q_stricmp( com_token, "," ) )
			break;

		if ( !Q_stricmp( com_token, "DT_SIGNED" ) )
		{
			pdelta->fieldType |= DT_SIGNED;
			continue;
		}

		if ( !Q_stricmp( com_token, "DT_BYTE" ) )
		{
			pdelta->fieldType |= DT_BYTE;
		}
		else if ( !Q_stricmp( com_token, "DT_SHORT" ) )
		{
			pdelta->fieldType |= DT_SHORT;
		}
		else if ( !Q_stricmp( com_token, "DT_FLOAT" ) )
		{
			pdelta->fieldType |= DT_FLOAT;
		}
		else if ( !Q_stricmp( com_token, "DT_INTEGER" ) )
		{
			pdelta->fieldType |= DT_INTEGER;
		}
		else if ( !Q_stricmp( com_token, "DT_ANGLE" ) )
		{
			pdelta->fieldType |= DT_ANGLE;
		}
		else if ( !Q_stricmp( com_token, "DT_TIMEWINDOW_8" ) )
		{
			pdelta->fieldType |= DT_TIMEWINDOW_8;
		}
		else if ( !Q_stricmp( com_token, "DT_TIMEWINDOW_BIG" ) )
		{
			pdelta->fieldType |= DT_TIMEWINDOW_BIG;
		}
		else if ( !Q_stricmp( com_token, "DT_STRING" ) )
		{
			pdelta->fieldType |= DT_STRING;
		}
		else
		{
			Sys_Error( "DELTA_ParseField:  Unknown type or type flag %s\n", com_token );
			return false;
		}
	}

	return true;
}

/*
==================
DELTA_ParseField

==================
*/
qboolean DELTA_ParseField( int count, delta_definition_t *pdefinition, delta_link_t *pField, char **pstream )
{
	qboolean	readpost;

	readpost = false;

	if ( !Q_stricmp( com_token, "DEFINE_DELTA" ) )
	{
		// no postmultiply
	}
	else if ( !Q_stricmp( com_token, "DEFINE_DELTA_POST" ) )
	{
		readpost = true;
	}
	else
	{
		Sys_Error( "DELTA_ParseField:  Expecting DEFINE_*, got %s\n", com_token );
		return false;
	}

	*pstream = COM_Parse( *pstream );
	if ( Q_stricmp( com_token, "(" ) )
	{
		Sys_Error( "DELTA_ParseField:  Expecting (, got %s\n", com_token );
		return false;
	}

	*pstream = COM_Parse( *pstream );
	if ( Q_strlen( com_token ) <= 0 )
	{
		Sys_Error( "DELTA_ParseField:  Expecting fieldname\n" );
		return false;
	}

	Q_strncpy( pField->delta->fieldName, com_token, sizeof( pField->delta->fieldName ) - 1 );
	pField->delta->fieldName[sizeof( pField->delta->fieldName ) - 1] = 0;

	pField->delta->fieldOffset = DELTA_FindOffset( count, pdefinition, com_token );

	*pstream = COM_Parse( *pstream );
	if ( !DELTA_ParseType( pField->delta, pstream ) )
		return false;

	*pstream = COM_Parse( *pstream );
	pField->delta->fieldSize = 1;
	pField->delta->significant_bits = Q_atoi( com_token );
	*pstream = COM_Parse( *pstream );
	*pstream = COM_Parse( *pstream );
	pField->delta->premultiply = Q_atof( com_token );

	if ( readpost )
	{
		*pstream = COM_Parse( *pstream );
		*pstream = COM_Parse( *pstream );
		pField->delta->postmultiply = atof( com_token );
	}
	else
	{
		pField->delta->postmultiply = 1.0;
	}

	*pstream = COM_Parse( *pstream );
	if ( Q_stricmp( com_token, ")" ) )
	{
		Con_Printf( "DELTA_ParseField:  Expecting ), got %s\n", com_token );
		return false;
	}

	*pstream = COM_Parse( *pstream );
	if ( Q_stricmp( com_token, "," ) )
	{
		COM_UngetToken();
	}

	return true;
}

/*
==================
DELTA_FreeDescription

==================
*/
void DELTA_FreeDescription( delta_t **ppdesc )
{
	if ( !ppdesc )
		return;

	if ( *ppdesc )
	{
		if ( ( *ppdesc )->dynamic )
			Mem_Free( ( *ppdesc )->pdd );

		Mem_Free( *ppdesc );
		*ppdesc = NULL;
	}
}

/*
==================
DELTA_AddDefinition

==================
*/
void DELTA_AddDefinition( char *name, delta_definition_t *pdef, int numelements )
{
	delta_definition_list_t		*p;

	p = g_defs;
	while ( p )
	{
		if ( !Q_stricmp( name, p->ptypename ) )
		{
			p->pdefinition = pdef;
			p->numelements = numelements;
			return;
		}
		p = p->next;
	}

	p = (delta_definition_list_t *)Mem_ZeroMalloc( sizeof( delta_definition_list_t ) );
	p->next = g_defs;
	g_defs = p;
	p->ptypename = Mem_Strdup( name );
	p->pdefinition = pdef;
	p->numelements = numelements;
}

/*
==================
DELTA_ClearDefinitions

==================
*/
void DELTA_ClearDefinitions( void )
{
	delta_definition_list_t		*p, *n;

	p = g_defs;
	while ( p )
	{
		n = p->next;
		Mem_Free( p->ptypename );
		Mem_Free( p );
		p = n;
	}

	g_defs = NULL;
}

/*
==================
DELTA_FindDefinition

==================
*/
delta_definition_t *DELTA_FindDefinition( char *name, int *count )
{
	delta_definition_list_t		*p;

	*count = 0;

	p = g_defs;
	while ( p )
	{
		if ( !Q_stricmp( name, p->ptypename ) )
		{
			*count = p->numelements;
			return p->pdefinition;
		}
		p = p->next;
	}

	return NULL;
}

/*
==================
DELTA_SkipDescription

==================
*/
void DELTA_SkipDescription( char **pstream )
{
	*pstream = COM_Parse( *pstream );

	while ( 1 )
	{
		*pstream = COM_Parse( *pstream );
		if ( Q_strlen( com_token ) <= 0 )
		{
			Sys_Error( "Error during description skip" );
			return;
		}

		if ( !Q_stricmp( com_token, "}" ) )
			break;
	}
}

/*
==================
DELTA_ParseOneField

==================
*/
qboolean DELTA_ParseOneField( char **ppstream, delta_link_t **pplist, int count, delta_definition_t *pdefinition )
{
	delta_link_t			*newlink = NULL;
	delta_link_t			link;
	delta_description_t		*delta;

	while ( 1 )
	{
		if ( !Q_stricmp( com_token, "}" ) )
		{
			COM_UngetToken();
			break;
		}

		*ppstream = COM_Parse( *ppstream );
		if ( Q_strlen( com_token ) <= 0 )
			break;

		Q_memset( &link, 0, sizeof( link ) );
		delta = (delta_description_t *)Mem_ZeroMalloc( sizeof( delta_description_t ) );
		link.delta = delta;

		if ( !DELTA_ParseField( count, pdefinition, &link, ppstream ) )
			return false;

		newlink = (delta_link_t *)Mem_ZeroMalloc( sizeof( delta_link_t ) );
		newlink->delta = link.delta;
		newlink->next = *pplist;
		*pplist = newlink;
	}

	return true;
}

/*
==================
DELTA_ParseDescription

==================
*/
qboolean DELTA_ParseDescription( char *name, delta_t **ppdesc, char *pstream )
{
	char					*p;
	delta_link_t			*links;
	delta_definition_t		*pdefinition;
	char					encoder[32];
	char					source[32];
	int						count;

	links = NULL;
	pdefinition = NULL;
	count = 0;
	encoder[0] = 0;

	if ( !ppdesc )
	{
		Sys_Error( "DELTA_ParseDescription with no delta_description_t\n" );
		return false;
	}

	*ppdesc = NULL;

	if ( !pstream )
	{
		Sys_Error( "DELTA_ParseDescription with no data stream\n" );
		return false;
	}

	p = pstream;

	while ( 1 )
	{
		// Parse delta name
		p = COM_Parse( p );
		if ( Q_strlen( com_token ) <= 0 )
			break;

		if ( Q_stricmp( com_token, name ) )
		{
			DELTA_SkipDescription( &p );
			continue;
		}

		pdefinition = DELTA_FindDefinition( com_token, &count );
		if ( !pdefinition )
		{
			Sys_Error( "DELTA_ParseDescription:  Unknown data type:  %s\n", com_token );
			return false;
		}

		// Parse source of conditional encoder
		p = COM_Parse( p );
		if ( Q_strlen( com_token ) <= 0 )
		{
			Sys_Error( "DELTA_ParseDescription:  Unknown encoder :  %s\nValid values:\nnone\ngamedll funcname\nclientdll funcname\n", com_token );
			return false;
		}

		// Parse name of conditional encoder
		if ( Q_stricmp( com_token, "none" ) )
		{
			Q_strncpy( source, com_token, sizeof( source ) - 1 );
			source[sizeof( source ) - 1] = 0;

			p = COM_Parse( p );
			if ( Q_strlen( com_token ) <= 0 )
			{
				Sys_Error( "DELTA_ParseDescription:  Expecting encoder\n" );
				return false;
			}

			Q_strncpy( encoder, com_token, sizeof( encoder ) - 1 );
			encoder[sizeof( encoder ) - 1] = 0;
		}

		// Parse the fields
		while ( 1 )
		{
			p = COM_Parse( p );
			if ( Q_strlen( com_token ) <= 0 )
				break;

			if ( !Q_stricmp( com_token, "}" ) )
				break;

			if ( Q_stricmp( com_token, "{" ) )
			{
				Con_Printf( "DELTA_ParseDescription:  Expecting {, got %s\n", com_token );
				return false;
			}

			if ( !DELTA_ParseOneField( &p, &links, count, pdefinition ) )
				return false;
		}
	}

	*ppdesc = DELTA_BuildFromLinks( &links );

	if ( Q_strlen( encoder ) > 0 )
	{
		Q_strncpy( ( *ppdesc )->conditionalencodename, encoder, sizeof( ( *ppdesc )->conditionalencodename ) - 1 );
		( *ppdesc )->conditionalencodename[sizeof( ( *ppdesc )->conditionalencodename ) - 1] = 0;
		( *ppdesc )->conditionalencode = NULL;
	}

	return true;
}

/*
==================
DELTA_Load

==================
*/
qboolean DELTA_Load( char *name, delta_t **ppdesc, char *pszFile )
{
	char		*pbuf = NULL;
	qboolean	bret = false;

	pbuf = (char *)COM_LoadFile( pszFile, 5, NULL );
	if ( !pbuf )
	{
		Sys_Error( "DELTA_Load:  Couldn't load file %s\n", pszFile );
		return false;
	}

	bret = DELTA_ParseDescription( name, ppdesc, pbuf );

	if ( pbuf )
		Mem_Free( pbuf );

	return bret;
}

/*
==================
DELTA_RegisterDescription

==================
*/
void DELTA_RegisterDescription( char *name )
{
	delta_registry_t	*p;

	p = (delta_registry_t *)Mem_ZeroMalloc( sizeof( delta_registry_t ) );
	p->next = g_deltaregistry;
	g_deltaregistry = p;
	p->name = Mem_Strdup( name );
	p->pdesc = NULL;
}

/*
==================
DELTA_ClearRegistrations

==================
*/
void DELTA_ClearRegistrations( void )
{
	delta_registry_t	*p, *n;

	p = g_deltaregistry;
	while ( p )
	{
		n = p->next;
		Mem_Free( p->name );
		if ( p->pdesc )
			DELTA_FreeDescription( &p->pdesc );
		Mem_Free( p );
		p = n;
	}

	g_deltaregistry = NULL;
}

/*
==================
DELTA_LookupRegistration

==================
*/
delta_t **DELTA_LookupRegistration( char *name )
{
	delta_registry_t	*p;

	p = g_deltaregistry;
	while ( p )
	{
		if ( !Q_stricmp( p->name, name ) )
			return &p->pdesc;
		p = p->next;
	}

	return NULL;
}

/*
==================
DELTA_ClearStats

==================
*/
void DELTA_ClearStats( delta_t *p )
{
	int		i;

	if ( !p )
		return;

	for ( i = 0; i < p->fieldCount; i++ )
	{
		p->pdd[i].stats.sendcount = 0;
		p->pdd[i].stats.receivedcount = 0;
	}
}

/*
==================
DELTA_ClearStats_f

==================
*/
void DELTA_ClearStats_f( void )
{
	delta_registry_t	*p;

	Con_Printf( "Clearing delta stats\n" );

	p = g_deltaregistry;
	while ( p )
	{
		DELTA_ClearStats( p->pdesc );
		p = p->next;
	}
}

/*
==================
DELTA_PrintStats

==================
*/
void DELTA_PrintStats( char *name, delta_t *p )
{
	int		i;

	if ( !p )
		return;

	Con_Printf( "Stats for '%s'\n", name );

	for ( i = 0; i < p->fieldCount; i++ )
	{
		Con_Printf( "  %02i % 10s:  s % 5i r % 5i\n", i + 1, p->pdd[i].fieldName, p->pdd[i].stats.sendcount, p->pdd[i].stats.receivedcount );
	}

	Con_Printf( "\n" );
}

/*
==================
DELTA_DumpStats_f

==================
*/
void DELTA_DumpStats_f( void )
{
	delta_registry_t	*p;

	Con_Printf( "Delta Stats\n" );

	p = g_deltaregistry;
	while ( p )
	{
		DELTA_PrintStats( p->name, p->pdesc );
		p = p->next;
	}
}

/*
==================
DELTA_Init

==================
*/
void DELTA_Init( void )
{
	Cmd_AddCommand( "delta_stats", DELTA_DumpStats_f );
	Cmd_AddCommand( "delta_clear", DELTA_ClearStats_f );

	DELTA_AddDefinition( "clientdata_t", g_ClientDataDefinition, ARRAYSIZE( g_ClientDataDefinition ) );
	DELTA_AddDefinition( "weapon_data_t", g_WeaponDataDefinition, ARRAYSIZE( g_WeaponDataDefinition ) );
	DELTA_AddDefinition( "usercmd_t", g_UsercmdDataDefinition, ARRAYSIZE( g_UsercmdDataDefinition ) );
	DELTA_AddDefinition( "entity_state_t", g_EntityDataDefinition, ARRAYSIZE( g_EntityDataDefinition ) );
	DELTA_AddDefinition( "entity_state_player_t", g_EntityDataDefinition, ARRAYSIZE( g_EntityDataDefinition ) );
	DELTA_AddDefinition( "custom_entity_state_t", g_EntityDataDefinition, ARRAYSIZE( g_EntityDataDefinition ) );
	DELTA_AddDefinition( "event_t", g_EventDataDefinition, ARRAYSIZE( g_EventDataDefinition ) );
}

/*
==================
DELTA_Shutdown

==================
*/
void DELTA_Shutdown( void )
{
	DELTA_ClearEncoders();
	DELTA_ClearDefinitions();
	DELTA_ClearRegistrations();
}
