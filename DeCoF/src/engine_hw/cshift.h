// cshift.h -- the color shift of the screen (water, contents, damage)

#ifndef CSHIFT_H
#define CSHIFT_H

typedef struct
{
	int		destcolor[3];
	int		percent;		// 0-256
} cshift_t;

extern	cshift_t	cshift_water;

#endif // CSHIFT_H
