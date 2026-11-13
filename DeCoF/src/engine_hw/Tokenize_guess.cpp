// Tokenize_guess.cpp -- splits a text buffer into lines of tokens (one TokenLine per line)
//
// Only the class name comes from the binary (RTTI .?AVTokenize@@); nothing in the
// engine calls it.

#include <stdlib.h>
#include <string.h>

#include "UtlVector.h"
#include "TokenLine.h"

class Tokenize
{
public:
	Tokenize();
	virtual ~Tokenize();

	int			SetBuffer_guess( const char *buffer );
	int			LineCount_guess();
	TokenLine	*GetLine_guess( int line );
	int			TokenCount_guess( int line );
	char		*GetToken_guess( int line, int token );
	TokenLine	*Line_guess( int line );
	TokenLine	*LineNoCheck_guess( int line );

	int			IsValidLine_guess( int line ) { return line < m_nLines_guess; }

private:
	int							m_nLines_guess;
	CUtlVector< TokenLine * >	m_Lines_guess;
};

Tokenize::Tokenize()
{
	m_nLines_guess = 0;
}

Tokenize::~Tokenize()
{
	for ( int i = 0; i < m_Lines_guess.Count(); i++ )
	{
		free( m_Lines_guess[ i ] );
	}
}

int Tokenize::SetBuffer_guess( const char *buffer )
{
	char *copy_guess = (char *)malloc( strlen( buffer ) + 1 );
	if ( !copy_guess )
		return -1;

	strcpy( copy_guess, buffer );

	char *line_guess = copy_guess;
	m_nLines_guess = 0;

	while ( line_guess && strlen( line_guess ) > 0 )
	{
		char *next_guess = strchr( line_guess, '\n' );
		if ( next_guess )
		{
			*next_guess = '\0';
			next_guess++;
		}

		TokenLine *tokenLine_guess = new TokenLine( line_guess );
		m_Lines_guess.AddToTail( tokenLine_guess );
		m_nLines_guess++;

		line_guess = next_guess;
	}

	free( copy_guess );
	return 0;
}

int Tokenize::LineCount_guess()
{
	return m_nLines_guess;
}

TokenLine *Tokenize::GetLine_guess( int line )
{
	if ( IsValidLine_guess( line ) )
		return m_Lines_guess[ line ];

	return NULL;
}

int Tokenize::TokenCount_guess( int line )
{
	if ( IsValidLine_guess( line ) )
		return m_Lines_guess[ line ]->CountToken();

	return 0;
}

char *Tokenize::GetToken_guess( int line, int token )
{
	if ( IsValidLine_guess( line ) )
	{
		if ( m_Lines_guess[ line ]->CountToken() > token )
			return m_Lines_guess[ line ]->GetToken( token );

		return NULL;
	}

	return NULL;
}

TokenLine *Tokenize::Line_guess( int line )
{
	return m_Lines_guess[ line ];
}

TokenLine *Tokenize::LineNoCheck_guess( int line )
{
	return m_Lines_guess[ line ];
}
