//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: an IImage over a block of RGBA memory (the HTML window's page)
//
// $NoKeywords: $
//=============================================================================

#ifndef MEMORYBITMAP_H
#define MEMORYBITMAP_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui/IImage.h>
#include <Color.h>

namespace vgui2
{

//-----------------------------------------------------------------------------
// Purpose: holds a single image created from a chunk of memory
//-----------------------------------------------------------------------------
class MemoryBitmap : public IImage
{
public:
	MemoryBitmap(unsigned char *texture, int wide, int tall);
	~MemoryBitmap();

	// IImage implementation
	virtual void Paint();
	virtual void GetSize(int &wide, int &tall);
	virtual void GetContentSize(int &wide, int &tall);
	virtual void SetPos(int x, int y);
	virtual void SetSize(int x, int y);
	virtual void SetColor(Color col);

	const char *GetName_guess();

	// uploads new memory into the texture
	void ForceUpload_guess(unsigned char *texture, int wide, int tall);

	// returns the texture id
	int GetID_guess();

private:
	int _id_guess;
	bool _uploaded_guess;
	bool _valid_guess;
	unsigned char *_texture_guess;
	int _pos_guess[2];
	Color _color_guess;
	int _w_guess, _h_guess;
};

} // namespace vgui2

#endif // MEMORYBITMAP_H
