//---------------------------------------------------------------------------
//
//		S c r i p t e d   S e q u e n c e s
//
// Parser and storage of the .seq files: entries of timed commands
// (text, sounds, sentences, target firing) and sentence groups.
//---------------------------------------------------------------------------

#include "quakedef.h"
#include "mathlib.h"
#include "usercmd.h"
#include "cvardef.h"
#include "cvar.h"
#include "mem.h"
#include "APIProxy.h"
#include "tmessage.h"
#include "Sequence.h"

#include <ctype.h>

int32	RandomLong( int32 lLow, int32 lHigh );

//---------------------------------------------------------------------------
// Globals
//---------------------------------------------------------------------------
int						g_lineNum = 1;
char*					g_scan;
char*					g_lineScan;
sequenceCommandLine_s	g_fileScopeDefaults;
sequenceCommandLine_s	g_blockScopeDefaults;
sequenceEntry_s*		g_sequenceList;
sentenceGroupEntry_s*	g_sentenceGroupList;
qboolean				g_sequenceParseFileIsGlobal;
char					g_sequenceParseFileName[256];
unsigned int			g_nonGlobalSentences;

//---------------------------------------------------------------------------
// g_sequenceCommandMappingTable
//
// Maps command names to their enumerated values.
//---------------------------------------------------------------------------
static sequenceCommandMapping_s g_sequenceCommandMappingTable[] =
{
	{ SEQUENCE_COMMAND_PAUSE,			"pause",		SEQUENCE_TYPE_COMMAND },
	{ SEQUENCE_COMMAND_TEXT,			"text",			SEQUENCE_TYPE_COMMAND },
	{ SEQUENCE_COMMAND_SOUND,			"sound",		SEQUENCE_TYPE_COMMAND },
	{ SEQUENCE_COMMAND_FIRETARGETS,		"firetargets",	SEQUENCE_TYPE_COMMAND },
	{ SEQUENCE_COMMAND_KILLTARGETS,		"killtargets",	SEQUENCE_TYPE_COMMAND },
	{ SEQUENCE_COMMAND_GOSUB,			"gosub",		SEQUENCE_TYPE_COMMAND },
	{ SEQUENCE_COMMAND_SENTENCE,		"sentence",		SEQUENCE_TYPE_COMMAND },
	{ SEQUENCE_COMMAND_REPEAT,			"repeat",		SEQUENCE_TYPE_COMMAND },
	{ SEQUENCE_COMMAND_SETDEFAULTS,		"setdefaults",	SEQUENCE_TYPE_COMMAND },
	{ SEQUENCE_COMMAND_MODIFIER,		"modifier",		SEQUENCE_TYPE_COMMAND },
	{ SEQUENCE_COMMAND_POSTMODIFIER,	"postmodifier",	SEQUENCE_TYPE_COMMAND },
	{ SEQUENCE_COMMAND_NOOP,			"noop",			SEQUENCE_TYPE_COMMAND },

	{ SEQUENCE_MODIFIER_EFFECT,			"effect",		SEQUENCE_TYPE_MODIFIER },
	{ SEQUENCE_MODIFIER_POSITION,		"position",		SEQUENCE_TYPE_MODIFIER },
	{ SEQUENCE_MODIFIER_COLOR,			"color",		SEQUENCE_TYPE_MODIFIER },
	{ SEQUENCE_MODIFIER_COLOR2,			"color2",		SEQUENCE_TYPE_MODIFIER },
	{ SEQUENCE_MODIFIER_FADEIN,			"fadein",		SEQUENCE_TYPE_MODIFIER },
	{ SEQUENCE_MODIFIER_FADEOUT,		"fadeout",		SEQUENCE_TYPE_MODIFIER },
	{ SEQUENCE_MODIFIER_HOLDTIME,		"holdtime",		SEQUENCE_TYPE_MODIFIER },
	{ SEQUENCE_MODIFIER_FXTIME,			"fxtime",		SEQUENCE_TYPE_MODIFIER },
	{ SEQUENCE_MODIFIER_SPEAKER,		"speaker",		SEQUENCE_TYPE_MODIFIER },
	{ SEQUENCE_MODIFIER_LISTENER,		"listener",		SEQUENCE_TYPE_MODIFIER },
	{ SEQUENCE_MODIFIER_TEXTCHANNEL,	"channel",		SEQUENCE_TYPE_MODIFIER },
};


//---------------------------------------------------------------------------
// Sequence_FreeDataPointer
//---------------------------------------------------------------------------
void Sequence_FreeDataPointer( void *data )
{
	if ( !data )
		return;

	Mem_Free( data );
}


//---------------------------------------------------------------------------
// Sequence_GetCommandEnumForName
//
// Returns the enumerated value of a command name, or SEQUENCE_COMMAND_ERROR.
//---------------------------------------------------------------------------
sequenceCommandEnum_e Sequence_GetCommandEnumForName( const char *commandName )
{
	int i;
	int numTableEntries = sizeof( g_sequenceCommandMappingTable ) / sizeof( sequenceCommandMapping_s );

	for ( i = 0; i < numTableEntries; i++ )
	{
		if ( !Q_stricmp( commandName, g_sequenceCommandMappingTable[i].commandName ) )
			return g_sequenceCommandMappingTable[i].commandEnum;
	}

	return SEQUENCE_COMMAND_ERROR;
}


//---------------------------------------------------------------------------
// Sequence_ResetDefaults
//
// Copies the defaults from source into destination, or the built-in
// defaults when there is no source.
//---------------------------------------------------------------------------
void Sequence_ResetDefaults( sequenceCommandLine_s *destination, sequenceCommandLine_s *source )
{
	if ( !source )
	{
		destination->clientMessage.effect	= 0;
		destination->clientMessage.r1		= 255;
		destination->clientMessage.g1		= 255;
		destination->clientMessage.b1		= 255;
		destination->clientMessage.a1		= 255;
		destination->clientMessage.r2		= 255;
		destination->clientMessage.g2		= 255;
		destination->clientMessage.b2		= 255;
		destination->clientMessage.a2		= 255;
		destination->clientMessage.x		= 0.5;
		destination->clientMessage.y		= 0.5;
		destination->clientMessage.fadein	= 0.2;
		destination->clientMessage.fadeout	= 0.2;
		destination->clientMessage.holdtime	= 1.6;
		destination->clientMessage.fxtime	= 1.0;
		destination->clientMessage.pName	= NULL;
		destination->clientMessage.pMessage	= NULL;
		destination->textChannel			= 0;
		destination->delay					= 0;
		destination->repeatCount			= 0;
		destination->nextCommandLine		= NULL;
		destination->soundFileName			= NULL;
		destination->speakerName			= NULL;
		destination->listenerName			= NULL;
		return;
	}

	destination->clientMessage.effect	= source->clientMessage.effect;
	destination->clientMessage.r1		= source->clientMessage.r1;
	destination->clientMessage.g1		= source->clientMessage.g1;
	destination->clientMessage.b1		= source->clientMessage.b1;
	destination->clientMessage.a1		= source->clientMessage.a1;
	destination->clientMessage.r2		= source->clientMessage.r2;
	destination->clientMessage.g2		= source->clientMessage.g2;
	destination->clientMessage.b2		= source->clientMessage.b2;
	destination->clientMessage.a2		= source->clientMessage.a2;
	destination->clientMessage.x		= source->clientMessage.x;
	destination->clientMessage.y		= source->clientMessage.y;
	destination->clientMessage.fadein	= source->clientMessage.fadein;
	destination->clientMessage.fadeout	= source->clientMessage.fadeout;
	destination->clientMessage.holdtime	= source->clientMessage.holdtime;
	destination->clientMessage.fxtime	= source->clientMessage.fxtime;
	destination->clientMessage.pName	= NULL;
	destination->clientMessage.pMessage	= NULL;
	destination->textChannel			= source->textChannel;
	destination->delay					= source->delay;
	destination->repeatCount			= source->repeatCount;
	destination->nextCommandLine		= NULL;
	destination->soundFileName			= NULL;

	Sequence_FreeDataPointer( destination->speakerName );
	destination->speakerName = Mem_Strdup( source->speakerName );

	Sequence_FreeDataPointer( destination->listenerName );
	destination->listenerName = Mem_Strdup( source->listenerName );
}


