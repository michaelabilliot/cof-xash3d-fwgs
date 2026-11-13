// delta_packet.h -- the entities of one delta compressed packet

#ifndef DELTA_PACKET_H
#define DELTA_PACKET_H

typedef struct packet_entities_s
{
	int						num_entities;

	unsigned char			flags[ 32 ];

	struct entity_state_s	*entities;
} packet_entities_t;

extern	int		SV_UPDATE_BACKUP;
extern	int		SV_UPDATE_MASK;
extern	int		CL_UPDATE_BACKUP;
extern	int		CL_UPDATE_MASK;

#endif // DELTA_PACKET_H
