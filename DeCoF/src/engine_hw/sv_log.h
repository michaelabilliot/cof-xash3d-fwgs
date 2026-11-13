// sv_log.h -- server logging to a file and to network listeners (sv_log.c)

#ifndef SV_LOG_H
#define SV_LOG_H

extern	cvar_t	mp_logfile;
extern	cvar_t	mp_logecho;
extern	cvar_t	sv_log_onefile;
extern	cvar_t	sv_log_singleplayer;
extern	cvar_t	sv_logsecret;

void	Log_Printf( char *fmt, ... );
void	Log_PrintServerVars( void );
void	Log_Close( void );
void	Log_Open( void );
void	SV_SetLogAddress_f( void );
void	SV_AddLogAddress_f( void );
void	SV_DelLogAddress_f( void );
void	SV_ServerLog_f( void );

#endif // SV_LOG_H