//---------------------------------------------------------------------------
// Sequence_WriteDefaults
//
// Copies only the modifiers that source specifies into destination.
//---------------------------------------------------------------------------
void Sequence_WriteDefaults( sequenceCommandLine_s *source, sequenceCommandLine_s *destination )
{
	if ( !destination )
		Sys_Error( "Attempt to bake defaults into a non-existant command." );
	if ( !source )
		Sys_Error( "Attempt to bake defaults from a non-existant command." );

	if ( source->modifierBitField & SEQUENCE_MODIFIER_EFFECT_BIT )
	{
		destination->clientMessage.effect = source->clientMessage.effect;
	}

	if ( source->modifierBitField & SEQUENCE_MODIFIER_COLOR_BIT )
	{
		destination->clientMessage.r1 = source->clientMessage.r1;
		destination->clientMessage.g1 = source->clientMessage.g1;
		destination->clientMessage.b1 = source->clientMessage.b1;
		destination->clientMessage.a1 = source->clientMessage.a1;
	}

	if ( source->modifierBitField & SEQUENCE_MODIFIER_COLOR2_BIT )
	{
		destination->clientMessage.r2 = source->clientMessage.r2;
		destination->clientMessage.g2 = source->clientMessage.g2;
		destination->clientMessage.b2 = source->clientMessage.b2;
		destination->clientMessage.a2 = source->clientMessage.a2;
	}

	if ( source->modifierBitField & SEQUENCE_MODIFIER_POSITION_BIT )
	{
		destination->clientMessage.x = source->clientMessage.x;
		destination->clientMessage.y = source->clientMessage.y;
	}

	if ( source->modifierBitField & SEQUENCE_MODIFIER_FADEIN_BIT )
	{
		destination->clientMessage.fadein = source->clientMessage.fadein;
	}

	if ( source->modifierBitField & SEQUENCE_MODIFIER_FADEOUT_BIT )
	{
		destination->clientMessage.fadeout = source->clientMessage.fadeout;
	}

	if ( source->modifierBitField & SEQUENCE_MODIFIER_HOLDTIME_BIT )
	{
		destination->clientMessage.holdtime = source->clientMessage.holdtime;
	}

	if ( source->modifierBitField & SEQUENCE_MODIFIER_FXTIME_BIT )
	{
		destination->clientMessage.fxtime = source->clientMessage.fxtime;
	}

	if ( source->modifierBitField & SEQUENCE_MODIFIER_TEXTCHANNEL_BIT )
	{
		destination->textChannel = source->textChannel;
	}

	if ( source->modifierBitField & SEQUENCE_MODIFIER_SPEAKER_BIT )
	{
		Sequence_FreeDataPointer( destination->speakerName );
		destination->speakerName = Mem_Strdup( source->speakerName );
	}

	if ( source->modifierBitField & SEQUENCE_MODIFIER_LISTENER_BIT )
	{
		Sequence_FreeDataPointer( destination->listenerName );
		destination->listenerName = Mem_Strdup( source->listenerName );
	}
}


//---------------------------------------------------------------------------
// Sequence_BakeDefaults
//
// Copies every modifier of source into destination.
//---------------------------------------------------------------------------
void Sequence_BakeDefaults( sequenceCommandLine_s *destination, sequenceCommandLine_s *source )
{
	if ( !destination )
		Sys_Error( "Attempt to bake defaults into a non-existant command." );
	if ( !source )
		Sys_Error( "Attempt to bake defaults from a non-existant command." );

	destination->clientMessage.effect	= source->clientMessage.effect;
	destination->clientMessage.r1		= source->clientMessage.r1;
	destination->clientMessage.g1		= source->clientMessage.g1;
	destination->clientMessage.b1		= source->clientMessage.b1;
	destination->clientMessage.a1		= source->clientMessage.a1;
	destination->clientMessage.r2		= source->clientMessage.r2;
	destination->clientMessage.g2		= source->clientMessage.g2;
	destination->clientMessage.b2		= source->clientMessage.b2;
	destination->clientMessage.a2		= source->clientMessage.a2;
	destination->clientMessage.x		= source->clientMessage.x;
	destination->clientMessage.y		= source->clientMessage.y;
	destination->clientMessage.fadein	= source->clientMessage.fadein;
	destination->clientMessage.fadeout	= source->clientMessage.fadeout;
	destination->clientMessage.holdtime	= source->clientMessage.holdtime;
	destination->clientMessage.fxtime	= source->clientMessage.fxtime;
	destination->textChannel			= source->textChannel;

	Sequence_FreeDataPointer( destination->speakerName );
	destination->speakerName = Mem_Strdup( source->speakerName );

	Sequence_FreeDataPointer( destination->listenerName );
	destination->listenerName = Mem_Strdup( source->listenerName );
}


//---------------------------------------------------------------------------
// Sequence_SkipWhitespace
//
// Returns true if a carriage return was passed.
//---------------------------------------------------------------------------
qboolean Sequence_SkipWhitespace( void )
{
	qboolean passedCarriageReturn = false;

	while ( isspace( *g_scan ) )
	{
		if ( *g_scan == '\n' )
		{
			g_lineNum++;
			g_lineScan = g_scan + 1;
			passedCarriageReturn = true;
		}

		g_scan++;
	}

	return passedCarriageReturn;
}


//---------------------------------------------------------------------------
// Sequence_IsNameValueChar
//---------------------------------------------------------------------------
qboolean Sequence_IsNameValueChar( char ch )
{
	if ( isalnum( ch ) )
		return true;

	if ( ch == '.' || ch == '-' || ch == '_' || ch == '/' || ch == '\\' )
		return true;

	return false;
}


//---------------------------------------------------------------------------
// Sequence_IsSymbol
//---------------------------------------------------------------------------
qboolean Sequence_IsSymbol( char ch )
{
	switch ( ch )
	{
	case '"':
	case '#':
	case '$':
	case '%':
	case ',':
	case '=':
	case '@':
	case '{':
	case '}':
		return true;
	}

	return false;
}


//---------------------------------------------------------------------------
// Sequence_GetNameValueString
//---------------------------------------------------------------------------
void Sequence_GetNameValueString( char *token )
{
	char *write = token;

	Sequence_SkipWhitespace();

	if ( !Sequence_IsNameValueChar( *g_scan ) )
	{
		if ( *g_scan == '#' || *g_scan == '$' )
			Sys_Error( "Parsing error on line %d of %s.seq: cannot have more than one '%c' per line; '%c' must be at the beginning of the line ONLY\n", g_lineNum, g_sequenceParseFileName, *g_scan, *g_scan );
		else
			Sys_Error( "Parsing error on line %d of %s.seq: expected name/value, found illegal character '%c'\n", g_lineNum, g_sequenceParseFileName, *g_scan );
	}

	while ( Sequence_IsNameValueChar( *g_scan ) )
	{
		*write = *g_scan;
		g_scan++;
		write++;
	}

	*write = 0;
}


