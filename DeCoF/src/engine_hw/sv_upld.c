// sv_upld.c -- upload of player customizations (custom decals) to the server

#include "quakedef.h"
#include "cvar.h"
#include "mem.h"
#include "delta.h"
#include "server.h"
#include "eiface.h"
#include "hashpak.h"
#include "protocol.h"

cvar_t	sv_uploadinterval = { "sv_uploadinterval", "1.0" };

void	SV_ClearResourceList( resource_t *pList );
void	SV_AddToResourceList( resource_t *pResource, resource_t *pList );
void	SV_RemoveFromResourceList( resource_t *pResource );
qboolean	SV_RequestMissingResources( void );

/*
==================
SV_CheckFile

Returns true if the file is already in the custom pack, otherwise asks the
client to upload it
==================
*/
qboolean SV_CheckFile( sizebuf_t *msg, char *filename )
{
	resource_t	p;

	Q_memset( &p, 0, sizeof( resource_t ) );

	if ( Q_strlen( filename ) == 36 && !Q_strnicmp( filename, "!MD5", 4 ) )
	{
		// MD5 signature is correct length; convert to binary
		COM_HexConvert( filename + 4, 32, p.rgucMD5_hash );

		// Check our cache
		if ( HPAK_GetDataPointer( "custom.hpk", &p, NULL, NULL ) )
			return true;
	}

	if ( !sv_allow_upload.value )
		return true;

	MSG_WriteByte( msg, svc_stufftext );
	MSG_WriteString( msg, va( "upload \"!MD5%s\"\n", MD5_Print( p.rgucMD5_hash ) ) );
	return false;
}

/*
==================
SV_ClearResourceLists

==================
*/
void SV_ClearResourceLists( client_t *cl )
{
	if ( !cl )
		Sys_Error( "SV_ClearResourceLists with NULL client!" );

	SV_ClearResourceList( &cl->resourcesneeded );
	SV_ClearResourceList( &cl->resourcesonhand );
}

/*
==================
SV_CreateCustomizationList

==================
*/
void SV_CreateCustomizationList( client_t *pHost )
{
	resource_t	*pResource;

	pHost->customdata.pNext = NULL;

	pResource = pHost->resourcesonhand.pNext;
	while ( pResource != &pHost->resourcesonhand )
	{
		customization_t	*pList;
		qboolean		bFound;

		// Search for this resource in the customization list
		bFound = false;
		pList = pHost->customdata.pNext;
		while ( pList )
		{
			if ( !Q_memcmp( pList->resource.rgucMD5_hash, pResource->rgucMD5_hash, 16 ) )
			{
				bFound = true;
				break;
			}

			pList = pList->pNext;
		}

		if ( !bFound )
		{
			customization_t	*pCust;
			qboolean		bNoError;
			int				nLumps;

			// Create it
			nLumps = 0;

			bNoError = COM_CreateCustomization( &pHost->customdata, pResource, -1, ( FCUST_FROMHPAK | FCUST_WIPEDATA ), &pCust, &nLumps );
			if ( bNoError )
			{
				pCust->nUserData2 = nLumps;
				gEntityInterface.pfnPlayerCustomization( pHost->edict, pCust );
			}
			else
			{
				if ( sv_allow_upload.value )
					Con_Printf( "Ignoring invalid custom decal from %s\n", pHost->name );
				else
					Con_Printf( "Ignoring custom decal from %s\n", pHost->name );
			}
		}
		else
		{
			Con_DPrintf( "SV_CreateCustomization list, ignoring dup. resource for player %s\n", pHost->name );
		}

		pResource = pResource->pNext;
	}
}

/*
==================
SV_Customization

Sends a player's customization to the other clients
==================
*/
void SV_Customization( client_t *pPlayer, resource_t *pResource, qboolean bSkipPlayer )
{
	int		i;
	int		nPlayerNumber;

	nPlayerNumber = -1;

	// Get originating player id
	for ( i = 0, host_client = svs.clients; i < svs.maxclients; i++, host_client++ )
	{
		if ( host_client != pPlayer )
			continue;

		nPlayerNumber = i;
		break;
	}

	if ( nPlayerNumber == -1 )
	{
		Sys_Error( "Couldn't find player index for customization." );
		return;
	}

	// Send resource to all other active players
	for ( i = 0, host_client = svs.clients; i < svs.maxclients; i++, host_client++ )
	{
		if ( ( !host_client->active && !host_client->spawned ) || host_client->fakeclient )
			continue;

		if ( host_client == pPlayer && bSkipPlayer )
			continue;

		MSG_WriteByte( &host_client->netchan.message, svc_customization );
		MSG_WriteByte( &host_client->netchan.message, nPlayerNumber );
		MSG_WriteByte( &host_client->netchan.message, pResource->type );
		MSG_WriteString( &host_client->netchan.message, pResource->szFileName );
		MSG_WriteShort( &host_client->netchan.message, pResource->nIndex );
		MSG_WriteLong( &host_client->netchan.message, pResource->nDownloadSize );
		MSG_WriteByte( &host_client->netchan.message, pResource->ucFlags );

		if ( pResource->ucFlags & RES_CUSTOM )
			SZ_Write( &host_client->netchan.message, pResource->rgucMD5_hash, 16 );
	}
}

