// verify_guess.c -- a marker block in the code: eight magic bytes and 28 zero bytes,
// room for data a tool can find and fill in after the build. Nothing calls it, only
// g_pVerifyMarker_guess points at it. No Linux file and no string names this object;
// swds.dll has the same block between UtlBuffer.cpp and vgui_EngineSurface.cpp.

void VerifyMarker_guess( void )
{
	__asm
	{
		_emit 0x01
		_emit 0x02
		_emit 0x03
		_emit 0x04
		_emit 0x04
		_emit 0x03
		_emit 0x02
		_emit 0x01
		_emit 0x00
		_emit 0x00
		_emit 0x00
		_emit 0x00
		_emit 0x00
		_emit 0x00
		_emit 0x00
		_emit 0x00
		_emit 0x00
		_emit 0x00
		_emit 0x00
		_emit 0x00
		_emit 0x00
		_emit 0x00
		_emit 0x00
		_emit 0x00
		_emit 0x00
		_emit 0x00
		_emit 0x00
		_emit 0x00
		_emit 0x00
		_emit 0x00
		_emit 0x00
		_emit 0x00
		_emit 0x00
		_emit 0x00
		_emit 0x00
		_emit 0x00
	}
}

void *g_pVerifyMarker_guess = (void *)VerifyMarker_guess;
