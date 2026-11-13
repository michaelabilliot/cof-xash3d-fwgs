//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: The tasks of a career map
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "CareerGameTasks.h"
#include "../game_shared/shared_util.h"

#include <vgui/ILocalize.h>
#include <vgui_controls/Controls.h>

using namespace vgui2;

//-----------------------------------------------------------------------------
// Purpose: Builds a localized string like ILocalize::ConstructString, but
//			honours reordered parameters (%s2 before %s1) in the format string
//-----------------------------------------------------------------------------
void ConstructStringWithReordering( wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, wchar_t *formatString, int numFormatParameters, ... )
{
	if ( !formatString )
	{
		unicodeOutput[0] = 0;
		return;
	}

	int unicodeBufferSize = unicodeBufferSizeInBytes / sizeof( wchar_t );

	va_list argList;
	va_start( argList, numFormatParameters );

	int argNum = 0;
	int stringNum = 1;

	int i;
	wchar_t **fragments = new wchar_t *[numFormatParameters];
	for ( i = 0; i < numFormatParameters; i++ )
	{
		fragments[i] = va_arg( argList, wchar_t * );
	}

	va_end( argList );

	int nextFragment = 0;
	wchar_t *tmpFormatString = new wchar_t[wcslen( formatString ) + 1];
	wcscpy( tmpFormatString, formatString );
	formatString = tmpFormatString;

	// put the parameters in the order the format string uses them
	wchar_t *tmp = formatString;
	while ( tmp && nextFragment < numFormatParameters )
	{
		tmp = wcsstr( tmp, L"%s" );
		if ( tmp && tmp[2] >= L'0' && tmp[2] <= L'9' )
		{
			int foundFragment = tmp[2] - L'1';
			if ( foundFragment < numFormatParameters )
			{
				wchar_t *tmpFragment = fragments[nextFragment];
				fragments[nextFragment] = fragments[foundFragment];
				fragments[foundFragment] = tmpFragment;

				wchar_t replacementChar = tmp[2];
				tmp[2] = nextFragment + L'1';
				++nextFragment;

				// renumber the other occurrence of the new number
				wchar_t otherPercent[4] = L"%s1";
				otherPercent[2] = tmp[2];
				wchar_t *other = wcsstr( tmp + 3, otherPercent );
				if ( other )
				{
					other[2] = replacementChar;
				}
			}
			tmp += 3;
		}
	}

	nextFragment = 0;
	while ( 1 )
	{
		char searchStr[4];
		wchar_t unicodePS[4];
		searchStr[0] = '%';
		searchStr[1] = 's';
		searchStr[2] = (char)stringNum + '0';
		searchStr[3] = 0;
		localize()->ConvertANSIToUnicode( searchStr, unicodePS, sizeof( unicodePS ) );

		wchar_t *searchPos = wcsstr( formatString, unicodePS );
		int precopyCount = 0;
		if ( searchPos )
		{
			precopyCount = searchPos - formatString;
		}
		else
		{
			precopyCount = wcslen( formatString ) + 1;
		}

		if ( precopyCount > unicodeBufferSize )
		{
			precopyCount = unicodeBufferSize;
		}

		wcsncpy( unicodeOutput, formatString, precopyCount );
		unicodeOutput += precopyCount;
		formatString += precopyCount;
		unicodeBufferSize -= precopyCount;

		if ( !searchPos )
			break;

		if ( argNum < numFormatParameters )
		{
			wchar_t *param = fragments[nextFragment++];
			if ( !param )
			{
				delete [] tmpFormatString;
				delete [] fragments;
				return;
			}

			int paramSize = wcslen( param );
			if ( paramSize > unicodeBufferSize )
			{
				paramSize = unicodeBufferSize;
			}

			wcsncpy( unicodeOutput, param, paramSize );
			unicodeOutput += paramSize;
			unicodeBufferSize -= paramSize;

			formatString += 3;
		}

		++stringNum;
		++argNum;
	}

	unicodeOutput[unicodeBufferSize - 1] = 0;

	delete [] tmpFormatString;
	delete [] fragments;
}

