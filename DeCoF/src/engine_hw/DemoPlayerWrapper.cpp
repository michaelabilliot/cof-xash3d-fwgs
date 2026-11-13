// DemoPlayerWrapper.cpp -- C wrappers around the HLTV demo player module

extern "C"
{
#include "quakedef.h"
#include "vgui_int.h"

void	VGuiWrap2_ShowDemoPlayer( void );
}

#include "interface.h"
#include "SystemWrapper.h"
#include "IDemoPlayer.h"

static IDemoPlayer	*s_DemoPlayer;
static int			s_DemoPlayerMode;

extern "C"
{

int DemoPlayer_Init( void )
{
	s_DemoPlayer = dynamic_cast< IDemoPlayer * >( gSystemWrapper.GetModule( DEMOPLAYER_INTERFACE_VERSION, "demoplayer" ) );
	s_DemoPlayerMode = 0;

	if ( !s_DemoPlayer )
	{
		Con_DPrintf( "Failed to load demo player module.\n" );
		return 0;
	}

	s_DemoPlayer->SetEditMode( COM_CheckParm( "-demoedit" ) != 0 );
	return 1;
}

void DemoPlayer_SetActive( int state )
{
	s_DemoPlayerMode = state;
}

int DemoPlayer_IsActive( void )
{
	return ( s_DemoPlayer && s_DemoPlayerMode );
}

int DemoPlayer_LoadGame( char *filename )
{
	if ( !s_DemoPlayer )
		return 0;

	return s_DemoPlayer->LoadGame( filename ) ? true : false;
}

void DemoPlayer_SetEditMode( int state )
{
	if ( !s_DemoPlayer )
		return;

	s_DemoPlayer->SetEditMode( state != 0 );
}

int DemoPlayer_ReadDemoMessage( unsigned char *buffer, int size )
{
	if ( !s_DemoPlayer )
		return 0;

	return s_DemoPlayer->ReadDemoMessage( buffer, size );
}

void DemoPlayer_GetDemoViewInfo( ref_params_t *rp, float *view, int *viewmodel )
{
	if ( !s_DemoPlayer )
		return;

	s_DemoPlayer->GetDemoViewInfo( rp, view, viewmodel );
}

void DemoPlayer_Stop( void )
{
	if ( !s_DemoPlayer )
		return;

	s_DemoPlayer->Stop();
}

void DemoPlayer_ShowGUI( void )
{
	if ( !s_DemoPlayer )
		return;

	VGuiWrap2_ShowDemoPlayer();
}

void DemoPlayer_ReadNetchanState( int *incoming_sequence, int *incoming_acknowledged, int *incoming_reliable_acknowledged, int *incoming_reliable_sequence, int *outgoing_sequence, int *reliable_sequence, int *last_reliable_sequence )
{
	if ( !s_DemoPlayer )
		return;

	s_DemoPlayer->ReadNetchanState( incoming_sequence, incoming_acknowledged, incoming_reliable_acknowledged, incoming_reliable_sequence, outgoing_sequence, reliable_sequence, last_reliable_sequence );
}

}
