// usermsg.h -- user messages registered by the game dll (sv_main.c)

#ifndef USERMSG_H
#define USERMSG_H

#include "APIProxy.h"

typedef struct _UserMsg
{
	int		iMsg;
	int		iSize;
	char	szName[16];
	struct _UserMsg	*next;
	pfnUserMsgHook	pfn;
} UserMsg;

extern	UserMsg	*sv_gpNewUserMsgs;
extern	UserMsg	*sv_gpUserMsgs;

#endif // USERMSG_H
