/*
===========================================================================
Q3Doom - Linux: UDP networking over BSD sockets (the counterpart of
win32/win_net.c, without IPX; SOCKS 5 kept)

Based on Quake III Arena source code, Copyright (C) 1999-2005 Id Software, Inc.
Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#include "../game/q_shared.h"
#include "../qcommon/qcommon.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#define INVALID_SOCKET	-1

static qboolean	usingSocks = qfalse;
static qboolean	networkingEnabled = qfalse;

static cvar_t	*net_noudp;

static cvar_t	*net_socksEnabled;
static cvar_t	*net_socksServer;
static cvar_t	*net_socksPort;
static cvar_t	*net_socksUsername;
static cvar_t	*net_socksPassword;
static struct sockaddr	socksRelayAddr;

static int		ip_socket;
static int		socks_socket;

#define	MAX_IPS		16
static	int		numIP;
static	byte	localIP[MAX_IPS][4];

//=============================================================================

char *NET_ErrorString( void ) {
	return strerror( errno );
}

static void NetadrToSockadr( netadr_t *a, struct sockaddr *s ) {
	memset( s, 0, sizeof( *s ) );

	if ( a->type == NA_BROADCAST ) {
		( (struct sockaddr_in *)s )->sin_family = AF_INET;
		( (struct sockaddr_in *)s )->sin_port = a->port;
		( (struct sockaddr_in *)s )->sin_addr.s_addr = INADDR_BROADCAST;
	} else if ( a->type == NA_IP ) {
		( (struct sockaddr_in *)s )->sin_family = AF_INET;
		memcpy( &( (struct sockaddr_in *)s )->sin_addr.s_addr, a->ip, 4 );
		( (struct sockaddr_in *)s )->sin_port = a->port;
	}
}

static void SockadrToNetadr( struct sockaddr *s, netadr_t *a ) {
	if ( s->sa_family == AF_INET ) {
		a->type = NA_IP;
		memcpy( a->ip, &( (struct sockaddr_in *)s )->sin_addr.s_addr, 4 );
		a->port = ( (struct sockaddr_in *)s )->sin_port;
	}
}

static qboolean Sys_StringToSockaddr( const char *s, struct sockaddr *sadr ) {
	struct hostent	*h;

	memset( sadr, 0, sizeof( *sadr ) );
	( (struct sockaddr_in *)sadr )->sin_family = AF_INET;
	( (struct sockaddr_in *)sadr )->sin_port = 0;

	if ( s[0] >= '0' && s[0] <= '9' ) {
		( (struct sockaddr_in *)sadr )->sin_addr.s_addr = inet_addr( s );
	} else {
		if ( ( h = gethostbyname( s ) ) == 0 ) {
			return qfalse;
		}
		memcpy( &( (struct sockaddr_in *)sadr )->sin_addr.s_addr, h->h_addr_list[0], 4 );
	}
	return qtrue;
}

/*
=============
Sys_StringToAdr

idnewt
192.246.40.70
=============
*/
qboolean Sys_StringToAdr( const char *s, netadr_t *a ) {
	struct sockaddr sadr;

	if ( !Sys_StringToSockaddr( s, &sadr ) ) {
		return qfalse;
	}
	SockadrToNetadr( &sadr, a );
	return qtrue;
}

//=============================================================================

/*
==================
Sys_GetPacket

Never called by the game logic, just the system event queing
==================
*/
qboolean Sys_GetPacket( netadr_t *net_from, msg_t *net_message ) {
	int				ret;
	struct sockaddr	from;
	socklen_t		fromlen;

	if ( !ip_socket ) {
		return qfalse;
	}

	fromlen = sizeof( from );
	ret = recvfrom( ip_socket, net_message->data, net_message->maxsize, 0, (struct sockaddr *)&from, &fromlen );
	if ( ret < 0 ) {
		if ( errno != EWOULDBLOCK && errno != EAGAIN && errno != ECONNREFUSED ) {
			Com_Printf( "NET_GetPacket: %s\n", NET_ErrorString() );
		}
		return qfalse;
	}

	memset( ( (struct sockaddr_in *)&from )->sin_zero, 0, 8 );

	if ( usingSocks && memcmp( &from, &socksRelayAddr, fromlen ) == 0 ) {
		if ( ret < 10 || net_message->data[0] != 0 || net_message->data[1] != 0 || net_message->data[2] != 0 || net_message->data[3] != 1 ) {
			return qfalse;
		}
		net_from->type = NA_IP;
		memcpy( net_from->ip, &net_message->data[4], 4 );
		memcpy( &net_from->port, &net_message->data[8], 2 );
		net_message->readcount = 10;
	} else {
		SockadrToNetadr( &from, net_from );
		net_message->readcount = 0;
	}

	if ( ret == net_message->maxsize ) {
		Com_Printf( "Oversize packet from %s\n", NET_AdrToString( *net_from ) );
		return qfalse;
	}

	net_message->cursize = ret;
	return qtrue;
}

