/* 
   Unix SMB/CIFS implementation.
   return a list of network interfaces
   Copyright (C) Andrew Tridgell 1998
   
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


/* working out the interfaces for a OS is an incredibly non-portable
   thing. We have several possible implementations below, and autoconf
   tries each of them to see what works

   Note that this file does _not_ include includes.h. That is so this code
   can be called directly from the autoconf tests. That also means
   this code cannot use any of the normal Samba debug stuff or defines.
   This is standalone code.

*/

#include "config.h"

#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif

#include <libmapi/libmapi.h>

#ifdef __COMPAR_FN_T
#define QSORT_CAST (__compar_fn_t)
#endif

#ifndef QSORT_CAST
#define QSORT_CAST (int (*)(const void *, const void *))
#endif

/* this works for Linux 2.2, Solaris 2.5, SunOS4, HPUX 10.20, OSF1
   V4.0, Ultrix 4.4, SCO Unix 3.2, IRIX 6.4 and FreeBSD 3.2.

   It probably also works on any BSD style system.  */

/****************************************************************************
  get the netmask address for a local interface
****************************************************************************/
static int _get_interfaces(struct iface_struct *ifaces, int max_interfaces)
{  
	struct ifconf		ifc;
	struct ifreq		*ifr=NULL;
	struct in_addr		ipaddr;
	struct in_addr		nmask;
	struct sockaddr		*sockaddr;
	struct sockaddr_in	*sockaddr_in;
	char			*iname;
	char			buff[8192];
	int			fd, i, n;
	int			total = 0;

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		return -1;
	}
  
	ifc.ifc_len = sizeof(buff);
	ifc.ifc_buf = buff;

	if (ioctl(fd, SIOCGIFCONF, &ifc) != 0) {
		close(fd);
		return -1;
	} 

	ifr = ifc.ifc_req;
  
	n = ifc.ifc_len / sizeof(struct ifreq);

	/* Loop through interfaces, looking for given IP address */
	for (i=n-1;i>=0 && total < max_interfaces;i--) {
		if (ioctl(fd, SIOCGIFADDR, &ifr[i]) != 0) {
			continue;
		}

		iname = ifr[i].ifr_name;
		sockaddr = (struct sockaddr *) &ifr[i].ifr_addr;
		sockaddr_in = (struct sockaddr_in *) sockaddr;
		ipaddr = sockaddr_in->sin_addr;

		if (ioctl(fd, SIOCGIFFLAGS, &ifr[i]) != 0) {
			continue;
		}  

		if (!(ifr[i].ifr_flags & IFF_UP)) {
			continue;
		}

		if (ioctl(fd, SIOCGIFNETMASK, &ifr[i]) != 0) {
			continue;
		}  

		nmask = sockaddr_in->sin_addr;

		strncpy(ifaces[total].name, iname, sizeof(ifaces[total].name)-1);
		ifaces[total].name[sizeof(ifaces[total].name)-1] = 0;
		ifaces[total].ip = ipaddr;
		ifaces[total].netmask = nmask;
		total++;
	}

	close(fd);

	return total;
}  

static int iface_comp(struct iface_struct *i1, struct iface_struct *i2)
{
	int r;
	r = strcmp(i1->name, i2->name);
	if (r) return r;
	r = ntohl(i1->ip.s_addr) - ntohl(i2->ip.s_addr);
	if (r) return r;
	r = ntohl(i1->netmask.s_addr) - ntohl(i2->netmask.s_addr);
	return r;
}

/* this wrapper is used to remove duplicates from the interface list generated
   above */
_PUBLIC_ int get_interfaces(struct iface_struct *ifaces, int max_interfaces)
{
	int total, i, j;

	total = _get_interfaces(ifaces, max_interfaces);
	if (total <= 0) return total;

	/* now we need to remove duplicates */
	qsort(ifaces, total, sizeof(ifaces[0]), QSORT_CAST iface_comp);

	for (i=1;i<total;) {
		if (iface_comp(&ifaces[i-1], &ifaces[i]) == 0) {
			for (j=i-1;j<total-1;j++) {
				ifaces[j] = ifaces[j+1];
			}
			total--;
		} else {
			i++;
		}
	}

	return total;
}