//---------------------------------------------------------------------------
// Sequence_GetSymbol
//---------------------------------------------------------------------------
char Sequence_GetSymbol( void )
{
	char ch;

	Sequence_SkipWhitespace();

	if ( *g_scan == 0 )
		return 0;

	ch = *g_scan;
	g_scan++;

	return ch;
}


//---------------------------------------------------------------------------
// Sequence_ValidateNameValueString
//---------------------------------------------------------------------------
void Sequence_ValidateNameValueString( char *token )
{
	char *scan;

	for ( scan = token; *scan; scan++ )
	{
		if ( !Sequence_IsNameValueChar( *scan ) )
			Sys_Error( "Parsing error on line %d of %s.seq: name/value string \"%s\" had illegal character '%c'\n", g_lineNum, g_sequenceParseFileName, token, *scan );
	}
}


//---------------------------------------------------------------------------
// Sequence_GetToken
//---------------------------------------------------------------------------
void Sequence_GetToken( char *token )
{
	Sequence_SkipWhitespace();

	if ( Sequence_IsNameValueChar( *g_scan ) )
	{
		Sequence_GetNameValueString( token );
	}
	else if ( Sequence_IsSymbol( *g_scan ) )
	{
		sprintf( token, "%c", *g_scan );
		g_scan++;
	}
	else
	{
		Sys_Error( "Parsing error on line %d of %s.seq: expected token, found '%c' instead\n", g_lineNum, g_sequenceParseFileName, *g_scan );
	}
}


//---------------------------------------------------------------------------
// Sequence_GetLine
//---------------------------------------------------------------------------
void Sequence_GetLine( char *line, int lineMaxLen )
{
	int lineLen;
	char *read;
	char *write = line;

	Sequence_SkipWhitespace();

	read = strchr( g_scan, '\n' );
	if ( !read )
		Sys_Error( "Syntax Error on line %d of %s.seq: expected sentence definition or '}', found End-Of-File!\n", g_lineNum, g_sequenceParseFileName );

	lineLen = read - g_scan;
	if ( lineLen >= lineMaxLen )
		Sys_Error( "Syntax Error on line %d of %s.seq: line was too long (was %d chars; max is %d chars)\n", g_lineNum, g_sequenceParseFileName, lineLen, lineMaxLen - 1 );

	strncpy( line, g_scan, lineLen );
	line[lineLen] = 0;
	g_scan = read;
}


//---------------------------------------------------------------------------
// Sequence_StripComments
//
// Removes // and /* */ comments in place; the line breaks inside block
// comments are kept so that the line numbers stay right.
//---------------------------------------------------------------------------
void Sequence_StripComments( char *buffer, int *pBufSize )
{
	char *eof = buffer + *pBufSize;
	char *read = buffer;
	char *write = buffer;

	while ( read < eof && *read )
	{
		if ( read[0] == '/' && read[1] == '/' )
		{
			// skip to the end of the line
			while ( *read && *read != '\n' && *read != '\r' )
			{
				read++;
			}
		}
		else if ( read[0] == '/' && read[1] == '*' )
		{
			read += 2;

			// skip to the end of the comment, keeping the line breaks
			while ( *read && read[1] )
			{
				if ( read[0] == '*' && read[1] == '/' )
				{
					read += 2;
					break;
				}

				if ( *read == '\n' || *read == '\r' )
				{
					*write = *read;
					write++;
					read++;
				}
				else
				{
					read++;
				}
			}
		}
		else
		{
			*write = *read;
			write++;
			read++;
		}
	}

	*write = 0;
}


//---------------------------------------------------------------------------
// Sequence_ReadInt
//---------------------------------------------------------------------------
int Sequence_ReadInt( void )
{
	char intString[256];

	Sequence_SkipWhitespace();
	Sequence_GetNameValueString( intString );

	return atoi( intString );
}


//---------------------------------------------------------------------------
// Sequence_ReadFloat
//---------------------------------------------------------------------------
float Sequence_ReadFloat( void )
{
	char floatString[256];

	Sequence_SkipWhitespace();
	Sequence_GetNameValueString( floatString );

	return atof( floatString );
}


//---------------------------------------------------------------------------
// Sequence_ReadString
//---------------------------------------------------------------------------
void Sequence_ReadString( char *string )
{
	Sequence_SkipWhitespace();
	Sequence_GetNameValueString( string );
}


//---------------------------------------------------------------------------
// Sequence_ReadQuotedString
//---------------------------------------------------------------------------
void Sequence_ReadQuotedString( char *string )
{
	char ch;
	char *write = string;

	Sequence_SkipWhitespace();

	ch = Sequence_GetSymbol();
	if ( ch != '\"' )
		Sys_Error( "Parsing error on or before line %d of %s.seq: expected quote (\"), found '%c' instead\n", g_lineNum, g_sequenceParseFileName, ch );

	while ( *g_scan && *g_scan != '\"' )
	{
		if ( *g_scan == '\n' )
			g_lineNum++;

		*write = *g_scan;
		g_scan++;
		write++;
	}

	*write = 0;
	g_scan++;
}


//---------------------------------------------------------------------------
// Sequence_ConfirmCarriageReturnOrSymbol
//---------------------------------------------------------------------------
qboolean Sequence_ConfirmCarriageReturnOrSymbol( char symbol )
{
	qboolean foundCarriageReturn;

	foundCarriageReturn = Sequence_SkipWhitespace();
	if ( foundCarriageReturn )
		return true;

	if ( *g_scan == symbol )
		return true;

	return false;
}


//---------------------------------------------------------------------------
// Sequence_IsCommandAModifier
//---------------------------------------------------------------------------
qboolean Sequence_IsCommandAModifier( sequenceCommandEnum_e commandEnum )
{
	int i;
	int numTableEntries = sizeof( g_sequenceCommandMappingTable ) / sizeof( sequenceCommandMapping_s );

	for ( i = 0; i < numTableEntries; i++ )
	{
		if ( g_sequenceCommandMappingTable[i].commandEnum == commandEnum )
		{
			if ( g_sequenceCommandMappingTable[i].commandType == SEQUENCE_TYPE_MODIFIER )
				return true;
			else
				return false;
		}
	}

	Sys_Error( "Internal error caused by line %d of %s.seq: unknown command enum = %d\n", g_lineNum, g_sequenceParseFileName, commandEnum );
	return false;
}