//=============================================================================

static char socksBuf[4096];

/*
==================
Sys_SendPacket
==================
*/
void Sys_SendPacket( int length, const void *data, netadr_t to ) {
	int				ret;
	struct sockaddr	addr;

	if ( to.type != NA_BROADCAST && to.type != NA_IP ) {
		if ( to.type == NA_IPX || to.type == NA_BROADCAST_IPX ) {
			return;		// no IPX on Linux
		}
		Com_Error( ERR_FATAL, "Sys_SendPacket: bad address type" );
		return;
	}
	if ( !ip_socket ) {
		return;
	}

	NetadrToSockadr( &to, &addr );

	if ( usingSocks && to.type == NA_IP ) {
		socksBuf[0] = 0;	// reserved
		socksBuf[1] = 0;
		socksBuf[2] = 0;	// fragment (not fragmented)
		socksBuf[3] = 1;	// address type: IPV4
		memcpy( &socksBuf[4], &( (struct sockaddr_in *)&addr )->sin_addr.s_addr, 4 );
		memcpy( &socksBuf[8], &( (struct sockaddr_in *)&addr )->sin_port, 2 );
		memcpy( &socksBuf[10], data, length );
		ret = sendto( ip_socket, socksBuf, length + 10, 0, &socksRelayAddr, sizeof( socksRelayAddr ) );
	} else {
		ret = sendto( ip_socket, data, length, 0, &addr, sizeof( addr ) );
	}
	if ( ret < 0 ) {
		// wouldblock is silent
		if ( errno == EWOULDBLOCK || errno == EAGAIN ) {
			return;
		}
		// some PPP links do not allow broadcasts and return an error
		if ( ( errno == EADDRNOTAVAIL || errno == EACCES ) && to.type == NA_BROADCAST ) {
			return;
		}
		Com_Printf( "NET_SendPacket: %s\n", NET_ErrorString() );
	}
}

//=============================================================================

/*
==================
Sys_IsLANAddress

LAN clients will have their rate var ignored
==================
*/
qboolean Sys_IsLANAddress( netadr_t adr ) {
	int		i;

	if ( adr.type == NA_LOOPBACK ) {
		return qtrue;
	}
	if ( adr.type != NA_IP ) {
		return qfalse;
	}
	if ( adr.ip[0] == 127 ) {
		return qtrue;
	}

	// Class A
	if ( ( adr.ip[0] & 0x80 ) == 0x00 ) {
		for ( i = 0 ; i < numIP ; i++ ) {
			if ( adr.ip[0] == localIP[i][0] ) {
				return qtrue;
			}
		}
		return qfalse;
	}

	// Class B
	if ( ( adr.ip[0] & 0xc0 ) == 0x80 ) {
		for ( i = 0 ; i < numIP ; i++ ) {
			if ( adr.ip[0] == localIP[i][0] && adr.ip[1] == localIP[i][1] ) {
				return qtrue;
			}
			// also check against the RFC1918 class b blocks
			if ( adr.ip[0] == 172 && localIP[i][0] == 172 && ( adr.ip[1] & 0xf0 ) == 16 && ( localIP[i][1] & 0xf0 ) == 16 ) {
				return qtrue;
			}
		}
		return qfalse;
	}

	// Class C
	for ( i = 0 ; i < numIP ; i++ ) {
		if ( adr.ip[0] == localIP[i][0] && adr.ip[1] == localIP[i][1] && adr.ip[2] == localIP[i][2] ) {
			return qtrue;
		}
		// also check against the RFC1918 class c blocks
		if ( adr.ip[0] == 192 && localIP[i][0] == 192 && adr.ip[1] == 168 && localIP[i][1] == 168 ) {
			return qtrue;
		}
	}
	return qfalse;
}

