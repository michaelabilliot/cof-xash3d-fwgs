// glide_guess.c -- 3Dfx Glide helpers for the 3Dfx OpenGL driver (glide2x.dll)

#include <windows.h>
#include "quakedef.h"

// from glide.h
typedef unsigned long	FxU32;

typedef struct
{
	FxU32	pixelsIn;
	FxU32	chromaFail;
	FxU32	zFuncFail;
	FxU32	aFuncFail;
	FxU32	pixelsOut;
} GrSstPerfStats_t;

static HMODULE	hGlide3Dfx_guess;

void InitGlide3Dfx_guess( void )
{
	if ( !hGlide3Dfx_guess )
		hGlide3Dfx_guess = GetModuleHandle( "glide2x" );
}

int PerfStats3Dfx_guess( void )
{
	FARPROC				pgrSstResetPerfStats_guess;
	FARPROC				pgrTriStats_guess;
	FARPROC				pgrIdle_guess;
	GrSstPerfStats_t	perfStats_guess;
	FARPROC				pgrSstPerfStats_guess;
	FARPROC				pfnResetTriStats_guess;
	FxU32				nProcessed_guess;
	FxU32				numDrawn_guess;

	InitGlide3Dfx_guess();
	if ( hGlide3Dfx_guess )
	{
		pgrSstPerfStats_guess = GetProcAddress( hGlide3Dfx_guess, "_grSstPerfStats@4" );
		pgrSstResetPerfStats_guess = GetProcAddress( hGlide3Dfx_guess, "_grSstResetPerfStats@0" );
		pgrIdle_guess = GetProcAddress( hGlide3Dfx_guess, "_grSstIdle@0" );
		pgrTriStats_guess = GetProcAddress( hGlide3Dfx_guess, "_grTriStats@8" );
		pfnResetTriStats_guess = GetProcAddress( hGlide3Dfx_guess, "_grResetTriStats@0" );

		if ( pgrSstPerfStats_guess && pgrIdle_guess && pgrSstResetPerfStats_guess && pgrTriStats_guess && pfnResetTriStats_guess )
		{
			pgrIdle_guess();
			pgrSstPerfStats_guess( &perfStats_guess );
			pgrTriStats_guess( &nProcessed_guess, &numDrawn_guess );
			pgrSstResetPerfStats_guess();
			pfnResetTriStats_guess();
			return perfStats_guess.pixelsIn;
		}
	}

	return 0;
}

int ReadPixels3Dfx_guess( int x, int y, int width, int height, byte *buffer )
{
	int				n1_guess, n2_guess;
	int				i_guess, j_guess;
	unsigned short	temp_guess;
	FARPROC			proc_guess;

	InitGlide3Dfx_guess();
	if ( !hGlide3Dfx_guess )
		return 0;

	proc_guess = GetProcAddress( hGlide3Dfx_guess, "_grLfbReadRegion@28" );
	if ( proc_guess && proc_guess( 0, x, y, width, height, width * 2, buffer ) )
	{
		// the frame buffer is bottom up: flip the rows
		for ( i_guess = 0; i_guess < height / 2; i_guess++ )
		{
			n1_guess = i_guess * width;
			n2_guess = ( height - i_guess - 1 ) * width;
			for ( j_guess = 0; j_guess < width; j_guess++, n1_guess++, n2_guess++ )
			{
				temp_guess = ( (unsigned short *)buffer )[n1_guess];
				( (unsigned short *)buffer )[n1_guess] = ( (unsigned short *)buffer )[n2_guess];
				( (unsigned short *)buffer )[n2_guess] = temp_guess;
			}
		}

		// expand the 565 pixels to RGB in place, from the end
		for ( i_guess = 0; i_guess < height; i_guess++ )
		{
			n1_guess = ( height - i_guess ) * width;
			for ( j_guess = 0; j_guess < width; j_guess++ )
			{
				n1_guess--;
				temp_guess = ( (unsigned short *)buffer )[n1_guess];
				buffer[n1_guess * 3] = ( temp_guess & 0xF800 ) >> 8;
				buffer[n1_guess * 3 + 1] = ( temp_guess & 0x07E0 ) >> 3;
				buffer[n1_guess * 3 + 2] = ( temp_guess & 0x001F ) << 3;
			}
		}
		return 1;
	}

	return 0;
}