//---------------------------------------------------------------------------
// Sequence_ReadCommandData
//---------------------------------------------------------------------------
void Sequence_ReadCommandData( sequenceCommandEnum_e commandEnum, sequenceCommandLine_s *defaults )
{
	char tempString[1024];

	switch ( commandEnum )
	{
	case SEQUENCE_MODIFIER_EFFECT:
		defaults->clientMessage.effect = Sequence_ReadInt();
		defaults->modifierBitField |= SEQUENCE_MODIFIER_EFFECT_BIT;
		break;

	case SEQUENCE_MODIFIER_POSITION:
		defaults->clientMessage.x = Sequence_ReadFloat();
		defaults->clientMessage.y = Sequence_ReadFloat();
		defaults->modifierBitField |= SEQUENCE_MODIFIER_POSITION_BIT;
		break;

	case SEQUENCE_MODIFIER_COLOR:
		defaults->clientMessage.r1 = Sequence_ReadInt();
		defaults->clientMessage.g1 = Sequence_ReadInt();
		defaults->clientMessage.b1 = Sequence_ReadInt();
		defaults->clientMessage.a1 = 255;
		defaults->modifierBitField |= SEQUENCE_MODIFIER_COLOR_BIT;
		break;

	case SEQUENCE_MODIFIER_COLOR2:
		defaults->clientMessage.r2 = Sequence_ReadInt();
		defaults->clientMessage.g2 = Sequence_ReadInt();
		defaults->clientMessage.b2 = Sequence_ReadInt();
		defaults->clientMessage.a2 = 255;
		defaults->modifierBitField |= SEQUENCE_MODIFIER_COLOR2_BIT;
		break;

	case SEQUENCE_MODIFIER_FADEIN:
		defaults->clientMessage.fadein = Sequence_ReadFloat();
		defaults->modifierBitField |= SEQUENCE_MODIFIER_FADEIN_BIT;
		break;

	case SEQUENCE_MODIFIER_FADEOUT:
		defaults->clientMessage.fadeout = Sequence_ReadFloat();
		defaults->modifierBitField |= SEQUENCE_MODIFIER_FADEOUT_BIT;
		break;

	case SEQUENCE_MODIFIER_HOLDTIME:
		defaults->clientMessage.holdtime = Sequence_ReadFloat();
		defaults->modifierBitField |= SEQUENCE_MODIFIER_HOLDTIME_BIT;
		break;

	case SEQUENCE_MODIFIER_FXTIME:
		defaults->clientMessage.fxtime = Sequence_ReadFloat();
		defaults->modifierBitField |= SEQUENCE_MODIFIER_FXTIME_BIT;
		break;

	case SEQUENCE_MODIFIER_SPEAKER:
		Sequence_ReadString( tempString );
		defaults->speakerName = Mem_Strdup( tempString );
		defaults->modifierBitField |= SEQUENCE_MODIFIER_SPEAKER_BIT;
		break;

	case SEQUENCE_MODIFIER_LISTENER:
		Sequence_ReadString( tempString );
		defaults->listenerName = Mem_Strdup( tempString );
		defaults->modifierBitField |= SEQUENCE_MODIFIER_LISTENER_BIT;
		break;

	case SEQUENCE_MODIFIER_TEXTCHANNEL:
		defaults->textChannel = Sequence_ReadInt();
		defaults->modifierBitField |= SEQUENCE_MODIFIER_TEXTCHANNEL_BIT;
		break;

	case SEQUENCE_COMMAND_PAUSE:
		defaults->delay = Sequence_ReadFloat();
		break;

	case SEQUENCE_COMMAND_TEXT:
		Sequence_ReadQuotedString( tempString );
		defaults->clientMessage.pMessage = Mem_Strdup( tempString );
		break;

	case SEQUENCE_COMMAND_SOUND:
		Sequence_ReadString( tempString );
		defaults->soundFileName = Mem_Strdup( tempString );
		break;

	case SEQUENCE_COMMAND_FIRETARGETS:
		Sequence_ReadQuotedString( tempString );
		defaults->fireTargetNames = Mem_Strdup( tempString );
		break;

	case SEQUENCE_COMMAND_KILLTARGETS:
		Sequence_ReadQuotedString( tempString );
		defaults->killTargetNames = Mem_Strdup( tempString );
		break;

	case SEQUENCE_COMMAND_GOSUB:
		Sequence_ReadString( tempString );
		defaults->clientMessage.pName = Mem_Strdup( tempString );
		break;

	case SEQUENCE_COMMAND_SENTENCE:
		Sequence_ReadString( tempString );
		defaults->sentenceName = Mem_Strdup( tempString );
		break;

	case SEQUENCE_COMMAND_REPEAT:
		defaults->repeatCount = Sequence_ReadInt();
		break;

	default:
		Sys_Error( "Internal error caused by line %d of %s.seq: unknown command enum = %d\n", g_lineNum, g_sequenceParseFileName, commandEnum );
	}
}


//---------------------------------------------------------------------------
// Sequence_ParseModifier
//---------------------------------------------------------------------------
char Sequence_ParseModifier( sequenceCommandLine_s *defaults )
{
	char modifierName[256];
	char delimiter;
	sequenceCommandEnum_e modifierEnum;

	Sequence_GetNameValueString( modifierName );

	modifierEnum = Sequence_GetCommandEnumForName( modifierName );
	if ( modifierEnum == SEQUENCE_COMMAND_ERROR )
		Sys_Error( "Parsing error on line %d of %s.seq: unknown modifier \"%s\"\n", g_lineNum, g_sequenceParseFileName, modifierName );

	if ( !Sequence_IsCommandAModifier( modifierEnum ) )
		Sys_Error( "Parsing error on line %d of %s.seq: \"%s\" is a #command, not a $modifier\n", g_lineNum, g_sequenceParseFileName, modifierName );

	delimiter = Sequence_GetSymbol();
	if ( delimiter != '=' )
		Sys_Error( "Parsing error on or after line %d of %s.seq: after modifier \"%s\", expected '=', found '%c'\n", g_lineNum, g_sequenceParseFileName, modifierName, delimiter );

	Sequence_ReadCommandData( modifierEnum, defaults );

	if ( !Sequence_ConfirmCarriageReturnOrSymbol( ',' ) )
		Sys_Error( "Parsing error on line %d of %s.seq: after value(s) for modifier \"%s\", expected ',' or End-Of-Line; found '%c'\n", g_lineNum, g_sequenceParseFileName, modifierName, *g_scan );

	delimiter = Sequence_GetSymbol();
	return delimiter;
}


//---------------------------------------------------------------------------
// Sequence_AddCommandLineToEntry
//---------------------------------------------------------------------------
void Sequence_AddCommandLineToEntry( sequenceCommandLine_s *commandLine, sequenceEntry_s *entry )
{
	sequenceCommandLine_s *scan;

	if ( !entry->firstCommand )
	{
		entry->firstCommand = commandLine;
		commandLine->nextCommandLine = NULL;
		return;
	}

	for ( scan = entry->firstCommand; scan->nextCommandLine; scan = scan->nextCommandLine )
		;

	scan->nextCommandLine = commandLine;
	commandLine->nextCommandLine = NULL;
}


//---------------------------------------------------------------------------
// Sequence_ParseModifierLine
//---------------------------------------------------------------------------
char Sequence_ParseModifierLine( sequenceEntry_s *entry, int modifierType )
{
	sequenceCommandLine_s *newCommandLine = NULL;
	char delimiter = ',';

	while ( delimiter == ',' )
	{
		switch ( modifierType )
		{
		case 0:
			delimiter = Sequence_ParseModifier( &g_fileScopeDefaults );
			break;

		case 1:
			newCommandLine = Mem_ZeroMalloc( sizeof( sequenceCommandLine_s ) );
			newCommandLine->commandType = SEQUENCE_COMMAND_MODIFIER;
			Sequence_AddCommandLineToEntry( newCommandLine, entry );
			delimiter = Sequence_ParseModifier( newCommandLine );
			break;
		}
	}

	return delimiter;
}