void Sys_ShowIP( void ) {
	int i;

	for ( i = 0 ; i < numIP ; i++ ) {
		Com_Printf( "IP: %i.%i.%i.%i\n", localIP[i][0], localIP[i][1], localIP[i][2], localIP[i][3] );
	}
}

//=============================================================================

static int NET_IPSocket( char *net_interface, int port ) {
	int					newsocket;
	struct sockaddr_in	address;
	int					on = 1;

	if ( net_interface ) {
		Com_Printf( "Opening IP socket: %s:%i\n", net_interface, port );
	} else {
		Com_Printf( "Opening IP socket: localhost:%i\n", port );
	}

	if ( ( newsocket = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP ) ) == INVALID_SOCKET ) {
		Com_Printf( "WARNING: UDP_OpenSocket: socket: %s\n", NET_ErrorString() );
		return 0;
	}

	// make it non-blocking
	if ( ioctl( newsocket, FIONBIO, &on ) < 0 ) {
		Com_Printf( "WARNING: UDP_OpenSocket: ioctl FIONBIO: %s\n", NET_ErrorString() );
		close( newsocket );
		return 0;
	}

	// make it broadcast capable
	if ( setsockopt( newsocket, SOL_SOCKET, SO_BROADCAST, (char *)&on, sizeof( on ) ) < 0 ) {
		Com_Printf( "WARNING: UDP_OpenSocket: setsockopt SO_BROADCAST: %s\n", NET_ErrorString() );
		close( newsocket );
		return 0;
	}

	if ( !net_interface || !net_interface[0] || !Q_stricmp( net_interface, "localhost" ) ) {
		memset( &address, 0, sizeof( address ) );
		address.sin_addr.s_addr = INADDR_ANY;
	} else {
		Sys_StringToSockaddr( net_interface, (struct sockaddr *)&address );
	}

	address.sin_port = port == PORT_ANY ? 0 : htons( (short)port );
	address.sin_family = AF_INET;

	if ( bind( newsocket, (void *)&address, sizeof( address ) ) < 0 ) {
		Com_Printf( "WARNING: UDP_OpenSocket: bind: %s\n", NET_ErrorString() );
		close( newsocket );
		return 0;
	}

	return newsocket;
}

