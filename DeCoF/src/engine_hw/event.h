// event.h -- precached event scripts (cl.event_precache, sv.event_precache)

#ifndef EVENT_H
#define EVENT_H

#define MAX_EVENTS	256

typedef struct event_s
{
	unsigned short	index;
	const char		*filename;
	int				filesize;
	const char		*pszScript;
} event_t;

#endif // EVENT_H