//---------------------------------------------------------------------------
// Sequence_ParseCommand
//---------------------------------------------------------------------------
char Sequence_ParseCommand( sequenceCommandLine_s *newCommandLine )
{
	char commandName[256];
	char delimiter;
	sequenceCommandEnum_e commandEnum;
	sequenceCommandLine_s *modifierCommandLine;

	Sequence_GetNameValueString( commandName );

	commandEnum = Sequence_GetCommandEnumForName( commandName );
	if ( commandEnum == SEQUENCE_COMMAND_ERROR )
	{
		Sys_Error( "Parsing error on line %d of %s.seq: unknown command \"%s\"\n", g_lineNum, g_sequenceParseFileName, commandName );
	}
	else if ( Sequence_IsCommandAModifier( commandEnum ) )
	{
		modifierCommandLine = Mem_ZeroMalloc( sizeof( sequenceCommandLine_s ) );
		modifierCommandLine->commandType = SEQUENCE_COMMAND_POSTMODIFIER;

		while ( newCommandLine->nextCommandLine )
			newCommandLine = newCommandLine->nextCommandLine;

		newCommandLine->nextCommandLine = modifierCommandLine;
		newCommandLine = modifierCommandLine;
	}

	delimiter = Sequence_GetSymbol();
	if ( delimiter != '=' )
		Sys_Error( "Parsing error on or before line %d of %s.seq: after command \"%s\", expected '=', found '%c'\n", g_lineNum, g_sequenceParseFileName, commandName, delimiter );

	Sequence_ReadCommandData( commandEnum, newCommandLine );

	return Sequence_GetSymbol();
}


//---------------------------------------------------------------------------
// Sequence_ParseCommandLine
//---------------------------------------------------------------------------
char Sequence_ParseCommandLine( sequenceEntry_s *entry )
{
	char delimiter = ',';
	sequenceCommandLine_s *newCommandLine;

	newCommandLine = Mem_ZeroMalloc( sizeof( sequenceCommandLine_s ) );
	Sequence_ResetDefaults( newCommandLine, &g_blockScopeDefaults );
	Sequence_AddCommandLineToEntry( newCommandLine, entry );

	while ( delimiter == ',' )
	{
		delimiter = Sequence_ParseCommand( newCommandLine );
	}

	return delimiter;
}


//---------------------------------------------------------------------------
// Sequence_ParseMacro
//---------------------------------------------------------------------------
char Sequence_ParseMacro( sequenceEntry_s *entry )
{
	char delimiter = ',';
	sequenceCommandLine_s *newCommandLine;

	newCommandLine = Mem_Malloc( sizeof( sequenceCommandLine_s ) );
	Q_memset( newCommandLine, 0, sizeof( sequenceCommandLine_s ) );
	Sequence_ResetDefaults( newCommandLine, &g_blockScopeDefaults );
	Sequence_AddCommandLineToEntry( newCommandLine, entry );
	Sequence_ReadCommandData( SEQUENCE_COMMAND_GOSUB, newCommandLine );

	delimiter = Sequence_GetSymbol();
	while ( delimiter == ',' )
	{
		delimiter = Sequence_ParseCommand( newCommandLine );
	}

	return delimiter;
}


//---------------------------------------------------------------------------
// Sequence_ParseLine
//---------------------------------------------------------------------------
char Sequence_ParseLine( char startToken, sequenceEntry_s *entry )
{
	char endToken;

	if ( startToken == '$' )
		endToken = Sequence_ParseModifierLine( entry, 1 );
	else if ( startToken == '#' )
		endToken = Sequence_ParseCommandLine( entry );
	else if ( startToken == '@' )
		endToken = Sequence_ParseMacro( entry );
	else
		Sys_Error( "Parsing error on line %d of %s.seq: line must begin with either '#' (command) or '$' (modifier); found '%c'\n", g_lineNum, g_sequenceParseFileName, startToken );

	return endToken;
}


//---------------------------------------------------------------------------
// Sequence_CalcEntryDuration
//---------------------------------------------------------------------------
float Sequence_CalcEntryDuration( sequenceEntry_s *entry )
{
	float duration;
	sequenceCommandLine_s *cmd;

	duration = 0;

	for ( cmd = entry->firstCommand; cmd; cmd = cmd->nextCommandLine )
		duration += cmd->delay;

	return duration;
}


//---------------------------------------------------------------------------
// Sequence_DoesEntryContainInfiniteLoop
//---------------------------------------------------------------------------
qboolean Sequence_DoesEntryContainInfiniteLoop( sequenceEntry_s *entry )
{
	sequenceCommandLine_s *cmd;

	for ( cmd = entry->firstCommand; cmd; cmd = cmd->nextCommandLine )
	{
		if ( cmd->repeatCount < 0 )
			return true;
	}

	return false;
}


//---------------------------------------------------------------------------
// Sequence_IsEntrySafe
//
// An entry that repeats forever must take some time.
//---------------------------------------------------------------------------
qboolean Sequence_IsEntrySafe( sequenceEntry_s *entry )
{
	float duration;
	sequenceCommandLine_s *cmd;

	duration = 0;

	for ( cmd = entry->firstCommand; cmd; cmd = cmd->nextCommandLine )
	{
		duration += cmd->delay;

		if ( cmd->repeatCount < 0 )
		{
			if ( duration <= 0 )
				return false;
		}
	}

	return true;
}


//---------------------------------------------------------------------------
// Sequence_CreateDefaultsCommand
//---------------------------------------------------------------------------
void Sequence_CreateDefaultsCommand( sequenceEntry_s *entry )
{
	sequenceCommandLine_s *defaultsCommand = NULL;

	defaultsCommand = Mem_ZeroMalloc( sizeof( sequenceCommandLine_s ) );
	Sequence_ResetDefaults( defaultsCommand, &g_fileScopeDefaults );
	defaultsCommand->commandType = SEQUENCE_COMMAND_SETDEFAULTS;
	defaultsCommand->modifierBitField = 0xFF;
	Sequence_AddCommandLineToEntry( defaultsCommand, entry );
}


//---------------------------------------------------------------------------
// Sequence_ParseEntry
//---------------------------------------------------------------------------
char Sequence_ParseEntry( void )
{
	char symbol;
	char delimiter;
	char token[256];
	sequenceEntry_s *newEntry = NULL;

	Sequence_GetNameValueString( token );

	symbol = Sequence_GetSymbol();
	if ( symbol != '{' )
		Sys_Error( "Parsing error on line %d of %s.seq: expected '{' to start a\n new entry block; found '%c' instead!", g_lineNum, g_sequenceParseFileName, symbol );

	newEntry = Mem_Malloc( sizeof( sequenceEntry_s ) );
	Sequence_ResetDefaults( &g_blockScopeDefaults, &g_fileScopeDefaults );
	newEntry->entryName = Mem_Strdup( token );
	newEntry->fileName = Mem_Strdup( g_sequenceParseFileName );
	newEntry->isGlobal = g_sequenceParseFileIsGlobal;
	newEntry->firstCommand = NULL;
	Sequence_CreateDefaultsCommand( newEntry );

	delimiter = Sequence_GetSymbol();
	while ( delimiter != '}' )
	{
		delimiter = Sequence_ParseLine( delimiter, newEntry );
	}

	if ( !Sequence_IsEntrySafe( newEntry ) )
		Sys_Error( "Logic error in file %s.seq before line %d: execution of entry \"%%%s\" would cause an infinite loop!", g_sequenceParseFileName, g_lineNum, newEntry->entryName );

	newEntry->nextEntry = g_sequenceList;
	g_sequenceList = newEntry;

	delimiter = Sequence_GetSymbol();
	return delimiter;
}