static void NET_OpenSocks( int port ) {
	struct sockaddr_in	address;
	struct hostent		*h;
	int					len;
	qboolean			rfc1929;
	unsigned char		buf[64];
	short				nport;

	usingSocks = qfalse;

	Com_Printf( "Opening connection to SOCKS server.\n" );

	if ( ( socks_socket = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP ) ) == INVALID_SOCKET ) {
		Com_Printf( "WARNING: NET_OpenSocks: socket: %s\n", NET_ErrorString() );
		return;
	}

	h = gethostbyname( net_socksServer->string );
	if ( h == NULL ) {
		Com_Printf( "WARNING: NET_OpenSocks: gethostbyname: %s\n", NET_ErrorString() );
		return;
	}
	if ( h->h_addrtype != AF_INET ) {
		Com_Printf( "WARNING: NET_OpenSocks: gethostbyname: address type was not AF_INET\n" );
		return;
	}
	memset( &address, 0, sizeof( address ) );
	address.sin_family = AF_INET;
	memcpy( &address.sin_addr.s_addr, h->h_addr_list[0], 4 );
	address.sin_port = htons( (short)net_socksPort->integer );

	if ( connect( socks_socket, (struct sockaddr *)&address, sizeof( address ) ) < 0 ) {
		Com_Printf( "NET_OpenSocks: connect: %s\n", NET_ErrorString() );
		return;
	}

	// send socks authentication handshake
	rfc1929 = ( *net_socksUsername->string || *net_socksPassword->string ) ? qtrue : qfalse;

	buf[0] = 5;		// SOCKS version
	if ( rfc1929 ) {
		buf[1] = 2;
		len = 4;
	} else {
		buf[1] = 1;
		len = 3;
	}
	buf[2] = 0;		// method #1 - method id #00: no authentication
	if ( rfc1929 ) {
		buf[2] = 2;		// method #2 - method id #02: username/password
	}
	if ( send( socks_socket, buf, len, 0 ) < 0 ) {
		Com_Printf( "NET_OpenSocks: send: %s\n", NET_ErrorString() );
		return;
	}

	// get the response
	len = recv( socks_socket, buf, 64, 0 );
	if ( len < 0 ) {
		Com_Printf( "NET_OpenSocks: recv: %s\n", NET_ErrorString() );
		return;
	}
	if ( len != 2 || buf[0] != 5 ) {
		Com_Printf( "NET_OpenSocks: bad response\n" );
		return;
	}
	if ( buf[1] != 0 && buf[1] != 2 ) {
		Com_Printf( "NET_OpenSocks: request denied\n" );
		return;
	}

	// do username/password authentication if needed
	if ( buf[1] == 2 ) {
		int		ulen = strlen( net_socksUsername->string );
		int		plen = strlen( net_socksPassword->string );

		buf[0] = 1;		// username/password authentication version
		buf[1] = ulen;
		if ( ulen ) {
			memcpy( &buf[2], net_socksUsername->string, ulen );
		}
		buf[2 + ulen] = plen;
		if ( plen ) {
			memcpy( &buf[3 + ulen], net_socksPassword->string, plen );
		}
		if ( send( socks_socket, buf, 3 + ulen + plen, 0 ) < 0 ) {
			Com_Printf( "NET_OpenSocks: send: %s\n", NET_ErrorString() );
			return;
		}
		len = recv( socks_socket, buf, 64, 0 );
		if ( len < 0 ) {
			Com_Printf( "NET_OpenSocks: recv: %s\n", NET_ErrorString() );
			return;
		}
		if ( len != 2 || buf[0] != 1 ) {
			Com_Printf( "NET_OpenSocks: bad response\n" );
			return;
		}
		if ( buf[1] != 0 ) {
			Com_Printf( "NET_OpenSocks: authentication failed\n" );
			return;
		}
	}

	// send the UDP associate request
	buf[0] = 5;		// SOCKS version
	buf[1] = 3;		// command: UDP associate
	buf[2] = 0;		// reserved
	buf[3] = 1;		// address type: IPV4
	memset( &buf[4], 0, 4 );		// INADDR_ANY
	nport = htons( (short)port );
	memcpy( &buf[8], &nport, 2 );
	if ( send( socks_socket, buf, 10, 0 ) < 0 ) {
		Com_Printf( "NET_OpenSocks: send: %s\n", NET_ErrorString() );
		return;
	}

	// get the response
	len = recv( socks_socket, buf, 64, 0 );
	if ( len < 0 ) {
		Com_Printf( "NET_OpenSocks: recv: %s\n", NET_ErrorString() );
		return;
	}
	if ( len < 2 || buf[0] != 5 ) {
		Com_Printf( "NET_OpenSocks: bad response\n" );
		return;
	}
	if ( buf[1] != 0 ) {
		Com_Printf( "NET_OpenSocks: request denied: %i\n", buf[1] );
		return;
	}
	if ( buf[3] != 1 ) {
		Com_Printf( "NET_OpenSocks: relay address is not IPV4: %i\n", buf[3] );
		return;
	}
	memset( &socksRelayAddr, 0, sizeof( socksRelayAddr ) );
	( (struct sockaddr_in *)&socksRelayAddr )->sin_family = AF_INET;
	memcpy( &( (struct sockaddr_in *)&socksRelayAddr )->sin_addr.s_addr, &buf[4], 4 );
	memcpy( &( (struct sockaddr_in *)&socksRelayAddr )->sin_port, &buf[8], 2 );

	usingSocks = qtrue;
}

