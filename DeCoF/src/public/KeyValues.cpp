//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <direct.h>
#include <malloc.h>

#include <vgui/KeyValues.h>
#include "IKeyValues.h"
#include "FileSystem.h"
#include "UtlBuffer.h"
#include <vgui_controls/Controls.h>

#define KEYVALUES_TOKEN_SIZE	512

//-----------------------------------------------------------------------------
// Purpose: Writes out a set of indenting
//-----------------------------------------------------------------------------
static inline void WriteIndents(CUtlBuffer &buffer, int indentLevel)
{
	for (int i = 0; i < indentLevel; i++)
	{
		buffer.Put("\t", 1);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
KeyValues::KeyValues( const char *setName )
{
	Init(setName);
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
KeyValues::KeyValues( const char *setName, const char *firstKey, const char *firstValue )
{
	Init(setName);
	SetString(firstKey, firstValue);
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
KeyValues::KeyValues( const char *setName, const char *firstKey, const wchar_t *firstValue )
{
	Init(setName);
	SetWString(firstKey, firstValue);
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
KeyValues::KeyValues( const char *setName, const char *firstKey, int firstValue )
{
	Init(setName);
	SetInt(firstKey, firstValue);
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
KeyValues::KeyValues( const char *setName, const char *firstKey, const char *firstValue, const char *secondKey, const char *secondValue )
{
	Init(setName);
	SetString(firstKey, firstValue);
	SetString(secondKey, secondValue);
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
KeyValues::KeyValues( const char *setName, const char *firstKey, int firstValue, const char *secondKey, int secondValue )
{
	Init(setName);
	SetInt(firstKey, firstValue);
	SetInt(secondKey, secondValue);
}

//-----------------------------------------------------------------------------
// Purpose: Initialize member variables
//-----------------------------------------------------------------------------
void KeyValues::Init(const char *setName)
{
	m_iKeyName = keyvalues()->GetSymbolForString(setName);
	m_iDataType = TYPE_NONE;

	m_pSub = NULL;
	m_pPeer = NULL;
	m_sValue = NULL;
	m_iAllocationSize = 0;
	m_pValue = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
KeyValues::~KeyValues()
{
	KeyValues *datNext = NULL;
	for (KeyValues *dat = m_pSub; dat != NULL; dat = datNext)
	{
		datNext = dat->m_pPeer;
		delete dat;
	}

	FreeAllocatedValue();
}

//-----------------------------------------------------------------------------
// Purpose: Get the name of the current key section
//-----------------------------------------------------------------------------
const char *KeyValues::GetName( void ) const
{
	return keyvalues()->GetStringForSymbol(m_iKeyName);
}

//-----------------------------------------------------------------------------
// Purpose: Get the symbol name of the current key section
//-----------------------------------------------------------------------------
int KeyValues::GetNameSymbol() const
{
	return m_iKeyName;
}

//-----------------------------------------------------------------------------
// Purpose: Read a token from buffer, returns NULL if at end of file
//-----------------------------------------------------------------------------
static char *ReadToken(char **buffer)
{
	static char buf[KEYVALUES_TOKEN_SIZE];
	int bufC = 0;
	char c = 0;
	bool skipWhitespace = true;

	while (skipWhitespace)
	{
		skipWhitespace = false;

		// skip any whitespace
		do
		{
			c = **buffer;
			(*buffer)++;
		}
		while (isspace(c));

		// skip c++ style comments
		if (c == '/')
		{
			c = **buffer;
			(*buffer)++;
			if (c != '/')
			{
				// not a comment, keep the slash
				if (bufC < sizeof(buf))
				{
					buf[bufC++] = '/';
				}
			}
			else
			{
				// read till end of line
				while (c > 0 && c != '\n')
				{
					c = **buffer;
					(*buffer)++;
				}
				skipWhitespace = true;
			}
		}
	}

	if (c == '\"')
	{
		// read the quoted string
		while (1)
		{
			c = **buffer;
			(*buffer)++;
			if (c == '\"')
				break;

			if (!c)
				break;

			if (c == '\\')
			{
				// escape sequence
				c = **buffer;
				(*buffer)++;

				switch (c)
				{
				case 'n':  c = '\n'; break;
				case '\\': c = '\\'; break;
				case 't':  c = '\t'; break;
				case '\"': c = '\"'; break;
				default:   c = c;    break;
				}
			}

			if (bufC < sizeof(buf))
			{
				buf[bufC++] = c;
			}
		}
	}
	else
	{
		// read in the string until we hit a whitespace
		while (**buffer)
		{
			if (c < 0)
				break;

			if (isspace(c))
				break;

			if (bufC < sizeof(buf))
			{
				buf[bufC++] = c;
			}
			c = **buffer;
			(*buffer)++;
		}
	}

	// if we're at the end of the buffer, return NULL
	if (c < 0 && !bufC)
		return NULL;

	// null terminate
	if (bufC == sizeof(buf))
	{
		bufC--;
	}
	buf[bufC] = 0;
	return buf;
}

//-----------------------------------------------------------------------------
// Purpose: Load keyValues from disk
//-----------------------------------------------------------------------------
bool KeyValues::LoadFromFile( IFileSystem *filesystem, const char *resourceName, const char *pathID )
{
	FileHandle_t f = filesystem->Open(resourceName, "rb", pathID);
	if (!f)
		return false;

	// load file into a null-terminated buffer
	int fileSize = filesystem->Size(f);
	void *bufStart = malloc(fileSize + 1);
	char *buffer = (char *)bufStart;
	if (!buffer)
		return false;

	buffer[fileSize] = 0;
	filesystem->Read(buffer, fileSize, f); // read into local buffer

	LoadFromBuffer(buffer);

	filesystem->Close(f);	// close file after reading
	free(bufStart);

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: parses a key description from a buffer
//-----------------------------------------------------------------------------
void KeyValues::LoadFromBuffer( const char *buffer )
{
	char *bufStart = (char *)buffer;

	// the first thing must be a key
	char *s = ReadToken(&bufStart);
	if (s)
	{
		int newName = keyvalues()->GetSymbolForString(s);
		m_iKeyName = newName;
	}

	// get the '{'
	s = ReadToken(&bufStart);

	if (m_iKeyName > INVALID_KEY_SYMBOL && s && *s == '{')
	{
		// header is valid so load the file
		RecursiveLoadFromBuffer(&bufStart);
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void KeyValues::RecursiveLoadFromBuffer(char **buffer)
{
	while (1)
	{
		// get the key name
		char *s = ReadToken(buffer);
		if (!s || !*s)
			break;

		if (!strcmp(s, "}"))
			break;

		KeyValues *dat = FindKey(s, true);

		// get the value
		s = ReadToken(buffer);

		if (!s)
			break;

		if (!strcmp(s, "}"))
			break;

		if (!strcmp(s, "{"))
		{
			// sub value list
			dat->RecursiveLoadFromBuffer(buffer);
		}
		else
		{
			int len = strlen(s) + 1;
			FreeAllocatedValue();
			dat->AllocateValueBlock(len);
			memcpy(dat->m_sValue, s, len);
			dat->m_iDataType = TYPE_STRING;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Save the keyvalues to disk
//			Creates the path to the file if it doesn't exist
//-----------------------------------------------------------------------------
bool KeyValues::SaveToFile( IFileSystem *filesystem, const char *resourceName, const char *pathID )
{
	// create a write file
	FileHandle_t f = filesystem->Open(resourceName, "wb", pathID);

	if (!f)
	{
		// create the path to the file
		const char *currentFileName = resourceName;
		char szBuf[1024] = "";

		// Find any slashes in the path and use them to make directories
		while (1)
		{
			const char *fileSlash = strchr(currentFileName, '\\');
			if (!fileSlash)
				break;

			int pathSize = fileSlash - currentFileName;

			if (szBuf[0])
			{
				strcat(szBuf, "\\");
			}

			strncat(szBuf, currentFileName, pathSize);
			_mkdir(szBuf);
			currentFileName += pathSize + 1;
		}

		f = filesystem->Open(resourceName, "wb", pathID);
	}

	if (!f)
		return false;

	CUtlBuffer buffer(0, 4096, true);
	RecursiveSaveToFile(filesystem, buffer, 0);
	buffer.PutChar(0);
	filesystem->Write(buffer.Base(), buffer.TellPut(), f);

	filesystem->Close(f);
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Write out a string where we convert the double quotes to backslash double quote
//-----------------------------------------------------------------------------
void KeyValues::WriteConvertedString( CUtlBuffer &buffer, const char *pszString )
{
	// handle double quote chars within the string
	// the worst possible case is that the whole string is quotes
	int len = strlen(pszString);
	char *convertedString = (char *) _alloca ((len + 1) * sizeof(char) * 2);
	int j=0;
	for (int i=0; i <= len; i++)
	{
		if (pszString[i] == '\"')
		{
			convertedString[j] = '\\';
			j++;
		}
		else if (pszString[i] == '\\')
		{
			convertedString[j] = '\\';
			j++;
		}
		convertedString[j] = pszString[i];
		j++;
	}

	buffer.Put(convertedString, strlen(convertedString));
}

//-----------------------------------------------------------------------------
// Purpose: Save keyvalues from disk, if subkey values are detected, calls
//			itself to save those
//-----------------------------------------------------------------------------
void KeyValues::RecursiveSaveToFile( IFileSystem *filesystem, CUtlBuffer &buffer, int indentLevel )
{
	// write header
	WriteIndents(buffer, indentLevel);
	buffer.Put("\"", 1);
	buffer.Put(GetName(), strlen(GetName()));
	buffer.Put("\"\n", 2);
	WriteIndents(buffer, indentLevel);
	buffer.Put("{\n", 2);

	// loop through all our keys writing them to disk
	for (KeyValues *dat = m_pSub; dat != NULL; dat = dat->m_pPeer)
	{
		if ( dat->m_pSub )
		{
			dat->RecursiveSaveToFile(filesystem, buffer, indentLevel + 1);
		}
		else
		{
			// only write non-empty keys

			switch (dat->m_iDataType)
			{
			case TYPE_STRING:
				{
					if (dat->m_sValue && *(dat->m_sValue))
					{
						WriteIndents(buffer, indentLevel + 1);
						buffer.Put("\"", 1);
						buffer.Put(dat->GetName(), strlen(dat->GetName()));
						buffer.Put("\"\t\t\"", 4);

						WriteConvertedString(buffer, dat->m_sValue);

						buffer.Put("\"\n", 2);
					}
					break;
				}
			case TYPE_WSTRING:
				{
					if ( dat->m_pValue )
					{
						char buf[1024];
						keyvalues()->GetANSIFromLocalized((wchar_t *)dat->m_pValue, buf, sizeof(buf));

						WriteIndents(buffer, indentLevel + 1);
						buffer.Put("\"", 1);
						buffer.Put(dat->GetName(), strlen(dat->GetName()));
						buffer.Put("\"\t\t\"", 4);

						WriteConvertedString(buffer, buf);

						buffer.Put("\"\n", 2);
					}
					break;
				}

			case TYPE_INT:
				{
					WriteIndents(buffer, indentLevel + 1);
					buffer.Put("\"", 1);
					buffer.Put(dat->GetName(), strlen(dat->GetName()));
					buffer.Put("\"\t\t\"", 4);

					char buf[32];
					sprintf(buf, "%d", dat->m_iValue);

					buffer.Put(buf, strlen(buf));
					buffer.Put("\"\n", 2);
					break;
				}

			case TYPE_FLOAT:
				{
					WriteIndents(buffer, indentLevel + 1);
					buffer.Put("\"", 1);
					buffer.Put(dat->GetName(), strlen(dat->GetName()));
					buffer.Put("\"\t\t\"", 4);

					char buf[48];
					sprintf(buf, "%f", dat->m_flValue);

					buffer.Put(buf, strlen(buf));
					buffer.Put("\"\n", 2);
					break;
				}
			}
		}
	}

	// write tail
	WriteIndents(buffer, indentLevel);
	buffer.Put("}\n", 2);
}

//-----------------------------------------------------------------------------
// Purpose: looks up a key by symbol name
//-----------------------------------------------------------------------------
KeyValues *KeyValues::FindKey(int keySymbol) const
{
	for (KeyValues *dat = m_pSub; dat != NULL; dat = dat->m_pPeer)
	{
		if (dat->m_iKeyName == keySymbol)
			return dat;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Find a keyValue, create it if it is not found.
//			Set bCreate to true to create the key if it doesn't already exist
//			(which ensures a valid pointer will be returned)
//-----------------------------------------------------------------------------
KeyValues *KeyValues::FindKey(const char *keyName, bool bCreate)
{
	// return the current key if a NULL subkey is asked for
	if (!keyName || !keyName[0])
		return this;

	// look for '/' characters deliminating sub fields
	char szBuf[256];
	const char *subStr = strchr(keyName, '/');
	const char *searchStr = keyName;

	// pull out the substring if it exists
	if (subStr)
	{
		int size = subStr - keyName;
		memcpy( szBuf, keyName, size );
		szBuf[size] = 0;
		searchStr = szBuf;
	}

	// lookup the symbol for the search string
	HKeySymbol iSearchStr = keyvalues()->GetSymbolForString(searchStr);

	KeyValues *lastItem = NULL;
	KeyValues *dat;
	// find the searchStr in the current peer list
	for (dat = m_pSub; dat != NULL; dat = dat->m_pPeer)
	{
		lastItem = dat;	// record the last item looked at (for if we need to append to the end of the list)

		// symbol compare
		if (dat->m_iKeyName == iSearchStr)
		{
			break;
		}
	}

	// make sure a key was found
	if (!dat)
	{
		if (bCreate)
		{
			// we need to create a new key
			dat = new KeyValues( searchStr );

			// insert new key at end of list
			if (lastItem)
			{
				lastItem->m_pPeer = dat;
			}
			else
			{
				m_pSub = dat;
			}
			dat->m_pPeer = NULL;

			// a key graduates to be a submsg as soon as it's m_pSub is set
			// this should be the only place m_pSub is set
			m_iDataType = TYPE_NONE;
		}
		else
		{
			return NULL;
		}
	}

	// if we've still got a subStr we need to keep looking deeper in the tree
	if ( subStr )
	{
		// recursively chain down through the paths in the string
		return dat->FindKey(subStr + 1, bCreate);
	}

	return dat;
}

//-----------------------------------------------------------------------------
// Purpose: Create a new key, with an autogenerated name.
//			Name is guaranteed to be an integer, of value 1 higher than the highest
//			other integer key name
//-----------------------------------------------------------------------------
KeyValues *KeyValues::CreateNewKey()
{
	int newID = 1;

	// search for any key with higher values
	for (KeyValues *dat = m_pSub; dat != NULL; dat = dat->m_pPeer)
	{
		// case-insensitive string compare
		int val = atoi(dat->GetName());
		if (newID <= val)
		{
			newID = val + 1;
		}
	}

	char buf[12];
	_snprintf( buf, sizeof(buf), "%d", newID );

	return FindKey(buf, true);
}

//-----------------------------------------------------------------------------
// Purpose: Adds a subkey. Make sure the subkey isn't a child of some other keyvalues
//-----------------------------------------------------------------------------
void KeyValues::AddSubKey( KeyValues *pSubkey )
{
	// add into subkey list
	if ( m_pSub == NULL )
	{
		m_pSub = pSubkey;
	}
	else
	{
		KeyValues *pTempDat = m_pSub;
		while ( pTempDat->GetNextKey() != NULL )
		{
			pTempDat = pTempDat->GetNextKey();
		}

		pTempDat->SetNextKey( pSubkey );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets this key's peer to the KeyValues passed in
//-----------------------------------------------------------------------------
void KeyValues::SetNextKey( KeyValues *pDat )
{
	m_pPeer = pDat;
}

//-----------------------------------------------------------------------------
// Purpose: Remove a subkey from the list
//-----------------------------------------------------------------------------
void KeyValues::RemoveSubKey(KeyValues *subKey)
{
	if (!subKey)
		return;

	// check the list pointer
	if (m_pSub == subKey)
	{
		m_pSub = subKey->m_pPeer;
	}
	else
	{
		// look through the list
		KeyValues *kv = m_pSub;
		while (kv->m_pPeer)
		{
			if (kv->m_pPeer == subKey)
			{
				kv->m_pPeer = subKey->m_pPeer;
				break;
			}

			kv = kv->m_pPeer;
		}
	}

	subKey->m_pPeer = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Return the first subkey in the list
//-----------------------------------------------------------------------------
KeyValues *KeyValues::GetFirstSubKey()
{
	return m_pSub;
}

//-----------------------------------------------------------------------------
// Purpose: Return the next subkey
//-----------------------------------------------------------------------------
KeyValues *KeyValues::GetNextKey()
{
	return m_pPeer;
}

//-----------------------------------------------------------------------------
// Purpose: Get the integer value of a keyName. Default value is returned
//			if the keyName can't be found.
//-----------------------------------------------------------------------------
int KeyValues::GetInt( const char *keyName, int defaultValue )
{
	KeyValues *dat = FindKey( keyName, false );
	if ( dat )
	{
		switch ( dat->m_iDataType )
		{
		case TYPE_STRING:
			return atoi(dat->m_sValue);
		case TYPE_WSTRING:
			return _wtoi((wchar_t *)dat->m_pValue);
		case TYPE_FLOAT:
			return (int)dat->m_flValue;
		case TYPE_UINT64:
			// can't convert, since it would lose data
			return 0;
		case TYPE_INT:
		case TYPE_PTR:
		default:
			return dat->m_iValue;
		};
	}
	return defaultValue;
}

//-----------------------------------------------------------------------------
// Purpose: Get the integer value of a keyName. Default value is returned
//			if the keyName can't be found.
//-----------------------------------------------------------------------------
uint64 KeyValues::GetUint64( const char *keyName, uint64 defaultValue )
{
	KeyValues *dat = FindKey( keyName, false );
	if ( dat )
	{
		switch ( dat->m_iDataType )
		{
		case TYPE_STRING:
			return atoi(dat->m_sValue);
		case TYPE_WSTRING:
			return _wtoi((wchar_t *)dat->m_pValue);
		case TYPE_FLOAT:
			return (int)dat->m_flValue;
		case TYPE_UINT64:
			return *((uint64 *)dat->m_sValue);
		case TYPE_INT:
		case TYPE_PTR:
		default:
			return dat->m_iValue;
		};
	}
	return defaultValue;
}

//-----------------------------------------------------------------------------
// Purpose: Get the pointer value of a keyName. Default value is returned
//			if the keyName can't be found.
//-----------------------------------------------------------------------------
void *KeyValues::GetPtr( const char *keyName, void *defaultValue )
{
	KeyValues *dat = FindKey( keyName, false );
	if ( dat )
	{
		switch ( dat->m_iDataType )
		{
		case TYPE_PTR:
			return dat->m_pValue;

		case TYPE_WSTRING:
		case TYPE_STRING:
		case TYPE_FLOAT:
		case TYPE_INT:
		case TYPE_UINT64:
		default:
			return NULL;
		};
	}
	return defaultValue;
}

//-----------------------------------------------------------------------------
// Purpose: Get the float value of a keyName. Default value is returned
//			if the keyName can't be found.
//-----------------------------------------------------------------------------
float KeyValues::GetFloat( const char *keyName, float defaultValue )
{
	KeyValues *dat = FindKey( keyName, false );
	if ( dat )
	{
		switch ( dat->m_iDataType )
		{
		case TYPE_STRING:
			return (float)atof(dat->m_sValue);
		case TYPE_WSTRING:
			return 0.0f;		// no wtof
		case TYPE_FLOAT:
			return dat->m_flValue;
		case TYPE_UINT64:
			return (float)(__int64)(*((uint64 *)dat->m_sValue));
		case TYPE_INT:
			return (float)dat->m_iValue;
		case TYPE_PTR:
		default:
			return 0.0f;
		};
	}
	return defaultValue;
}

//-----------------------------------------------------------------------------
// Purpose: Get the string pointer of a keyName. Default value is returned
//			if the keyName can't be found.
//-----------------------------------------------------------------------------
const char *KeyValues::GetString( const char *keyName, const char *defaultValue )
{
	KeyValues *dat = FindKey( keyName, false );
	if ( dat )
	{
		// convert the data to string form then return it
		char buf[64];
		switch ( dat->m_iDataType )
		{
		case TYPE_FLOAT:
			sprintf( buf, "%f", dat->m_flValue );
			SetString( keyName, buf );
			break;
		case TYPE_INT:
		case TYPE_PTR:
			sprintf( buf, "%d", dat->m_iValue );
			SetString( keyName, buf );
			break;
		case TYPE_WSTRING:
		{
			// convert the string to char *, set it for future use, and return it
			char buf[512];
			keyvalues()->GetANSIFromLocalized((wchar_t *)dat->m_pValue, buf, sizeof(buf));
			SetString( keyName, buf );
			break;
		}
		case TYPE_UINT64:
			_snprintf( buf, sizeof( buf ), "%I64i", *((uint64 *)(dat->m_sValue)) );
			SetString( keyName, buf );
			break;
		case TYPE_STRING:
			break;
		default:
			return defaultValue;
		};

		return dat->m_sValue;
	}
	return defaultValue;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const wchar_t *KeyValues::GetWString( const char *keyName, const wchar_t *defaultValue)
{
	KeyValues *dat = FindKey( keyName, false );
	if ( dat )
	{
		wchar_t wbuf[64];
		switch ( dat->m_iDataType )
		{
		case TYPE_FLOAT:
			_snwprintf(wbuf, sizeof(wbuf) / sizeof(wchar_t), L"%f", dat->m_flValue);
			SetWString( keyName, wbuf);
			break;
		case TYPE_INT:
		case TYPE_PTR:
			_snwprintf( wbuf, sizeof(wbuf), L"%d", dat->m_iValue );
			SetWString( keyName, wbuf );
			break;
		case TYPE_UINT64:
			_snwprintf( wbuf, sizeof(wbuf), L"%I64i", *((uint64 *)(dat->m_sValue)) );
			SetWString( keyName, wbuf );
			break;

		case TYPE_WSTRING:
			break;
		case TYPE_STRING:
		{
			wchar_t wbuftemp[512]; // convert to wide
			keyvalues()->GetLocalizedFromANSI(dat->m_sValue, wbuftemp, sizeof(wbuftemp));
			SetWString( keyName, wbuftemp );
			break;
		}
		default:
			return defaultValue;
		};

		return (const wchar_t *)dat->m_pValue;
	}
	return defaultValue;
}

//-----------------------------------------------------------------------------
// Purpose: Gets a color
//-----------------------------------------------------------------------------
Color KeyValues::GetColor( const char *keyName )
{
	Color color(0, 0, 0, 0);
	KeyValues *dat = FindKey( keyName, false );
	if ( dat )
	{
		if ( dat->m_iDataType == TYPE_COLOR )
		{
			color[0] = dat->m_Color[0];
			color[1] = dat->m_Color[1];
			color[2] = dat->m_Color[2];
			color[3] = dat->m_Color[3];
		}
		else if ( dat->m_iDataType == TYPE_FLOAT )
		{
			color[0] = dat->m_flValue;
		}
		else if ( dat->m_iDataType == TYPE_INT )
		{
			color[0] = dat->m_iValue;
		}
		else if ( dat->m_iDataType == TYPE_STRING )
		{
			// parse the colors out of the string
			float a = 0, b = 0, c = 0, d = 0;
			sscanf(dat->m_sValue, "%f %f %f %f", &a, &b, &c, &d);
			color[0] = (unsigned char)a;
			color[1] = (unsigned char)b;
			color[2] = (unsigned char)c;
			color[3] = (unsigned char)d;
		}
	}
	return color;
}

//-----------------------------------------------------------------------------
// Purpose: Sets a color
//-----------------------------------------------------------------------------
void KeyValues::SetColor( const char *keyName, Color value)
{
	KeyValues *dat = FindKey( keyName, true );

	if ( dat )
	{
		dat->FreeAllocatedValue();
		dat->m_iDataType = TYPE_COLOR;
		dat->m_Color[0] = value[0];
		dat->m_Color[1] = value[1];
		dat->m_Color[2] = value[2];
		dat->m_Color[3] = value[3];
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the string value of a keyName.
//-----------------------------------------------------------------------------
void KeyValues::SetString( const char *keyName, const char *value )
{
	KeyValues *dat = FindKey( keyName, true );

	if ( dat )
	{
		// delete the old value
		dat->FreeAllocatedValue();

		if (!value)
		{
			// ensure a valid value
			value = "";
		}

		// allocate memory for the new value and copy it in
		int len = strlen( value ) + 1;
		dat->AllocateValueBlock(len);
		memcpy( dat->m_sValue, value, len );

		dat->m_iDataType = TYPE_STRING;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the string value of a keyName.
//-----------------------------------------------------------------------------
void KeyValues::SetWString( const char *keyName, const wchar_t *value )
{
	KeyValues *dat = FindKey( keyName, true );
	if ( dat )
	{
		// delete the old value
		dat->FreeAllocatedValue();

		if (!value)
		{
			// ensure a valid value
			value = L"";
		}

		// allocate memory for the new value and copy it in
		int len = (wcslen( value ) + 1) * sizeof(wchar_t);
		dat->AllocateValueBlock(len);
		memcpy( dat->m_pValue, value, len );

		dat->m_iDataType = TYPE_WSTRING;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the integer value of a keyName.
//-----------------------------------------------------------------------------
void KeyValues::SetInt( const char *keyName, int value )
{
	KeyValues *dat = FindKey( keyName, true );

	if ( dat )
	{
		dat->FreeAllocatedValue();
		dat->m_iValue = value;
		dat->m_iDataType = TYPE_INT;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the integer value of a keyName.
//-----------------------------------------------------------------------------
void KeyValues::SetUint64( const char *keyName, uint64 value )
{
	KeyValues *dat = FindKey( keyName, true );

	if ( dat )
	{
		// delete the old value
		dat->FreeAllocatedValue();

		dat->AllocateValueBlock(sizeof(uint64));
		*((uint64 *)dat->m_sValue) = value;
		dat->m_iDataType = TYPE_UINT64;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the float value of a keyName.
//-----------------------------------------------------------------------------
void KeyValues::SetFloat( const char *keyName, float value )
{
	KeyValues *dat = FindKey( keyName, true );

	if ( dat )
	{
		dat->FreeAllocatedValue();
		dat->m_flValue = value;
		dat->m_iDataType = TYPE_FLOAT;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the pointer value of a keyName.
//-----------------------------------------------------------------------------
void KeyValues::SetPtr( const char *keyName, void *value )
{
	KeyValues *dat = FindKey( keyName, true );

	if ( dat )
	{
		dat->FreeAllocatedValue();
		dat->m_pValue = value;
		dat->m_iDataType = TYPE_PTR;
	}
}

//-----------------------------------------------------------------------------
// Purpose: allocates memory for the value, from the keyvalues pool if it is small
//-----------------------------------------------------------------------------
void KeyValues::AllocateValueBlock(int iAllocSize)
{
	if (iAllocSize <= sizeof(KeyValues))
	{
		m_pValue = keyvalues()->AllocKeyValuesMemory(iAllocSize);
	}
	else
	{
		m_pValue = new char[iAllocSize];
	}
	m_iAllocationSize = iAllocSize;
}

//-----------------------------------------------------------------------------
// Purpose: frees the value memory
//-----------------------------------------------------------------------------
void KeyValues::FreeAllocatedValue()
{
	if (m_iAllocationSize)
	{
		if (m_iAllocationSize <= sizeof(KeyValues))
		{
			keyvalues()->FreeKeyValuesMemory(m_pValue);
		}
		else
		{
			delete [] (char *)m_pValue;
		}

		m_iAllocationSize = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Makes a copy of the whole key-value pair set
//-----------------------------------------------------------------------------
KeyValues *KeyValues::MakeCopy( void ) const
{
	KeyValues *newKeyValue = new KeyValues(GetName());

	// copy data
	newKeyValue->m_iDataType = m_iDataType;
	switch ( m_iDataType )
	{
	case TYPE_STRING:
		{
			if ( m_sValue )
			{
				int len = strlen( m_sValue ) + 1;
				newKeyValue->AllocateValueBlock(len);
				memcpy( newKeyValue->m_sValue, m_sValue, len );
			}
		}
		break;
	case TYPE_WSTRING:
		{
			if ( m_pValue )
			{
				int len = (wcslen( (wchar_t *)m_pValue ) + 1) * sizeof(wchar_t);
				newKeyValue->AllocateValueBlock(len);
				memcpy( newKeyValue->m_pValue, m_pValue, len );
			}
		}
		break;
	case TYPE_INT:
		newKeyValue->m_iValue = m_iValue;
		break;
	case TYPE_UINT64:
		newKeyValue->AllocateValueBlock(sizeof(uint64));
		memcpy( newKeyValue->m_sValue, m_sValue, sizeof(uint64) );
		break;
	case TYPE_FLOAT:
		newKeyValue->m_flValue = m_flValue;
		break;
	case TYPE_PTR:
		newKeyValue->m_pValue = m_pValue;
		break;
	case TYPE_COLOR:
		newKeyValue->m_Color[0] = m_Color[0];
		newKeyValue->m_Color[1] = m_Color[1];
		newKeyValue->m_Color[2] = m_Color[2];
		newKeyValue->m_Color[3] = m_Color[3];
		break;
	};

	// recursively copy subkeys
	// Also maintain ordering....
	KeyValues *pPrev = NULL;
	for ( KeyValues *sub = m_pSub; sub != NULL; sub = sub->m_pPeer )
	{
		// take a copy of the subkey
		KeyValues *dat = sub->MakeCopy();

		// add into subkey list
		if (pPrev)
		{
			pPrev->m_pPeer = dat;
		}
		else
		{
			newKeyValue->m_pSub = dat;
		}
		dat->m_pPeer = NULL;
		pPrev = dat;
	}

	return newKeyValue;
}

//-----------------------------------------------------------------------------
// Purpose: Check if a keyName has no value assigned to it.
//-----------------------------------------------------------------------------
bool KeyValues::IsEmpty(const char *keyName)
{
	KeyValues *dat = FindKey(keyName, false);
	if (!dat)
		return true;

	if (dat->m_iDataType == TYPE_NONE && dat->m_pSub == NULL)
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Clear out all subkeys, and the current value
//-----------------------------------------------------------------------------
void KeyValues::Clear( void )
{
	KeyValues *datNext = NULL;
	for (KeyValues *dat = m_pSub; dat != NULL; dat = datNext)
	{
		datNext = dat->m_pPeer;
		delete dat;
	}
	m_pSub = NULL;

	FreeAllocatedValue();
	m_iDataType = TYPE_NONE;
}

//-----------------------------------------------------------------------------
// Purpose: Get the data type of the value stored in a keyName
//-----------------------------------------------------------------------------
KeyValues::types_t KeyValues::GetDataType(const char *keyName)
{
	KeyValues *dat = FindKey(keyName, false);
	if (dat)
		return (types_t)dat->m_iDataType;

	return TYPE_NONE;
}

//-----------------------------------------------------------------------------
// Purpose: Deletion, ensures object gets deleted from correct heap
//-----------------------------------------------------------------------------
void KeyValues::deleteThis()
{
	delete this;
}

//-----------------------------------------------------------------------------
// Purpose: memory allocator
//-----------------------------------------------------------------------------
void *KeyValues::operator new( size_t iAllocSize )
{
	// call into the standard tier0 allocator
	return keyvalues()->AllocKeyValuesMemory(iAllocSize);
}

void *KeyValues::operator new( size_t iAllocSize, int nBlockUse, const char *pFileName, int nLine )
{
	return keyvalues()->AllocKeyValuesMemory(iAllocSize);
}

//-----------------------------------------------------------------------------
// Purpose: deallocator
//-----------------------------------------------------------------------------
void KeyValues::operator delete( void *pMem )
{
	keyvalues()->FreeKeyValuesMemory(pMem);
}

void KeyValues::operator delete( void *pMem, int nBlockUse, const char *pFileName, int nLine )
{
	keyvalues()->FreeKeyValuesMemory(pMem);
}