//-----------------------------------------------------------------------------
// Purpose: Parses a task string ("kill 3 inarow", "killwith 2 awp") and
//			builds its localized descriptions
//-----------------------------------------------------------------------------
CCareerUITask::CCareerUITask( const char *taskName )
{
	strncpy( m_name, taskName, sizeof( m_name ) );
	m_name[sizeof( m_name ) - 1] = 0;
	m_survive = false;
	m_inarow = false;
	m_repeat = 1;
	m_isWinFast = false;

	const char *tmp = taskName;
	m_name[0] = 0;
	m_weapon[0] = 0;
	while ( SharedTokenWaiting( tmp ) )
	{
		tmp = SharedParse( tmp );
		const char *token = SharedGetToken();
		if ( token )
		{
			if ( !stricmp( token, "inarow" ) )
			{
				m_inarow = true;
				m_survive = false;
			}
			else if ( !stricmp( token, "survive" ) )
			{
				m_survive = true;
				m_inarow = false;
			}
			else
			{
				int val = atoi( token );
				if ( val )
				{
					m_repeat = val;
				}
				else if ( !m_name[0] )
				{
					strncpy( m_name, token, sizeof( m_name ) );
					m_name[sizeof( m_name ) - 1] = 0;
				}
				else if ( !m_weapon[0] )
				{
					strncpy( m_weapon, token, sizeof( m_weapon ) );
					m_weapon[sizeof( m_weapon ) - 1] = 0;
				}
			}
		}
	}

	if ( !stricmp( m_weapon, "grenade" ) && m_repeat > 1 )
	{
		strncpy( m_weapon, "hegren", sizeof( m_weapon ) );
		m_weapon[sizeof( m_weapon ) - 1] = 0;
	}
	else if ( !stricmp( m_weapon, "hegren" ) && m_repeat == 1 )
	{
		strncpy( m_weapon, "grenade", sizeof( m_weapon ) );
		m_weapon[sizeof( m_weapon ) - 1] = 0;
	}

	m_localizedName[0] = 0;

	char taskIdStr[144];
	sprintf( taskIdStr, "#Career_Weapon_%s", m_weapon );
	wchar_t *weaponStr = localize()->Find( taskIdStr );
	if ( !weaponStr )
	{
		weaponStr = L"";
	}

	wchar_t numBuf[16];
	_snwprintf( numBuf, sizeof( numBuf ) / sizeof( wchar_t ), L"%d", m_repeat );

	if ( m_repeat == 1 )
	{
		sprintf( taskIdStr, "#Career_Task_%s", m_name );
	}
	else
	{
		sprintf( taskIdStr, "#Career_Task_%s_multi", m_name );
	}

	if ( !stricmp( m_name, "winfast" ) )
	{
		m_isWinFast = true;
	}

	wchar_t tmpTask[512];
	wchar_t *localizedTaskStr = localize()->Find( taskIdStr );
	ConstructStringWithReordering( tmpTask, sizeof( tmpTask ) / sizeof( wchar_t ), localizedTaskStr, 2, weaponStr, numBuf );

	if ( m_inarow )
	{
		localize()->ConstructString( m_localizedName, sizeof( m_localizedName ) / sizeof( wchar_t ), localize()->Find( "#Career_Task_inarow" ), 1, tmpTask );
	}
	else if ( m_survive )
	{
		localize()->ConstructString( m_localizedName, sizeof( m_localizedName ) / sizeof( wchar_t ), localize()->Find( "#Career_Task_survive" ), 1, tmpTask );
	}
	else
	{
		wcscpy( m_localizedName, tmpTask );
	}

	localize()->ConstructString( m_localizedNameCompleted, sizeof( m_localizedNameCompleted ), localize()->Find( "#Career_Task_Done" ), 1, m_localizedName );

	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: The task name, with the completed marker once it is done
//-----------------------------------------------------------------------------
const wchar_t *CCareerUITask::GetLocalizedTaskNameWithCompletion()
{
	if ( m_isComplete )
	{
		return m_localizedNameCompleted;
	}
	else
	{
		return m_localizedName;
	}
}
