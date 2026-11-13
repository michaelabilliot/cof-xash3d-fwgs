// bzlib.h -- the parts of the bzip2 library interface the engine uses
// (bzip2 1.0, linked statically; BZ_API is WINAPI in the Windows build)

#ifndef BZLIB_H
#define BZLIB_H

#define BZ_OK		0

#define BZ_API( func )	WINAPI func

int BZ_API( BZ2_bzBuffToBuffCompress )( char *dest, unsigned int *destLen, char *source, unsigned int sourceLen, int blockSize100k, int verbosity, int workFactor );
int BZ_API( BZ2_bzBuffToBuffDecompress )( char *dest, unsigned int *destLen, char *source, unsigned int sourceLen, int /* small, a macro in windows.h */, int verbosity );

#endif // BZLIB_H
