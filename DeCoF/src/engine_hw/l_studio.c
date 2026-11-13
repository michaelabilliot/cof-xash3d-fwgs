// l_studio.c -- studio model loading

#include "quakedef.h"
#include "mathlib.h"
#include "cvar.h"
#include "zone.h"
#include "GL_MODEL.H"
#include "client.h"
#include "vid.h"
#include "render.h"
#include "GLQUAKE.H"
#include "studio.h"

#define STUDIO_VERSION	10

int		GL_LoadTexture( char *identifier, GL_TEXTURETYPE textureType, int width, int height, byte *data, int mipmap, int iType, byte *pPal );

int		giTextureSize;

/*
=================
Mod_LoadStudioModel
=================
*/
void Mod_LoadStudioModel( model_t *mod, void *buffer )
{
	int					i;
	byte				*pin, *pout;
	studiohdr_t			*phdr;
	mstudiotexture_t	*ptexture;
	int					total;
	int					version;

	pin = (byte *)buffer;

	phdr = (studiohdr_t *)pin;

	version = LittleLong( phdr->version );
	if ( version != STUDIO_VERSION )
	{
		Q_memset( phdr, 0, sizeof( studiohdr_t ) );
		Q_strcpy( phdr->name, "bogus" );
		phdr->length = sizeof( studiohdr_t );
		phdr->texturedataindex = phdr->length;
	}

	mod->type = mod_studio;
	mod->flags = phdr->flags;

	// keep the texture data out of the cache, it goes to the GL
	total = phdr->length;
	if ( phdr->textureindex )
		total -= ( phdr->length - phdr->texturedataindex );

	Cache_Alloc( &mod->cache, total, mod->name );
	if ( !mod->cache.data )
		return;

	pout = (byte *)mod->cache.data;

	ptexture = (mstudiotexture_t *)( pin + phdr->textureindex );
	if ( phdr->textureindex )
	{
		for ( i = 0; i < phdr->numtextures; i++ )
		{
			char		name[256];
			qboolean	mipmap;
			byte		*pal;

			mipmap = false;

			pal = pin + ptexture[i].width * ptexture[i].height + ptexture[i].index;

			_snprintf( name, sizeof( name ), "%s%s", mod->name, ptexture[i].name );

			if ( ptexture[i].flags & STUDIO_NF_NOMIPS )
				mipmap = true;

			if ( ptexture[i].flags & STUDIO_NF_MASKED )
				ptexture[i].index = GL_LoadTexture( name, GLT_STUDIO, ptexture[i].width, ptexture[i].height, pin + ptexture[i].index, mipmap, 1, pal );
			else
				ptexture[i].index = GL_LoadTexture( name, GLT_STUDIO, ptexture[i].width, ptexture[i].height, pin + ptexture[i].index, mipmap, 0, pal );
		}
	}

	Q_memcpy( pout, pin, total );
}
