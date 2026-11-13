// traceinit.h -- tracks the pairing of subsystem init and shutdown calls

#ifndef TRACEINIT_H
#define TRACEINIT_H

void	TraceInit( const char *i, const char *s, int listnum );
void	TraceShutdown( const char *s, int listnum );

#endif // TRACEINIT_H
