/* 
   Unix SMB/CIFS implementation.

   structures for socke/netif.c and socket/interface.c

   Copyright (C) Andrew Tridgell 2004
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/** used for network interfaces */
struct interface {
	struct interface *next, *prev;
	struct in_addr ip;
	struct in_addr nmask;
	const char *ip_s;
	const char *bcast_s;
	const char *nmask_s;
};

struct iface_struct {
	char name[16];
	struct in_addr ip;
	struct in_addr netmask;
};

#define MAX_INTERFACES 128
