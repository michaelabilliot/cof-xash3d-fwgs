//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Rate limit of the connectionless packets per ip and in total
//
// $NoKeywords: $
//=============================================================================

#ifndef IPRATELIMIT_H
#define IPRATELIMIT_H
#ifdef _WIN32
#pragma once
#endif

#include "UtlRBTree.h"

class CIPRateLimit
{
public:
	CIPRateLimit();
	~CIPRateLimit();

	// updates an ip entry, return true if the ip is allowed, false otherwise
	bool CheckIP( netadr_t adr );

private:
	enum { MAX_TREE_SIZE = 512, START_TREE_SIZE = 256, FLUSH_TIMEOUT = 120 };

	typedef int ip_t;
	typedef struct iprate_s
	{
		ip_t ip;
		long lastTime;
		int count;
	} iprate_t;

	CUtlRBTree< iprate_t, int > m_IPTree;
	int m_iGlobalCount;
	long m_lLastTime;

	static bool LessIP( const iprate_t &lhs, const iprate_t &rhs );
};

#endif // IPRATELIMIT_H
