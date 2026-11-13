// prefs_guess.c -- saves the configuration (no Linux file and no string names this
// object: it holds one function that nothing calls, between pr_edict.c and qgl.c)

#include "quakedef.h"

void SaveConfig_guess( void )
{
	Host_WriteConfiguration();
}
