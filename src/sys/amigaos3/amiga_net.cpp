/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

This file is part of the Doom 3 GPL Source Code ("Doom 3 Source Code").

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

/*
 * AmigaOS 3 port (PLAN.md §2): single player only, so no sockets. The
 * engine's network layer still exists and asks these; loopback addresses work,
 * everything else reports that there is no network.
 */

#include "sys/platform.h"
#include "idlib/Str.h"
#include "framework/Common.h"
#include "sys/sys_public.h"

void Sys_InitNetworking( void ) {
	common->Printf( "no networking on this platform (single player only)\n" );
}

void Sys_ShutdownNetworking( void ) {
}

bool Sys_StringToNetAdr( const char *s, netadr_t *a, bool doDNSResolve ) {
	memset( a, 0, sizeof( *a ) );
	if ( !idStr::Icmp( s, "localhost" ) || !idStr::Icmpn( s, "localhost:", 10 ) ) {
		a->type = NA_LOOPBACK;
		return true;
	}
	a->type = NA_BAD;
	return false;
}

const char *Sys_NetAdrToString( const netadr_t a ) {
	static char s[64];
	if ( a.type == NA_LOOPBACK ) {
		if ( a.port ) {
			idStr::snPrintf( s, sizeof( s ), "localhost:%i", a.port );
		} else {
			idStr::snPrintf( s, sizeof( s ), "localhost" );
		}
	} else if ( a.type == NA_IP ) {
		idStr::snPrintf( s, sizeof( s ), "%i.%i.%i.%i:%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3], a.port );
	} else {
		idStr::snPrintf( s, sizeof( s ), "(no address)" );
	}
	return s;
}

bool Sys_IsLANAddress( const netadr_t a ) {
	return a.type == NA_LOOPBACK;
}

bool Sys_CompareNetAdrBase( const netadr_t a, const netadr_t b ) {
	if ( a.type != b.type ) {
		return false;
	}
	if ( a.type == NA_LOOPBACK ) {
		return true;
	}
	return a.type == NA_IP && memcmp( a.ip, b.ip, 4 ) == 0;
}

idPort::idPort() {
	netSocket = 0;
	memset( &bound_to, 0, sizeof( bound_to ) );
	packetsRead = bytesRead = packetsWritten = bytesWritten = 0;
}

idPort::~idPort() {
	Close();
}

void idPort::Close() {
	netSocket = 0;
	memset( &bound_to, 0, sizeof( bound_to ) );
}

bool idPort::InitForPort( int portNumber ) {
	return false;	// no sockets: the port field stays 0, as the interface asks
}

bool idPort::GetPacket( netadr_t &from, void *data, int &size, int maxSize ) {
	return false;
}

bool idPort::GetPacketBlocking( netadr_t &from, void *data, int &size, int maxSize, int timeout ) {
	return false;
}

void idPort::SendPacket( const netadr_t to, const void *data, int size ) {
}

idTCP::idTCP() {
	fd = 0;
	memset( &address, 0, sizeof( address ) );
}

idTCP::~idTCP() {
	Close();
}

bool idTCP::Init( const char *host, short port ) {
	return false;
}

void idTCP::Close() {
	fd = 0;
}

int idTCP::Read( void *data, int size ) {
	return -1;
}

int idTCP::Write( void *data, int size ) {
	return -1;
}