static void NET_GetLocalAddress( void ) {
	char			hostname[256];
	struct hostent	*hostInfo;
	char			*p;

	if ( gethostname( hostname, sizeof( hostname ) ) < 0 ) {
		return;
	}
	hostInfo = gethostbyname( hostname );
	if ( !hostInfo || hostInfo->h_addrtype != AF_INET ) {
		return;
	}
	Com_Printf( "Hostname: %s\n", hostInfo->h_name );

	numIP = 0;
	while ( numIP < MAX_IPS && ( p = hostInfo->h_addr_list[numIP] ) != NULL ) {
		memcpy( localIP[numIP], p, 4 );
		Com_Printf( "IP: %i.%i.%i.%i\n", localIP[numIP][0], localIP[numIP][1], localIP[numIP][2], localIP[numIP][3] );
		numIP++;
	}
}

static void NET_OpenIP( void ) {
	cvar_t	*ip;
	int		port;
	int		i;

	ip = Cvar_Get( "net_ip", "localhost", CVAR_LATCH );
	port = Cvar_Get( "net_port", va( "%i", PORT_SERVER ), CVAR_LATCH )->integer;

	// automatically scan for a valid port, so multiple
	// dedicated servers can be started without requiring
	// a different net_port for each one
	for ( i = 0 ; i < 10 ; i++ ) {
		ip_socket = NET_IPSocket( ip->string, port + i );
		if ( ip_socket ) {
			Cvar_SetValue( "net_port", port + i );
			if ( net_socksEnabled->integer ) {
				NET_OpenSocks( port + i );
			}
			NET_GetLocalAddress();
			return;
		}
	}
	Com_Printf( "WARNING: Couldn't allocate IP port\n" );
}

//===================================================================

static qboolean NET_GetCvars( void ) {
	qboolean	modified = qfalse;

	if ( net_noudp && net_noudp->modified ) {
		modified = qtrue;
	}
	net_noudp = Cvar_Get( "net_noudp", "0", CVAR_LATCH | CVAR_ARCHIVE );

	if ( net_socksEnabled && net_socksEnabled->modified ) {
		modified = qtrue;
	}
	net_socksEnabled = Cvar_Get( "net_socksEnabled", "0", CVAR_LATCH | CVAR_ARCHIVE );

	if ( net_socksServer && net_socksServer->modified ) {
		modified = qtrue;
	}
	net_socksServer = Cvar_Get( "net_socksServer", "", CVAR_LATCH | CVAR_ARCHIVE );

	if ( net_socksPort && net_socksPort->modified ) {
		modified = qtrue;
	}
	net_socksPort = Cvar_Get( "net_socksPort", "1080", CVAR_LATCH | CVAR_ARCHIVE );

	if ( net_socksUsername && net_socksUsername->modified ) {
		modified = qtrue;
	}
	net_socksUsername = Cvar_Get( "net_socksUsername", "", CVAR_LATCH | CVAR_ARCHIVE );

	if ( net_socksPassword && net_socksPassword->modified ) {
		modified = qtrue;
	}
	net_socksPassword = Cvar_Get( "net_socksPassword", "", CVAR_LATCH | CVAR_ARCHIVE );

	return modified;
}

void NET_Config( qboolean enableNetworking ) {
	qboolean	modified;
	qboolean	stop;
	qboolean	start;

	// get any latched changes to cvars
	modified = NET_GetCvars();

	if ( net_noudp->integer ) {
		enableNetworking = qfalse;
	}

	// if enable state is the same and no cvars were modified, we have nothing to do
	if ( enableNetworking == networkingEnabled && !modified ) {
		return;
	}

	if ( enableNetworking == networkingEnabled ) {
		stop = start = enableNetworking;
	} else {
		stop = !enableNetworking;
		start = enableNetworking;
		networkingEnabled = enableNetworking;
	}

	if ( stop ) {
		if ( ip_socket ) {
			close( ip_socket );
			ip_socket = 0;
		}
		if ( socks_socket ) {
			close( socks_socket );
			socks_socket = 0;
		}
	}

	if ( start && !net_noudp->integer ) {
		NET_OpenIP();
	}
}

void NET_Init( void ) {
	// this is really just to get the cvars registered
	NET_GetCvars();
	NET_Config( qtrue );
}

void NET_Shutdown( void ) {
	NET_Config( qfalse );
}

/*
====================
NET_Sleep

sleeps msec or until net socket is ready
====================
*/
void NET_Sleep( int msec ) {
}

void NET_Restart( void ) {
	NET_Config( networkingEnabled );
}
