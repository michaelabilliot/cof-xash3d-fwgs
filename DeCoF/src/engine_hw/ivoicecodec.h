//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: the interface of the voice codec DLLs (voice_miles.dll)
//
// $NoKeywords: $
//=============================================================================

#ifndef IVOICECODEC_H
#define IVOICECODEC_H
#pragma once

#include "interface.h"

class IVoiceCodec : public IBaseInterface
{
protected:
	virtual			~IVoiceCodec() {}

public:
	// Initialize the object.
	virtual bool	Init( int quality ) = 0;

	// Use this to delete the object.
	virtual void	Release() = 0;

	// Compress the voice data. Returns the number of bytes written into pCompressed.
	virtual int		Compress( const char *pUncompressed, int nSamples, char *pCompressed, int maxCompressedBytes, bool bFinal ) = 0;

	// Decompress voice data. Returns the number of samples written into pUncompressed.
	virtual int		Decompress( const char *pCompressed, int compressedBytes, char *pUncompressed, int maxUncompressedBytes ) = 0;

	// Some codecs keep state between Compress and Decompress calls. This clears it.
	virtual bool	ResetState() = 0;
};

#endif // IVOICECODEC_H
