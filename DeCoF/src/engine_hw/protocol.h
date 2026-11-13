// protocol.h -- communications protocols

#ifndef PROTOCOL_H
#define PROTOCOL_H

#define	PROTOCOL_VERSION	48

//
// server to client
//
#define	svc_bad				0
#define	svc_nop				1
#define	svc_disconnect		2
#define	svc_event			3
#define	svc_version			4	// [long] server version
#define	svc_setview			5	// [short] entity number
#define	svc_sound			6	// <see code>
#define	svc_time			7	// [float] server time
#define	svc_print			8	// [string] null terminated string
#define	svc_stufftext		9	// [string] stuffed into client's console buffer
#define	svc_setangle		10	// [angle angle angle] set the view angle to this absolute value
#define	svc_serverinfo		11	// [long] version ...
#define	svc_lightstyle		12	// [byte] [string]
#define	svc_updateuserinfo	13	// [byte] slot [long] uid [string] userinfo
#define	svc_deltadescription	14
#define	svc_clientdata		15	// <shortbits + data>
#define	svc_stopsound		16	// <see code>
#define	svc_pings			17
#define	svc_particle		18	// [vec3] <variable>
#define	svc_damage			19
#define	svc_spawnstatic		20
#define	svc_event_reliable	21
#define	svc_spawnbaseline	22
#define	svc_temp_entity		23
#define	svc_setpause		24	// [byte] on / off
#define	svc_signonnum		25	// [byte]  used for the signon sequence
#define	svc_centerprint		26	// [string] to put in center of the screen
#define	svc_killedmonster	27
#define	svc_foundsecret		28
#define	svc_spawnstaticsound	29	// [coord3] [byte] samp [byte] vol [byte] aten
#define	svc_intermission	30	// [string] music
#define	svc_finale			31	// [string] music [string] text
#define	svc_cdtrack			32	// [byte] track [byte] looptrack
#define	svc_restore			33
#define	svc_cutscene		34
#define	svc_weaponanim		35
#define	svc_decalname		36	// [byte] index [string] name
#define	svc_roomtype		37	// [byte] roomtype (dsp effect)
#define	svc_addangle		38	// [angle] add angles when client turns
#define	svc_newusermsg		39
#define	svc_packetentities	40	// [...]  Non-delta compressed entities
#define	svc_deltapacketentities	41	// [...]  Delta compressed entities
#define	svc_choke			42	// # of packets held back on channel because too much data was flowing.
#define	svc_resourcelist	43
#define	svc_newmovevars		44
#define	svc_resourcerequest	45
#define	svc_customization	46
#define	svc_crosshairangle	47	// [char] pitch * 5 [char] yaw * 5
#define	svc_soundfade		48	// char percent, char holdtime, char fadeouttime, char fadeintime
#define	svc_filetxferfailed	49
#define	svc_hltv			50
#define	svc_director		51
#define	svc_voiceinit		52	// Initialize voice stuff.
#define	svc_voicedata		53	// Voicestream data from the server
#define	svc_sendextrainfo	54
#define	svc_timescale		55
#define	svc_resourcelocation	56
#define	svc_sendcvarvalue	57
#define	svc_sendcvarvalue2	58

#endif // PROTOCOL_H