//---------------------------------------------------------------------------
// Sequence_FindSentenceGroup
//---------------------------------------------------------------------------
sentenceGroupEntry_s *Sequence_FindSentenceGroup( const char *groupName )
{
	sentenceGroupEntry_s *groupEntry;

	groupEntry = g_sentenceGroupList;
	while ( groupEntry )
	{
		if ( !_stricmp( groupEntry->groupName, groupName ) )
			return groupEntry;

		groupEntry = groupEntry->nextEntry;
	}

	return NULL;
}


//---------------------------------------------------------------------------
// SequenceGetSentenceByIndex
//---------------------------------------------------------------------------
sentenceEntry_s *SequenceGetSentenceByIndex( unsigned int index )
{
	sentenceEntry_s *sentenceEntry = NULL;
	sentenceGroupEntry_s *groupEntry = g_sentenceGroupList;
	unsigned int sentenceCount = 0;

	while ( groupEntry )
	{
		if ( index < sentenceCount + groupEntry->numSentences )
		{
			sentenceEntry = groupEntry->firstSentence;
			while ( sentenceCount < index )
			{
				sentenceCount++;
				sentenceEntry = sentenceEntry->nextEntry;
			}

			return sentenceEntry;
		}

		sentenceCount += groupEntry->numSentences;
		groupEntry = groupEntry->nextEntry;
	}

	return NULL;
}


//---------------------------------------------------------------------------
// SequencePickSentence
//
// Picks a random sentence of the group.
//---------------------------------------------------------------------------
sentenceEntry_s *SequencePickSentence( const char *groupName, int pickMethod, int *picked )
{
	sentenceEntry_s *sentenceEntry = NULL;
	sentenceGroupEntry_s *groupEntry = Sequence_FindSentenceGroup( groupName );
	unsigned int pickedIndex = 0;
	unsigned int entryIndex = 0;

	g_engdstAddrs.pfnSequencePickSentence( &groupName, &pickMethod, &picked );

	if ( groupEntry )
	{
		pickedIndex = RandomLong( 0, groupEntry->numSentences - 1 );
		sentenceEntry = groupEntry->firstSentence;

		for ( entryIndex = 0; entryIndex < pickedIndex; entryIndex++ )
			sentenceEntry = sentenceEntry->nextEntry;
	}

	if ( picked )
		*picked = pickedIndex;

	return sentenceEntry;
}


//---------------------------------------------------------------------------
// Sequence_AddSentenceGroup
//---------------------------------------------------------------------------
sentenceGroupEntry_s *Sequence_AddSentenceGroup( char *groupName )
{
	sentenceGroupEntry_s *newEntry = NULL;
	sentenceGroupEntry_s *lastEntry = g_sentenceGroupList;

	newEntry = Mem_Malloc( sizeof( sentenceGroupEntry_s ) );
	newEntry->groupName = Mem_Strdup( groupName );
	newEntry->numSentences = 0;
	newEntry->firstSentence = NULL;
	newEntry->nextEntry = NULL;

	if ( lastEntry )
	{
		while ( lastEntry->nextEntry )
			lastEntry = lastEntry->nextEntry;

		lastEntry->nextEntry = newEntry;
	}
	else
	{
		g_sentenceGroupList = newEntry;
	}

	return newEntry;
}


//---------------------------------------------------------------------------
// Sequence_AddSentenceToGroup
//---------------------------------------------------------------------------
void Sequence_AddSentenceToGroup( char *groupName, char *data )
{
	sentenceEntry_s *newEntry = NULL;
	sentenceEntry_s *lastEntry = NULL;
	sentenceGroupEntry_s *groupEntry;

	groupEntry = Sequence_FindSentenceGroup( groupName );
	if ( !groupEntry )
		groupEntry = Sequence_AddSentenceGroup( groupName );

	if ( !groupEntry )
		Sys_Error( "Unable to allocate sentence group %s at line %d in file %s.seq", groupName, g_lineNum, g_sequenceParseFileName );

	newEntry = Mem_Malloc( sizeof( sentenceEntry_s ) );
	newEntry->data = Mem_Strdup( data );
	newEntry->isGlobal = g_sequenceParseFileIsGlobal;
	newEntry->index = g_nonGlobalSentences;
	newEntry->nextEntry = NULL;
	g_nonGlobalSentences++;
	groupEntry->numSentences++;

	lastEntry = groupEntry->firstSentence;
	if ( lastEntry )
	{
		while ( lastEntry->nextEntry )
			lastEntry = lastEntry->nextEntry;

		lastEntry->nextEntry = newEntry;
	}
	else
	{
		groupEntry->firstSentence = newEntry;
	}
}


//---------------------------------------------------------------------------
// Sequence_ParseSentenceLine
//
// Returns true at the end of the sentence block.
//---------------------------------------------------------------------------
qboolean Sequence_ParseSentenceLine( void )
{
	char data[1024];
	char fullgroup[64];
	char groupName[64];
	char *c = NULL;
	int lastCharacterPos = 0;

	Sequence_GetToken( fullgroup );
	if ( fullgroup[0] == '}' )
		return true;

	// strip the number off the end of the sentence name
	c = fullgroup + strlen( fullgroup ) - 1;
	while ( !isalpha( *c ) && *c != '_' )
		c--;

	c++;
	if ( c && *c )
		*c = 0;

	strcpy( groupName, fullgroup );

	Sequence_GetLine( data, sizeof( data ) );
	lastCharacterPos = strlen( data ) - 1;
	if ( data[lastCharacterPos] == '\n' || data[lastCharacterPos] == '\r' )
		data[lastCharacterPos] = 0;

	Sequence_AddSentenceToGroup( groupName, data );
	return false;
}


//---------------------------------------------------------------------------
// Sequence_ParseSentenceBlock
//---------------------------------------------------------------------------
char Sequence_ParseSentenceBlock( void )
{
	char symbol;
	qboolean finished = false;

	symbol = Sequence_GetSymbol();
	if ( symbol != '{' )
		Sys_Error( "Parsing error on line %d of %s.seq: expected '{' to start a\n new sentence block; found '%c' instead!", g_lineNum, g_sequenceParseFileName, symbol );

	while ( !finished )
	{
		finished = Sequence_ParseSentenceLine();
	}

	return Sequence_GetSymbol();
}


//---------------------------------------------------------------------------
// Sequence_ParseGlobalDataBlock
//---------------------------------------------------------------------------
char Sequence_ParseGlobalDataBlock( void )
{
	char token[256];

	Sequence_GetNameValueString( token );

	if ( !_stricmp( token, "Sentences" ) )
		return Sequence_ParseSentenceBlock();

	Sys_Error( "Syntax error in file %s.seq on line %d: found global data block symbol '!' with unknown data type \"%s\"", g_sequenceParseFileName, g_lineNum, token );
	return 0;
}


//---------------------------------------------------------------------------
// Sequence_GetEntryForName
//---------------------------------------------------------------------------
sequenceEntry_s *Sequence_GetEntryForName( const char *entryName )
{
	sequenceEntry_s *scan;

	for ( scan = g_sequenceList; scan; scan = scan->nextEntry )
	{
		if ( !Q_stricmp( entryName, scan->entryName ) )
			return scan;
	}

	return NULL;
}


