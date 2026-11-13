//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: an IImage over a block of RGBA memory (the HTML window's page)
//
// $NoKeywords: $
//=============================================================================

#include <wchar.h>

#include "MemoryBitmap.h"

#include <vgui/ISurface.h>

using namespace vgui2;

namespace vgui2
{
extern ISurface *g_pSurface;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
// Input  : *texture - RGBA data, wide x tall pixels
//-----------------------------------------------------------------------------
MemoryBitmap::MemoryBitmap(unsigned char *texture, int wide, int tall)
{
	_texture_guess = texture;
	_id_guess = 0;
	_uploaded_guess = false;
	_color_guess = Color(255, 255, 255, 255);
	_pos_guess[0] = _pos_guess[1] = 0;
	_valid_guess = true;
	_w_guess = wide;
	_h_guess = tall;
	ForceUpload_guess(texture, wide, tall);
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
MemoryBitmap::~MemoryBitmap()
{
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void MemoryBitmap::GetSize(int &wide, int &tall)
{
	wide = 0;
	tall = 0;

	if (!_valid_guess)
		return;

	g_pSurface->DrawGetTextureSize(_id_guess, wide, tall);
}

//-----------------------------------------------------------------------------
// Purpose: size of the bitmap
//-----------------------------------------------------------------------------
void MemoryBitmap::GetContentSize(int &wide, int &tall)
{
	GetSize(wide, tall);
}

//-----------------------------------------------------------------------------
// Purpose: ignored
//-----------------------------------------------------------------------------
void MemoryBitmap::SetSize(int x, int y)
{
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void MemoryBitmap::SetPos(int x, int y)
{
	_pos_guess[0] = x;
	_pos_guess[1] = y;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void MemoryBitmap::SetColor(Color col)
{
	_color_guess = col;
}

//-----------------------------------------------------------------------------
// Purpose: returns the name of the bitmap
//-----------------------------------------------------------------------------
const char *MemoryBitmap::GetName_guess()
{
	return "MemoryBitmap";
}

//-----------------------------------------------------------------------------
// Purpose: Renders the image, uploading it if necessary
//-----------------------------------------------------------------------------
void MemoryBitmap::Paint()
{
	if (!_valid_guess)
		return;

	if (!_id_guess)
	{
		_id_guess = g_pSurface->CreateNewTextureID(false);
	}

	// if we have not uploaded yet, lets go ahead and do so
	if (!_uploaded_guess)
	{
		ForceUpload_guess(_texture_guess, _w_guess, _h_guess);
	}

	// set the texture current, set the color, and draw it
	g_pSurface->DrawSetTexture(_id_guess);
	g_pSurface->DrawSetColor(_color_guess[0], _color_guess[1], _color_guess[2], _color_guess[3]);

	int wide, tall;
	GetSize(wide, tall);
	g_pSurface->DrawTexturedRect(_pos_guess[0], _pos_guess[1], _pos_guess[0] + wide, _pos_guess[1] + tall);
}

//-----------------------------------------------------------------------------
// Purpose: uploads the memory into the texture
//-----------------------------------------------------------------------------
void MemoryBitmap::ForceUpload_guess(unsigned char *texture, int wide, int tall)
{
	_texture_guess = texture;
	_w_guess = wide;
	_h_guess = tall;

	if (!_valid_guess)
		return;

	if (_w_guess == 0 || _h_guess == 0)
		return;

	if (!_id_guess)
	{
		_id_guess = g_pSurface->CreateNewTextureID(false);
	}

	g_pSurface->DrawSetTextureRGBA(_id_guess, _texture_guess, _w_guess, _h_guess, false, true);
	_uploaded_guess = true;

	_valid_guess = g_pSurface->IsTextureIDValid(_id_guess);
}

//-----------------------------------------------------------------------------
// Purpose: returns the texture id
//-----------------------------------------------------------------------------
int MemoryBitmap::GetID_guess()
{
	return _id_guess;
}