/*
==================
SV_RegisterResources

==================
*/
void SV_RegisterResources( void )
{
	resource_t	*pResource;
	client_t	*pHost;

	host_client->uploading = false;
	pHost = host_client;

	pResource = pHost->resourcesonhand.pNext;
	while ( pResource != &pHost->resourcesonhand )
	{
		SV_CreateCustomizationList( pHost );
		SV_Customization( pHost, pResource, true );

		pResource = pResource->pNext;
	}

	host_client = pHost;
}

/*
==================
SV_MoveToOnHandList

==================
*/
void SV_MoveToOnHandList( resource_t *pResource )
{
	if ( !pResource )
	{
		Con_DPrintf( "Null resource passed to SV_MoveToOnHandList\n" );
		return;
	}

	SV_RemoveFromResourceList( pResource );
	SV_AddToResourceList( pResource, &host_client->resourcesonhand );
}

/*
==================
SV_AddToResourceList

==================
*/
void SV_AddToResourceList( resource_t *pResource, resource_t *pList )
{
	if ( pResource->pPrev || pResource->pNext )
	{
		Con_Printf( "Resource already linked\n" );
		return;
	}

	pResource->pPrev = pList->pPrev;
	pList->pPrev->pNext = pResource;
	pList->pPrev = pResource;
	pResource->pNext = pList;
}

/*
==================
SV_ClearResourceList

==================
*/
void SV_ClearResourceList( resource_t *pList )
{
	resource_t	*p, *n;

	p = pList->pNext;
	while ( p && p != pList )
	{
		n = p->pNext;

		SV_RemoveFromResourceList( p );
		Mem_Free( p );

		p = n;
	}

	pList->pPrev = pList;
	pList->pNext = pList;
}

/*
==================
SV_RemoveFromResourceList

==================
*/
void SV_RemoveFromResourceList( resource_t *pResource )
{
	pResource->pPrev->pNext = pResource->pNext;
	pResource->pNext->pPrev = pResource->pPrev;
	pResource->pPrev = NULL;
	pResource->pNext = NULL;
}

/*
==================
SV_EstimateNeededResources

Marks the custom decals that are not in the custom pack yet and returns the
number of bytes to upload
==================
*/
int SV_EstimateNeededResources( void )
{
	resource_t	*p;
	int			missing_guess;	// the Linux build calls these missing and size; those
	int			size_guess;		// names do not give the retail stack slots

	size_guess = 0;

	for ( p = host_client->resourcesneeded.pNext; p != &host_client->resourcesneeded; p = p->pNext )
	{
		missing_guess = 0;

		switch ( p->type )
		{
		case t_decal:
			missing_guess = HPAK_ResourceForHash( "custom.hpk", p->rgucMD5_hash, NULL ) ? 0 : p->nDownloadSize;
			break;
		}

		if ( missing_guess )
		{
			p->ucFlags |= RES_WASMISSING;
			size_guess += missing_guess;
		}
	}

	return size_guess;
}

/*
==================
SV_RequestMissingResourcesFromClients

==================
*/
void SV_RequestMissingResourcesFromClients( void )
{
	int		i;

	for ( i = 0, host_client = svs.clients; i < svs.maxclients; i++, host_client++ )
	{
		if ( !host_client->active && !host_client->spawned )
			continue;

		while ( SV_RequestMissingResources() )
			;
	}
}

/*
==================
SV_UploadComplete

==================
*/
qboolean SV_UploadComplete( client_t *cl )
{
	if ( cl->resourcesneeded.pNext == &cl->resourcesneeded )
	{
		SV_RegisterResources();
		SV_PropagateCustomizations();

		if ( sv_allow_upload.value )
			Con_DPrintf( "Custom resource propagation complete.\n" );

		cl->uploaddoneregistering = true;
		return true;
	}

	return false;
}

