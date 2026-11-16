#ifndef SV_SAVE_TOKENS_H
#define SV_SAVE_TOKENS_H

#include <string.h>

/* Empty hash entries occupy one NUL in the on-disk table. */
static inline int SV_EncodeSaveTokens( char *output, size_t capacity, char *const *tokens,
	int tokenCount, size_t *bytesWritten )
{
	size_t bytes = 0;
	for( int i = 0; i < tokenCount; ++i )
	{
		const char *token = tokens[i] ? tokens[i] : "";
		const size_t length = strlen( token ) + 1;
		if( length > capacity - bytes )
			return 0;
		memcpy( output + bytes, token, length );
		bytes += length;
	}
	*bytesWritten = bytes;
	return 1;
}

#endif