//---------------------------------------------------------------------------
// Sequence_CopyCommand
//---------------------------------------------------------------------------
sequenceCommandLine_s *Sequence_CopyCommand( sequenceCommandLine_s *commandOrig )
{
	sequenceCommandLine_s *commandCopy = NULL;

	commandCopy = Mem_ZeroMalloc( sizeof( sequenceCommandLine_s ) );

	commandCopy->nextCommandLine	= NULL;
	commandCopy->commandType		= commandOrig->commandType;
	commandCopy->modifierBitField	= commandOrig->modifierBitField;
	commandCopy->repeatCount		= commandOrig->repeatCount;
	commandCopy->delay				= commandOrig->delay;
	commandCopy->textChannel		= commandOrig->textChannel;
	commandCopy->fireTargetNames	= Mem_Strdup( commandOrig->fireTargetNames );
	commandCopy->killTargetNames	= Mem_Strdup( commandOrig->killTargetNames );
	commandCopy->listenerName		= Mem_Strdup( commandOrig->listenerName );
	commandCopy->sentenceName		= Mem_Strdup( commandOrig->sentenceName );
	commandCopy->soundFileName		= Mem_Strdup( commandOrig->soundFileName );
	commandCopy->speakerName		= Mem_Strdup( commandOrig->speakerName );

	commandCopy->clientMessage.effect	= commandOrig->clientMessage.effect;
	commandCopy->clientMessage.a1		= commandOrig->clientMessage.a1;
	commandCopy->clientMessage.b1		= commandOrig->clientMessage.b1;
	commandCopy->clientMessage.a2		= commandOrig->clientMessage.a2;
	commandCopy->clientMessage.b2		= commandOrig->clientMessage.b2;
	commandCopy->clientMessage.fadein	= commandOrig->clientMessage.fadein;
	commandCopy->clientMessage.fadeout	= commandOrig->clientMessage.fadeout;
	commandCopy->clientMessage.fxtime	= commandOrig->clientMessage.fxtime;
	commandCopy->clientMessage.g1		= commandOrig->clientMessage.g1;
	commandCopy->clientMessage.g2		= commandOrig->clientMessage.g2;
	commandCopy->clientMessage.holdtime	= commandOrig->clientMessage.holdtime;
	commandCopy->clientMessage.r1		= commandOrig->clientMessage.r1;
	commandCopy->clientMessage.r2		= commandOrig->clientMessage.r2;
	commandCopy->clientMessage.x		= commandOrig->clientMessage.x;
	commandCopy->clientMessage.y		= commandOrig->clientMessage.y;
	commandCopy->clientMessage.pMessage	= Mem_Strdup( commandOrig->clientMessage.pMessage );
	commandCopy->clientMessage.pName	= Mem_Strdup( commandOrig->clientMessage.pName );

	return commandCopy;
}


//---------------------------------------------------------------------------
// Sequence_CopyCommandList
//
// Copies a command list, leaving out the setdefaults commands.
//---------------------------------------------------------------------------
sequenceCommandLine_s *Sequence_CopyCommandList( sequenceCommandLine_s *list )
{
	sequenceCommandLine_s *scan;
	sequenceCommandLine_s *copyCommand = NULL;
	sequenceCommandLine_s *newCommandList = NULL;
	sequenceCommandLine_s *prevCommand = NULL;

	for ( scan = list; scan; scan = scan->nextCommandLine )
	{
		if ( scan->commandType == SEQUENCE_COMMAND_SETDEFAULTS )
			continue;

		copyCommand = Sequence_CopyCommand( scan );

		if ( prevCommand )
			prevCommand->nextCommandLine = copyCommand;
		else
			newCommandList = copyCommand;

		prevCommand = copyCommand;
	}

	return newCommandList;
}


//---------------------------------------------------------------------------
// Sequence_ExpandGosubsForEntry
//
// Replaces each gosub of the entry by a copy of the commands of the gosub'd
// entry. Returns true when the entry had no gosubs left.
//---------------------------------------------------------------------------
qboolean Sequence_ExpandGosubsForEntry( sequenceEntry_s *entry )
{
	sequenceCommandLine_s *cmd;
	qboolean foundGosubs = false;

	for ( cmd = entry->firstCommand; cmd; cmd = cmd->nextCommandLine )
	{
		if ( cmd->clientMessage.pName )
		{
			sequenceEntry_s *gosubEntry;
			sequenceCommandLine_s *copyList;
			sequenceCommandLine_s *scan;

			if ( !_stricmp( cmd->clientMessage.pName, entry->entryName ) )
				Sys_Error( "Error in %s.seq: entry \"%s\" gosubs itself!\n", entry->fileName, entry->entryName );

			gosubEntry = Sequence_GetEntryForName( cmd->clientMessage.pName );
			if ( !gosubEntry )
				Sys_Error( "Error in %s.seq: Gosub in entry \"%s\" specified unknown entry \"%s\"\n", entry->fileName, entry->entryName, cmd->clientMessage.pName );

			foundGosubs = true;

			copyList = Sequence_CopyCommandList( gosubEntry->firstCommand );
			if ( copyList )
			{
				for ( scan = copyList; scan->nextCommandLine; scan = scan->nextCommandLine )
					;

				scan->nextCommandLine = cmd->nextCommandLine;
				cmd->nextCommandLine = copyList;

				Mem_Free( cmd->clientMessage.pName );
				cmd->clientMessage.pName = NULL;

				cmd = scan;
			}
			else
			{
				Mem_Free( cmd->clientMessage.pName );
				cmd->clientMessage.pName = NULL;
			}
		}
	}

	return !foundGosubs;
}


//---------------------------------------------------------------------------
// Sequence_ExpandAllGosubs
//---------------------------------------------------------------------------
void Sequence_ExpandAllGosubs( void )
{
	qboolean areGosubsRemaining = true;
	sequenceEntry_s *scan;

	while ( areGosubsRemaining )
	{
		areGosubsRemaining = false;

		for ( scan = g_sequenceList; scan; scan = scan->nextEntry )
		{
			qboolean isComplete;

			isComplete = Sequence_ExpandGosubsForEntry( scan );
			if ( !isComplete )
				areGosubsRemaining = true;
		}
	}
}


//---------------------------------------------------------------------------
// Sequence_FlattenEntry
//
// Applies the defaults and modifiers to the commands of the entry.
//---------------------------------------------------------------------------
void Sequence_FlattenEntry( sequenceEntry_s *entry )
{
	sequenceCommandLine_s *cmd;
	sequenceCommandLine_s *lastCmd;

	for ( cmd = entry->firstCommand; cmd; cmd = cmd->nextCommandLine )
	{
		switch ( cmd->commandType )
		{
		case SEQUENCE_COMMAND_SETDEFAULTS:
			Sequence_WriteDefaults( cmd, &g_blockScopeDefaults );
			cmd->commandType = SEQUENCE_COMMAND_NOOP;
			break;

		case SEQUENCE_COMMAND_MODIFIER:
			Sequence_WriteDefaults( cmd, &g_blockScopeDefaults );
			break;

		case SEQUENCE_COMMAND_POSTMODIFIER:
			Sequence_WriteDefaults( cmd, lastCmd );
			break;

		default:
			Sequence_BakeDefaults( cmd, &g_blockScopeDefaults );
			lastCmd = cmd;
		}
	}
}


//---------------------------------------------------------------------------
// Sequence_FlattenAllEntries
//---------------------------------------------------------------------------
void Sequence_FlattenAllEntries( void )
{
	sequenceEntry_s *entry;

	for ( entry = g_sequenceList; entry; entry = entry->nextEntry )
		Sequence_FlattenEntry( entry );
}