/*
==================
SV_BatchUploadRequest

==================
*/
void SV_BatchUploadRequest( client_t *cl )
{
	resource_t	*p, *n;
	char		filename[MAX_PATH];

	for ( p = cl->resourcesneeded.pNext; p != &cl->resourcesneeded; p = n )
	{
		n = p->pNext;

		if ( !( p->ucFlags & RES_WASMISSING ) )
		{
			SV_MoveToOnHandList( p );
			continue;
		}

		if ( p->type != t_decal )
			continue;

		if ( !( p->ucFlags & RES_CUSTOM ) )
		{
			Con_Printf( "Non customization in upload queue!\n" );
			SV_MoveToOnHandList( p );
			continue;
		}

		_snprintf( filename, sizeof( filename ), "!MD5%s", MD5_Print( p->rgucMD5_hash ) );

		if ( SV_CheckFile( &cl->netchan.message, filename ) )
			SV_MoveToOnHandList( p );
	}
}

/*
==================
SV_RequestMissingResources

==================
*/
qboolean SV_RequestMissingResources( void )
{
	if ( !host_client->uploading )
		return false;

	if ( host_client->uploaddoneregistering )
		return false;

	if ( SV_UploadComplete( host_client ) )
		return false;

	return false;
}

/*
==================
SV_ParseResourceList

==================
*/
void SV_ParseResourceList( client_t *pSenderClient )
{
	int				i, total;
	int				totalsize;
	resource_t		*resource;
	resourceinfo_t	ri;

	total = MSG_ReadShort();

	SV_ClearResourceList( &host_client->resourcesneeded );
	SV_ClearResourceList( &host_client->resourcesonhand );

	for ( i = 0; i < total; i++ )
	{
		resource = (resource_t *)Mem_ZeroMalloc( sizeof( resource_t ) );

		Q_strncpy( resource->szFileName, MSG_ReadString(), sizeof( resource->szFileName ) - 1 );
		resource->szFileName[sizeof( resource->szFileName ) - 1] = 0;
		resource->type = MSG_ReadByte();
		resource->nIndex = MSG_ReadShort();
		resource->nDownloadSize = MSG_ReadLong();
		resource->ucFlags = MSG_ReadByte();
		resource->pNext = NULL;
		resource->pPrev = NULL;
		resource->ucFlags &= ~RES_WASMISSING;

		if ( resource->ucFlags & RES_CUSTOM )
			MSG_ReadBuf( 16, resource->rgucMD5_hash );

		if ( msg_badread )
		{
			SV_ClearResourceList( &host_client->resourcesneeded );
			SV_ClearResourceList( &host_client->resourcesonhand );
			return;
		}

		if ( resource->type > t_world || resource->nDownloadSize > 1024 * 1024 * 1024 )
		{
			SV_ClearResourceList( &host_client->resourcesneeded );
			SV_ClearResourceList( &host_client->resourcesonhand );
			return;
		}

		SV_AddToResourceList( resource, &host_client->resourcesneeded );
	}

	if ( sv_allow_upload.value )
		Con_DPrintf( "Verifying and uploading resources...\n" );

	totalsize = COM_SizeofResourceList( &host_client->resourcesneeded, &ri );

	if ( totalsize && sv_allow_upload.value )
	{
		int		bytestodownload;

		bytestodownload = 0;

		Con_DPrintf( "Custom resources total %.2fK\n", totalsize / 1024.0 );

		if ( ri.info[t_model].size )
			Con_DPrintf( "  Models:  %.2fK\n", ri.info[t_model].size / 1024.0 );
		if ( ri.info[t_sound].size )
			Con_DPrintf( "  Sounds:  %.2fK\n", ri.info[t_sound].size / 1024.0 );
		if ( ri.info[t_decal].size )
			Con_DPrintf( "  Decals:  %.2fK\n", ri.info[t_decal].size / 1024.0 );
		if ( ri.info[t_skin].size )
			Con_DPrintf( "  Skins :  %.2fK\n", ri.info[t_skin].size / 1024.0 );
		if ( ri.info[t_generic].size )
			Con_DPrintf( "  Generic :  %.2fK\n", ri.info[t_generic].size / 1024.0 );
		if ( ri.info[t_eventscript].size )
			Con_DPrintf( "  Events  :  %.2fK\n", ri.info[t_eventscript].size / 1024.0 );

		Con_DPrintf( "----------------------\n" );

		bytestodownload = SV_EstimateNeededResources();

		if ( bytestodownload > 1024 * 1024 * sv_max_upload.value )
		{
			SV_ClearResourceList( &host_client->resourcesneeded );
			SV_ClearResourceList( &host_client->resourcesonhand );
			return;
		}

		if ( bytestodownload <= 1024 )
			Con_DPrintf( "Resources to request: %i bytes\n", bytestodownload );
		else
			Con_DPrintf( "Resources to request: %.2fK\n", bytestodownload / 1024.0f );
	}

	host_client->uploading = true;
	host_client->uploaddoneregistering = false;

	SV_BatchUploadRequest( host_client );
}
