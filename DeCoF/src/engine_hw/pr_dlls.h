// pr_dlls.h -- the game dll and its exported entity functions (pr_edict.c)

#ifndef PR_DLLS_H
#define PR_DLLS_H

typedef void (*ENTITYINIT)( struct entvars_s * );

ENTITYINIT	GetEntityInit( char *pClassName );

#endif // PR_DLLS_H
