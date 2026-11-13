// filter.h -- ip and userid ban lists (sv_main.c)

#ifndef FILTER_H
#define FILTER_H

#define MAX_IPFILTERS	32768
#define MAX_USERFILTERS	32768

typedef struct
{
	unsigned	mask;
	unsigned	compare;
	float		banEndTime;	// 0 for permanent ban
	float		banTime;
} ipfilter_t;

typedef struct
{
	USERID_t	userid;
	float		banEndTime;
	float		banTime;
} userfilter_t;

extern	ipfilter_t		ipfilters[MAX_IPFILTERS];
extern	int				numipfilters;
extern	userfilter_t	userfilters[MAX_USERFILTERS];
extern	int				numuserfilters;

#endif // FILTER_H
