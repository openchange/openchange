/*
   RPCExtract Implementation

   Copyright (C) Jerome MEDEGAN 2006

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

#ifndef			__RPCEXTRACT_H_
#define			__RPCEXTRACT_H_

#include		<stdlib.h>
#include		<unistd.h>
#include		<stdio.h>
#include		<string.h>
#include		<ctype.h>
#include		<limits.h>
#include		<fcntl.h>
#include		<errno.h>
#include		<sys/types.h>
#include		<sys/socket.h>
#include		<sys/stat.h>

#include		<pcap.h>

#include		<netinet/in_systm.h>
#include		<netinet/in.h>
#include		<netinet/ip.h>
#include		<ifaddrs.h>
#include		<netdb.h>
#include		<net/if.h>
#include		<net/ethernet.h>
#include		<netinet/tcp.h>
#include		<arpa/inet.h>

#ifdef			__BSD_VISIBLE
#define	SPORT		th_sport
#define	DPORT		th_dport
#else
#define	SPORT		source
#define	DPORT		dest
#endif

#define	UNKNOWN_PROTO	"UnknownProtocol"
#define UNKNOWN_NSPI_OP	"UnknownNspiOperation"
#define UNKNOWN_MAPI_OP	"UnknownMapiOperation"

#define RPC_REQ		0
#define RPC_RESP	2

#define	ETHERTYPE_IPINV	0x0008

#define FLWD_TSTRM	(((strncmp(cur_ip_src, ip_src, IP_LEN) == 0) && (strncmp(cur_ip_dst, ip_dst, IP_LEN) == 0)) || ((strncmp(cur_ip_src, ip_dst, IP_LEN) == 0) && (strncmp(cur_ip_dst, ip_src, IP_LEN) == 0)))
#define FLWD_DPORT	(dst_port == ENDPM_PORT)
#define FLWD_SPORT	(src_port == ENDPM_PORT)
#define PKT_GOT_DATA	(psh)
#define	ACK_ONLY	((ack) && (!syn) && (!ffin))

#define SNAPLEN		1514
#define TIMEOUT		1000
#define	IP_LEN		17
#define	ENDPM_PORT	135
#define LEN2ENDPMAP	78
#define	ENDPMAPHLEN	20
#define FLOORSTUFFLEN	13
#define TOWERSTUFFLEN	14
#define	UUID_LEN	16
#define	UUIDPTRLEN	20
#define	LEN2FLOOR	10
#define	LEN2ALLOCH	16

typedef	struct		filter_s
{
	char		*filter;
	struct filter_s *next_filter;
}			filter_t;

typedef struct		proto_s
{
	const char	*uuid;
	const char	*name;
}			proto_t;

typedef struct		sector_s
{
	short		opnum;
	const char	*opstr;
}			sector_t;

typedef struct		bport_s
{
	char		*proto;
	int		bport;
	struct bport_s	*next_bport;
}			bport_t;

bport_t			*bports;
short			last_opnum;

#endif			/* __RPCEXTRACT_H_ */
