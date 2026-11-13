// tmessage.h -- titles.txt text messages (tmessage.c)

#ifndef TMESSAGE_H
#define TMESSAGE_H

#define MAX_NETMESSAGE	4

typedef struct client_textmessage_s
{
	int		effect;
	byte	r1, g1, b1, a1;		// 2 colors for effects
	byte	r2, g2, b2, a2;
	float	x;
	float	y;
	float	fadein;
	float	fadeout;
	float	holdtime;
	float	fxtime;
	const char *pName;
	const char *pMessage;
} client_textmessage_t;

extern	client_textmessage_t	gNetworkTextMessage[MAX_NETMESSAGE];
extern	char		gNetworkTextMessageBuffer[MAX_NETMESSAGE][512];
extern	const char	*gNetworkMessageNames[MAX_NETMESSAGE];

void	TextMessageInit( void );
void	TextMessageShutdown( void );
void	TrimSpace( const char *source, char *dest );

#endif // TMESSAGE_H
