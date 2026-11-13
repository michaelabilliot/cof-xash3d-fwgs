//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Rate limit of the connectionless packets per ip and in total
//
// $NoKeywords: $
//=============================================================================

#include <time.h>

extern "C"
{
#include "quakedef.h"
#include "cvardef.h"
#include "net.h"

extern cvar_t	max_queries_sec;
extern cvar_t	max_queries_sec_global;
extern cvar_t	max_queries_window;
}

#include "ipratelimit.h"

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CIPRateLimit::CIPRateLimit() : m_IPTree( 0, START_TREE_SIZE, LessIP )
{
	m_iGlobalCount = 0;
	m_lLastTime = -1;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CIPRateLimit::~CIPRateLimit()
{
}

//-----------------------------------------------------------------------------
// Purpose: sort func for rb tree
//-----------------------------------------------------------------------------
bool CIPRateLimit::LessIP( const iprate_t &lhs, const iprate_t &rhs )
{
	return ( lhs.ip < rhs.ip );
}

//-----------------------------------------------------------------------------
// Purpose: return false if this IP has exceeded limits
//-----------------------------------------------------------------------------
bool CIPRateLimit::CheckIP( netadr_t adr )
{
	long curTime = time( NULL );

	// check the per ip rate (do this first, so one person dosing doesn't add to the global max rate
	ip_t clientIP;
	memcpy( &clientIP, adr.ip, sizeof( ip_t ) );

	if ( m_IPTree.Count() > MAX_TREE_SIZE ) // if we have stored too many items
	{
		ip_t tmp = m_IPTree.LastInorder(); // we step BACKWARD's through the tree
		int i = 0;
		while ( ( m_IPTree.Count() > ( 2 * MAX_TREE_SIZE ) / 3 ) && i < m_IPTree.MaxElement() ) // trim 1/3 the entries from the tree and only traverse the max nodes
		{
			if ( m_IPTree.IsValidIndex( tmp ) &&
				( curTime - m_IPTree[ tmp ].lastTime ) > FLUSH_TIMEOUT &&
				m_IPTree[ tmp ].ip != clientIP )
			{
				ip_t removeIPT = tmp;
				tmp = m_IPTree.PrevInorder( tmp );
				m_IPTree.RemoveAt( removeIPT );
				continue;
			}
			i++;
			tmp = m_IPTree.PrevInorder( tmp );
		}
	}

	// now find the entry and check if its within our rate limits
	iprate_t findEntry = { clientIP };
	ip_t entry = m_IPTree.Find( findEntry );

	if ( m_IPTree.IsValidIndex( entry ) )
	{
		m_IPTree[ entry ].count++; // a new hit

		if ( ( curTime - m_IPTree[ entry ].lastTime ) > max_queries_window.value )
		{
			m_IPTree[ entry ].lastTime = curTime;
			m_IPTree[ entry ].count = 1;
		}
		else
		{
			float query_rate = m_IPTree[ entry ].count / max_queries_window.value; // add one so the bottom is never zero
			if ( query_rate > max_queries_sec.value )
			{
				return false;
			}
		}
	}
	else
	{
		// not found, insert this new guy
		iprate_t newEntry;
		newEntry.count = 1;
		newEntry.lastTime = curTime;
		newEntry.ip = clientIP;
		m_IPTree.Insert( newEntry );
	}

	// now check the global rate
	m_iGlobalCount++;

	if ( ( curTime - m_lLastTime ) > max_queries_window.value )
	{
		m_lLastTime = curTime;
		m_iGlobalCount = 1;
	}
	else
	{
		float query_rate = m_iGlobalCount / max_queries_window.value; // add one so the bottom is never zero
		if ( query_rate > max_queries_sec_global.value )
		{
			return false;
		}
	}

	return true;
}
