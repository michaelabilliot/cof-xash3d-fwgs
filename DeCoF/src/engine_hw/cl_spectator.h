// cl_spectator.h -- the developer overview mode

#ifndef CL_SPECTATOR_H
#define CL_SPECTATOR_H

typedef struct overviewInfo_s
{
	vec3_t		origin;
	float		z_min, z_max;
	float		zoom;
	qboolean	rotated;
} overviewInfo_t;

extern	overviewInfo_t	gDevOverview;
extern	cvar_t			dev_overview;

int		CL_IsDevOverviewMode( void );

#endif // CL_SPECTATOR_H