//---------------------------------------------------------------------------
// Sequence_ParseBuffer
//---------------------------------------------------------------------------
void Sequence_ParseBuffer( byte *buffer, int bufferSize )
{
	char symbol;

	g_lineNum = 1;
	g_scan = (char *)buffer;
	g_lineScan = (char *)buffer;

	Sequence_StripComments( (char *)buffer, &bufferSize );
	Sequence_ResetDefaults( &g_fileScopeDefaults, NULL );

	symbol = Sequence_GetSymbol();
	while ( symbol )
	{
		if ( symbol == '$' )
			symbol = Sequence_ParseModifierLine( NULL, 0 );
		else if ( symbol == '%' )
			symbol = Sequence_ParseEntry();
		else if ( symbol == '!' )
			symbol = Sequence_ParseGlobalDataBlock();
		else
			Sys_Error( "Parsing error on line %d of %s.seq: At file scope, lines must begin with '$' (modifier) or '%' (entry block) or '!' (sentence / global data block); found '%c'\n", g_lineNum, g_sequenceParseFileName, symbol );
	}

	Sequence_ExpandAllGosubs();
	Sequence_FlattenAllEntries();
}


//---------------------------------------------------------------------------
// Sequence_ParseFile
//---------------------------------------------------------------------------
void Sequence_ParseFile( const char *fileName, qboolean isGlobal )
{
	byte *buffer;
	int bufSize;
	char filePathName[256];

	Q_strcpy( g_sequenceParseFileName, fileName );
	g_sequenceParseFileIsGlobal = isGlobal;

	sprintf( filePathName, "sequences/%s.seq", fileName );
	buffer = COM_LoadTempFile( filePathName, &bufSize );
	if ( !buffer )
	{
		Con_DPrintf( "WARNING: failed to locate sequence file %s\n", fileName );
		return;
	}

	Sequence_ParseBuffer( buffer, bufSize );
}


//---------------------------------------------------------------------------
// Sequence_Init
//---------------------------------------------------------------------------
void Sequence_Init( void )
{
	Sequence_ParseFile( "global", true );
}


//---------------------------------------------------------------------------
// SequenceGet
//---------------------------------------------------------------------------
sequenceEntry_s *SequenceGet( const char *fileName, const char *entryName )
{
	sequenceEntry_s *scan;

	g_engdstAddrs.pfnSequenceGet( &fileName, &entryName );

	for ( scan = g_sequenceList; scan; scan = scan->nextEntry )
	{
		if ( !Q_stricmp( entryName, scan->entryName ) )
			return scan;
	}

	return NULL;
}


//---------------------------------------------------------------------------
// Sequence_FreeCommand
//---------------------------------------------------------------------------
void Sequence_FreeCommand( sequenceCommandLine_s *kill )
{
	Sequence_FreeDataPointer( kill->fireTargetNames );
	Sequence_FreeDataPointer( kill->speakerName );
	Sequence_FreeDataPointer( kill->listenerName );
	Sequence_FreeDataPointer( kill->soundFileName );
	Sequence_FreeDataPointer( kill->sentenceName );
	Sequence_FreeDataPointer( kill->clientMessage.pName );
	Sequence_FreeDataPointer( kill->clientMessage.pMessage );
}


//---------------------------------------------------------------------------
// Sequence_FreeEntry
//---------------------------------------------------------------------------
void Sequence_FreeEntry( sequenceEntry_s *kill )
{
	Sequence_FreeDataPointer( kill->entryName );
	Sequence_FreeDataPointer( kill->fileName );

	while ( kill->firstCommand )
	{
		sequenceCommandLine_s *dead;

		dead = kill->firstCommand;
		kill->firstCommand = kill->firstCommand->nextCommandLine;
		Sequence_FreeCommand( dead );
	}

	Sequence_FreeDataPointer( kill );
}


//---------------------------------------------------------------------------
// Sequence_FreeSentence
//---------------------------------------------------------------------------
void Sequence_FreeSentence( sentenceEntry_s *sentenceEntry )
{
	Sequence_FreeDataPointer( sentenceEntry->data );
	Sequence_FreeDataPointer( sentenceEntry );
}


//---------------------------------------------------------------------------
// Sequence_FreeSentenceGroup
//---------------------------------------------------------------------------
void Sequence_FreeSentenceGroup( sentenceGroupEntry_s *groupEntry )
{
	Sequence_FreeDataPointer( groupEntry->groupName );
	Sequence_FreeDataPointer( groupEntry );
}


//---------------------------------------------------------------------------
// Sequence_FreeSentenceGroupEntries
//---------------------------------------------------------------------------
void Sequence_FreeSentenceGroupEntries( sentenceGroupEntry_s *groupEntry, qboolean purgeGlobals )
{
	sentenceEntry_s *sentenceEntry = NULL;
	sentenceEntry_s *deadSentence = NULL;
	sentenceEntry_s *prevSentence = NULL;

	sentenceEntry = groupEntry->firstSentence;
	while ( sentenceEntry )
	{
		if ( sentenceEntry->isGlobal && !purgeGlobals )
		{
			prevSentence = sentenceEntry;
			sentenceEntry = sentenceEntry->nextEntry;
			continue;
		}

		if ( prevSentence )
			prevSentence->nextEntry = sentenceEntry->nextEntry;
		else
			groupEntry->firstSentence = sentenceEntry->nextEntry;

		groupEntry->numSentences--;
		g_nonGlobalSentences--;

		deadSentence = sentenceEntry;
		sentenceEntry = sentenceEntry->nextEntry;
		Sequence_FreeSentence( deadSentence );
	}
}


//---------------------------------------------------------------------------
// Sequence_PurgeEntries
//---------------------------------------------------------------------------
void Sequence_PurgeEntries( qboolean purgeGlobals )
{
	sequenceEntry_s *scan;
	sequenceEntry_s *dead;
	sequenceEntry_s *prev = NULL;
	sentenceGroupEntry_s *groupEntry = NULL;
	sentenceGroupEntry_s *deadGroup = NULL;
	sentenceGroupEntry_s *prevGroup = NULL;

	scan = g_sequenceList;
	while ( scan )
	{
		if ( scan->isGlobal && !purgeGlobals )
		{
			prev = scan;
			scan = scan->nextEntry;
			continue;
		}

		if ( prev )
			prev->nextEntry = scan->nextEntry;
		else
			g_sequenceList = scan->nextEntry;

		dead = scan;
		scan = scan->nextEntry;
		Sequence_FreeEntry( dead );
	}

	groupEntry = g_sentenceGroupList;
	while ( groupEntry )
	{
		Sequence_FreeSentenceGroupEntries( groupEntry, purgeGlobals );

		if ( groupEntry->numSentences )
		{
			prevGroup = groupEntry;
			groupEntry = groupEntry->nextEntry;
			continue;
		}

		if ( prevGroup )
			prevGroup->nextEntry = groupEntry->nextEntry;
		else
			g_sentenceGroupList = groupEntry->nextEntry;

		deadGroup = groupEntry;
		groupEntry = groupEntry->nextEntry;
		Sequence_FreeSentenceGroup( deadGroup );
	}
}


//---------------------------------------------------------------------------
// Sequence_OnLevelLoad
//
// Purges the entries of the previous level and loads the sequence file of
// the new one.
//---------------------------------------------------------------------------
void Sequence_OnLevelLoad( const char *mapName )
{
	Sequence_PurgeEntries( false );
	Sequence_ParseFile( mapName, false );
}
