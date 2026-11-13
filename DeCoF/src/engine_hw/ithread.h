// ithread.h -- the worker thread that runs the CD audio commands

#ifndef ITHREAD_H
#define ITHREAD_H

class CCDAudio;

typedef void ( CCDAudio::*vfunc )( int param1, int param2 );

class IThread
{
public:
	virtual			~IThread() {}

	virtual bool	Init( void ) = 0;
	virtual bool	Shutdown( void ) = 0;
	virtual bool	AddThreadItem( vfunc pfn, int param1, int param2 ) = 0;
};

extern IThread *thread;

#endif // ITHREAD_H
