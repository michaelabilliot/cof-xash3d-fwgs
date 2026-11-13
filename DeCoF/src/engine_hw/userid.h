// userid.h -- the network id of a connected player

#ifndef USERID_H
#define USERID_H

// Authentication types
#define AUTH_IDTYPE_UNKNOWN	0
#define AUTH_IDTYPE_STEAM	1
#define AUTH_IDTYPE_VALVE	2
#define AUTH_IDTYPE_LOCAL	3

typedef struct USERID_s
{
	int		idtype;
	uint64	m_SteamID;
	unsigned int	clientip;
} USERID_t;

#endif // USERID_H
