// com_custom.c -- player customizations (custom decals)

#include "quakedef.h"
#include "mathlib.h"
#include "cvar.h"
#include "mem.h"
#include "zone.h"
#include "custom.h"
#include "client.h"
#include "GL_MODEL.H"
#include "draw.h"
#include "hashpak.h"

void	R_DecalRemoveAll( int textureIndex );

/*
=================
COM_ClearCustomizationList
=================
*/
void COM_ClearCustomizationList( customization_t *pHead, qboolean bCleanDecals )
{
	customization_t	*pNext, *pCurrent;
	int				i;

	pCurrent = pHead->pNext;
	while ( pCurrent )
	{
		pNext = pCurrent->pNext;

		if ( pCurrent->bInUse && pCurrent->pBuffer )
			Mem_Free( pCurrent->pBuffer );

		if ( pCurrent->bInUse && pCurrent->pInfo )
		{
			if ( pCurrent->resource.type == t_decal )
			{
				cachewad_t		*pWad;
				cacheentry_t	*pic;

				if ( bCleanDecals && cls.state == ca_active )
					R_DecalRemoveAll( -( pCurrent->resource.playernum + 1 ) );

				pWad = (cachewad_t *)pCurrent->pInfo;

				Mem_Free( pWad->lumps );

				for ( pic = pWad->cache, i = 0; i < pWad->cacheCount; i++, pic++ )
				{
					if ( Cache_Check( &pic->cache ) )
						Cache_Free( &pic->cache );
				}

				Mem_Free( pWad->name );
				Mem_Free( pWad->cache );
			}

			Mem_Free( pCurrent->pInfo );
		}

		Mem_Free( pCurrent );
		pCurrent = pNext;
	}

	pHead->pNext = NULL;
}

/*
=================
COM_CreateCustomization
=================
*/
qboolean COM_CreateCustomization( customization_t *pListHead, resource_t *pResource, int playernumber, int flags, customization_t **pCustomization, int *nLumps )
{
	customization_t	*pCust;
	qboolean		bError;

	bError = false;

	if ( pCustomization )
		*pCustomization = NULL;

	pCust = (customization_t *)Mem_ZeroMalloc( sizeof( customization_t ) );
	pCust->resource = *pResource;

	if ( pResource->nDownloadSize > 0 )
	{
		pCust->bInUse = true;

		if ( !( flags & FCUST_FROMHPAK ) )
		{
			pCust->pBuffer = COM_LoadFile( pResource->szFileName, 5, NULL );
		}
		else
		{
			if ( !HPAK_GetDataPointer( "custom.hpk", pResource, (unsigned char **)&pCust->pBuffer, NULL ) )
			{
				bError = true;
				goto CustomizationError;
			}
		}

		if ( ( pCust->resource.ucFlags & RES_CUSTOM ) && pCust->resource.type == t_decal )
		{
			void		*pNewBuffer;
			cachewad_t	*pWad;

			pNewBuffer = NULL;

			pCust->resource.playernum = playernumber;

			if ( !CustomDecal_Validate( pCust->pBuffer, pResource->nDownloadSize ) )
			{
				bError = true;
				goto CustomizationError;
			}

			if ( !( flags & FCUST_IGNOREINIT ) )
			{
				pCust->pInfo = Mem_ZeroMalloc( sizeof( cachewad_t ) );
				pWad = (cachewad_t *)pCust->pInfo;

				if ( pResource->nDownloadSize < 1024 || pResource->nDownloadSize > 20480 )
				{
					bError = true;
					goto CustomizationError;
				}

				__try
				{
					bError = !CustomDecal_Init( pWad, pCust->pBuffer, pResource->nDownloadSize, playernumber );
				}
				__except ( EXCEPTION_EXECUTE_HANDLER )
				{
					bError = true;
					goto CustomizationError;
				}

				if ( bError )
					goto CustomizationError;

				if ( pWad->lumpCount <= 0 )
				{
					bError = true;
					goto CustomizationError;
				}

				if ( nLumps )
					*nLumps = pWad->lumpCount;

				pCust->bTranslated = true;
				pCust->nUserData1 = 0;
				pCust->nUserData2 = pWad->lumpCount;

				if ( flags & FCUST_WIPEDATA )
				{
					Mem_Free( pWad->name );
					Mem_Free( pWad->cache );
					Mem_Free( pWad->lumps );
					Mem_Free( pCust->pInfo );
					pCust->pInfo = NULL;
				}
			}
		}
	}
	else
	{
		bError = true;
	}

CustomizationError:
	if ( !bError )
	{
		if ( pCustomization )
			*pCustomization = pCust;

		pCust->pNext = pListHead->pNext;
		pListHead->pNext = pCust;
	}
	else
	{
		if ( pCust->pBuffer )
			Mem_Free( pCust->pBuffer );

		if ( pCust->pInfo )
			Mem_Free( pCust->pInfo );

		Mem_Free( pCust );
	}

	return !bError;
}

/*
=================
COM_SizeofResourceList
=================
*/
int COM_SizeofResourceList( resource_t *pList, resourceinfo_t *ri )
{
	resource_t	*p;
	int			nSize;

	nSize = 0;

	Q_memset( ri, 0, sizeof( *ri ) );

	p = pList->pNext;
	while ( p != pList )
	{
		nSize += p->nDownloadSize;

		if ( p->type == t_model && p->nIndex == 1 )
		{
			ri->info[t_world].size += p->nDownloadSize;
		}
		else if ( p->type < ARRAYSIZE( ri->info ) )
		{
			ri->info[p->type].size += p->nDownloadSize;
		}

		p = p->pNext;
	}

	return nSize;
}
