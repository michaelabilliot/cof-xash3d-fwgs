// crc.h -- CRC32 checksums and MD5 hashes (crc.c)

#ifndef CRC_H
#define CRC_H

typedef unsigned int CRC32_t;

typedef struct
{
	unsigned int	buf[4];
	unsigned int	bits[2];
	unsigned char	in[64];
} MD5Context_t;

void	CRC32_Init( CRC32_t *pulCRC );
CRC32_t	CRC32_Final( CRC32_t pulCRC );
void	CRC32_ProcessByte( CRC32_t *pulCRC, unsigned char ch );
void	CRC32_ProcessBuffer( CRC32_t *pulCRC, void *pBuffer, int nBuffer );
byte	COM_BlockSequenceCRCByte( byte *base, int length, int sequence );
BOOL	CRC_File( CRC32_t *crcvalue, char *pszFileName );
int		CRC_MapFile( CRC32_t *crcvalue, char *pszFileName );

void	MD5Init( MD5Context_t *ctx );
void	MD5Update( MD5Context_t *ctx, const unsigned char *buf, unsigned int len );
void	MD5Final( unsigned char digest[16], MD5Context_t *ctx );
void	MD5Transform( unsigned int buf[4], const unsigned int in[16] );
BOOL	MD5_Hash_File( unsigned char digest[16], char *pszFileName, BOOL bUsefopen, BOOL bSeed, unsigned int seed[4] );
char	*MD5_Print( unsigned char hash[16] );

#endif // CRC_H
