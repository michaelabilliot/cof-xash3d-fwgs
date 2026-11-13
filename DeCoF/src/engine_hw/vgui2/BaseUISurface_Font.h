//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: the engine's copy of the VGUI1 font (BaseUISurface_Font.cpp)
//
// $NoKeywords: $
//=============================================================================

#ifndef BASEUISURFACE_FONT_H
#define BASEUISURFACE_FONT_H

#ifdef _WIN32
#pragma once
#endif

typedef unsigned char uchar;

class BaseFontPlat;
struct SurfacePlat;

class EngineFont
{
public:
	EngineFont( const char *name, void *pFileData, int fileDataLen, int tall, int wide, float rotation, int weight, bool italic, bool underline, bool strikeout, bool symbol );
	virtual ~EngineFont();

private:
	virtual void Init( const char *name, void *pFileData, int fileDataLen, int tall, int wide, float rotation, int weight, bool italic, bool underline, bool strikeout, bool symbol );

public:
	BaseFontPlat *getPlat();
	virtual void getCharRGBA( int ch, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, uchar *rgba );
	virtual void GetCharABCwide( int ch, int &a, int &b, int &c );
	virtual void GetTextSize( const char *text, int &wide, int &tall );
	virtual int GetTall();
	virtual int getId();

protected:
	char *_name;
	BaseFontPlat *_plat;
	int _id;
};

class BaseFontPlat
{
public:
	virtual ~BaseFontPlat() {}
	virtual bool equals( const char *name, int tall, int wide, float rotation, int weight, bool italic, bool underline, bool strikeout, bool symbol ) = 0;
	virtual void getCharRGBA( int ch, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, uchar *rgba ) = 0;
	virtual void GetCharABCwide( int ch, int &a, int &b, int &c ) = 0;
	virtual int GetTall() = 0;
	virtual void DrawSetTextFont( SurfacePlat *plat ) = 0;
	virtual int getId() = 0;
};

void Font_Reset();

#endif // BASEUISURFACE_FONT_H
