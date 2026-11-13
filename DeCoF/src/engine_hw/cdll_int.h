// cdll_int.h -- the engine's calls into the client dll (cdll_int.c)

#ifndef CDLL_INT_H
#define CDLL_INT_H

void	ClientDLL_ChatInputPosition( int *x, int *y );
int		ClientDLL_Key_Event( int down, int keynum, const char *pszCurrentBinding );

extern	struct modfuncs_s	g_modfuncs;

struct netadr_s;
struct usercmd_s;

void	ClientDLL_HudInit( void );
void	ClientDLL_HudVidInit( void );
void	ClientDLL_IN_Accumulate( void );
void	ClientDLL_CreateMove( float frametime, struct usercmd_s *cmd, int active );
int		ClientDLL_ConnectionlessPacket( const struct netadr_s *net_from, const char *args, char *response_buffer, int *response_buffer_size );
int		ClientDLL_GetHullBounds( int hullnumber, float *mins, float *maxs );

struct local_state_s;
struct playermove_s;

void	ClientDLL_PostRunCmd( struct local_state_s *from, struct local_state_s *to, struct usercmd_s *cmd, int runfuncs, double time, unsigned int random_seed );
void	ClientDLL_MoveClient( struct playermove_s *ppmove );
void	ClientDLL_DirectorMessage( int iSize, void *pbuf );
struct entity_state_s;
struct clientdata_s;
struct weapon_data_s;
struct cl_entity_s;
int		ClientDLL_AddEntity( int type, struct cl_entity_s *ent );
struct ref_params_s;
void	ClientDLL_CalcRefdef( struct ref_params_s *pparams );
void	ClientDLL_CreateEntities( void );
struct tempent_s;
void	ClientDLL_TempEntUpdate( double ft, double ct, double grav, struct tempent_s **ppFreeTE, struct tempent_s **ppActiveTE, int ( *addTEntity )( struct cl_entity_s *pEntity ), void ( *playTESound )( struct tempent_s *pTemp, float damp ) );
int		ClientDLL_IsThirdPerson( void );
void	ClientDLL_TxferLocalOverrides( struct entity_state_s *state, const struct clientdata_s *client );
void	ClientDLL_ProcessPlayerState( struct entity_state_s *dst, const struct entity_state_s *src );
void	ClientDLL_TxferPredictionData( struct entity_state_s *ps, const struct entity_state_s *pps, struct clientdata_s *pcd, const struct clientdata_s *ppcd, struct weapon_data_s *wd, const struct weapon_data_s *pwd );

// module.cpp
typedef struct module_s
{
	unsigned char	ucMD5Hash[16];	// hash over code
	qboolean		fLoaded;		// true if successfully loaded
} module_t;

extern	module_t	g_module;

void	CloseSecurityModule( void );
void	Net_APICheckTimeouts( void );

#endif // CDLL_INT_H
