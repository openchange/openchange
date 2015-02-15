/*
   RPCExtract Implementation

   Copyright (C) Julien Kerihuel 2014
   Copyright (C) Jerome Medegan  2006
   Copyright (C) Pauline Khun    2006

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

#include "libmapi/libmapi.h"
#include "rpcextract.h"

#include <popt.h>
#include <pcap.h>

bool verbose = false;
const char *destdir = NULL;

proto_t				protos[] =
{
	{"\x18\x5a\xcc\xf5\x64\x42\x1a\x10\x8c\x59\x08\x00\x2b\x2f\x84\x26", "Nspi"},
	{"\x00\xdb\xf1\xa4\x47\xca\x67\x10\xb3\x1f\x00\xdd\x01\x06\x62\xda", "Mapi"},
	{0, 0}
};

static void add_binded_port(char *proto_name, int proto_port)
{
	bport_t	*new_bport;

	new_bport = (bport_t *)malloc(sizeof(bport_t));
	new_bport->proto = strdup(proto_name);
	new_bport->bport = proto_port;
	new_bport->next_bport = bports;
	bports = new_bport;
}

static int seek_port(bport_t **bport, u_int port1, u_int port2)
{
	bport_t	*save;

	save = bports;
	while (save) {
		if ((save->bport == port1) || (save->bport == port2)) {
			*bport = save;
			return (1);
		}
		save = save->next_bport;
	}
	return (0);
}

static char *rip_proto(u_char *data)
{
	u_char	*endp_map = data + LEN2ENDPMAP;
	u_char	*tower_ptr = endp_map + UUIDPTRLEN;
	u_char	*fst_floor = tower_ptr + LEN2FLOOR + 4;
	short	*nb_floors = (short *)(tower_ptr + 4 + (LEN2FLOOR - 2));
	short	*lhs;
	short	*rhs;
	u_char	*prot;
	u_char	*uuid;
	int	i;
	int	j;

	if (tower_ptr[0] == '\0') return UNKNOWN_PROTO;

	uuid = malloc(sizeof(char) * (UUID_LEN + 1));
	uuid[UUID_LEN] = 0;
	for (i = 0; i < (*nb_floors); i++)
	{
		lhs = (short *)fst_floor;
		prot = (u_char *)fst_floor + sizeof(short);
		rhs = (short *)(fst_floor + sizeof(short) + (*lhs));
		if (*prot == 0x0d)
		{
			strncpy((char *)uuid, (char *)fst_floor + sizeof(short) + sizeof(u_char), UUID_LEN);
			for (j = 0; protos[j].name; j++) {
				if (strncmp((char *)protos[j].uuid, (char *)uuid, UUID_LEN) == 0) {
					free(uuid);
					return (strdup(protos[j].name));
				}
			}
		}
		fst_floor += (*lhs) + (*rhs) + (2 * sizeof(short));
	}
	free (uuid);
	return (UNKNOWN_PROTO);
}

static int rip_port(u_char *data)
{
	u_char	*endp_map = data + LEN2ENDPMAP;
	int	*nb_towers = (int *)(endp_map + ENDPMAPHLEN);
	u_char	*tower_ptr = endp_map + ENDPMAPHLEN + 16;
	short	*nb_floors = (short *)(tower_ptr + (4 * (*nb_towers)) + (LEN2FLOOR - 2));
	u_char	*fst_floor = tower_ptr + (4 * (*nb_towers))+ LEN2FLOOR;
	short	*lhs;
	short	*rhs;
	short	*port;
	u_char	*prot;
	int	i;

	for (i = 0; i < (*nb_floors); i++) {
		lhs = (short *)fst_floor;
		prot = (u_char *)fst_floor + sizeof(short);
		rhs = (short *)(fst_floor + sizeof(short) + (*lhs));
		if (*prot == 0x07)
		{
			port = (short *)(fst_floor + (2 * sizeof(short)) + (*lhs));
			return (ntohs(*port));
		}
		fst_floor += (*lhs) + (*rhs) + (2 * sizeof(short));
	}
	return (0);
}

sector_t MapiSector[] =
{
	{ 0, "EcDoConnect"},
	{ 1, "EcDoDisconnect"},
	{ 2, "EcDoRpc"},
	{ 3, "EcGetMoreRpc"},
	{ 4, "EcRRegisterPushNotification"},
	{ 5, "EcRUnregisterPushNotification"},
	{ 6, "EcDummyRpc"},
	{ 7, "EcRGetDCName"},
	{ 8, "EcRNetGetDCName"},
	{ 9, "EcDoRpcExt"},
	{ 10, "EcDoConnectEx"},
	{ 11, "EcDoRpcExt2"},
	{0, NULL}
};

static const char *GetMapiOpStr(short opnum)
{
	int i;

	for (i = 0; MapiSector[i].opstr; i++) {
		if (opnum == MapiSector[i].opnum)
			return (MapiSector[i].opstr);
	}
	return (UNKNOWN_MAPI_OP);
}


sector_t NspiSector[] =
{
	{ 0, "NspiBind"},
	{ 1, "NspiUnbind"},
	{ 2, "NspiUpdateStat"},
	{ 3, "NspiQueryRows"},
	{ 4, "NspiSeekEntries"},
	{ 5, "NspiGetMatches"},
	{ 6, "NspiResortRestriction"},
	{ 7, "NspiDNToEph"},
	{ 8, "NspiGetPropList"},
	{ 9, "NspiGetProps"},
	{ 10, "NspiCompareDNTs"},
	{ 11, "NspiModProps"},
	{ 12, "NspiGetHierarchyInfo"},
	{ 13, "NspiGetTemplateInfo"},
	{ 14, "NspiModLinkAttr"},
	{ 15, "NspiDeleteEntries"},
	{ 16, "NspiQueryColumns"},
	{ 17, "NspiGetNamesFromIDs"},
	{ 18, "NspiGetIDsFromNames"},
	{ 19, "NspiResolveNames"},
	{ 0, NULL}
};

static const char *GetNspiOpStr(short opnum)
{
	int i;

	for (i = 0; NspiSector[i].opstr; i++) {
		if (opnum == NspiSector[i].opnum)
			return (NspiSector[i].opstr);
	}
	return (UNKNOWN_NSPI_OP);
}


static void rip_file(int packet_num, char packet_type, char *proto, u_char *tcp)
{
	short			*opnum;
	u_int			*alloch;
	const char		*opstr = NULL;
	char			*filename;
	char			*olddir = getcwd(NULL, 0);
	int			fd;

	alloch = (u_int *)(tcp + sizeof(struct tcphdr) + LEN2ALLOCH);
	if (packet_type == RPC_REQ) {
		opnum =  (short *)(tcp + sizeof(struct tcphdr) + LEN2ALLOCH + 6);
	} else {
		opnum = (short *)(&last_opnum);
	}
	if (strcmp(proto, "Nspi") == 0)
		opstr = GetNspiOpStr(*opnum);
	if (strcmp(proto, "Mapi") == 0)
		opstr = GetMapiOpStr(*opnum);
	asprintf(&filename, "%d_%s_%s_%s", packet_num, (packet_type == RPC_REQ)?"in":"out", proto, opstr);
	if (verbose)
		OC_DEBUG(0, "[INFO] Extracting data into file %s (size = %d bytes)", filename, *alloch);
	if (destdir) {
		if (chdir(destdir) < 0)
			perror("chdir");
	}

	fd = open(filename, O_WRONLY | O_TRUNC | O_CREAT, 0600);
	if (fd < 0) {
		perror("open");
		exit (1);
	}
	write(fd, (char *)(tcp + sizeof(struct tcphdr) + LEN2ALLOCH + 8), *alloch);
	close(fd);

	if (destdir) {
		chdir(olddir);
	}

	if (verbose) {
		OC_DEBUG(0, "[INFO] Extraction accomplished");
	}
	last_opnum = *opnum;
}

static void extr_cb(u_char *param, const struct pcap_pkthdr *header, const u_char *data)
{
	struct ether_header	*packet_ether;
	struct ip		*packet_ip;
	struct tcphdr		*packet_tcp;
	static int		i = 1;
	static int		init = 0;
	static int		endp_bind = 0;
	static int		endp_deco = 0;
	static char		*proto = NULL;
	static int		binded = -1;
	static int		fin = 0;
	static char		ip_src[IP_LEN + 1];
	static char		ip_dst[IP_LEN + 1];
	char			cur_ip_src[IP_LEN + 1];
	char			cur_ip_dst[IP_LEN + 1];
	u_int			size;
	u_int			src_port;
	u_int			dst_port;
	u_int			no_hands = 0;
	u_int			ack;
	u_int			syn;
	u_int			ffin;
	u_int			psh;
	char			*packet_type;
	bport_t			*bport;

	ip_src[IP_LEN] = 0;
	ip_dst[IP_LEN] = 0;
	cur_ip_src[IP_LEN] = 0;
	cur_ip_dst[IP_LEN] = 0;

	i++;

	/* Packet Check */
	packet_ether = (struct ether_header *)(data);
	if (packet_ether->ether_type != ETHERTYPE_IPINV) {
		if (verbose) {
			OC_DEBUG(0, "[ERR] %d - Non IP Packet ... skipped", (i - 1));
		}
		return;
	}

	size = (header->caplen - sizeof(struct ether_header));
	if (size < sizeof(struct ip))
	{
		OC_DEBUG(0, "[ERR] Packet %d : Uncomplete IP Header..", i - 1);
		return ;
	}
	packet_ip = (struct ip *)(data + sizeof(struct ether_header));
	if ((packet_ip->ip_hl * 4) < sizeof (struct ip))
	{
		OC_DEBUG(0, "[ERR] Packet %d : IP Header error..", i - 1);
		return ;
	}
	size = (header->caplen - sizeof(struct ether_header) - (packet_ip->ip_hl * 4));
	if (size < sizeof(struct tcphdr))
	{
		printf("Packet %d : Uncomplete TCP Header..\n", i - 1);
		return ;
	}

	/* TCP Segment */

	packet_tcp = (struct tcphdr *)(data + sizeof(struct ether_header) + (packet_ip->ip_hl * 4));
	src_port = ntohs(packet_tcp->SPORT);
	dst_port = ntohs(packet_tcp->DPORT);

#ifndef	__BSD_VISIBLE
	ack = packet_tcp->ack;
	syn = packet_tcp->syn;
	ffin = packet_tcp->fin;
	psh = packet_tcp->psh;
#else
	ack = packet_tcp->th_flags & TH_ACK;
	syn = packet_tcp->th_flags & TH_SYN;
	ffin = packet_tcp->th_flags & TH_FIN;
	psh = packet_tcp->th_flags & TH_PUSH;
#endif

	if (packet_ip->ip_p == IPPROTO_TCP) {
		strncpy(cur_ip_src, (char *)inet_ntoa(packet_ip->ip_src), IP_LEN);
		strncpy(cur_ip_dst, (char *)inet_ntoa(packet_ip->ip_dst), IP_LEN);
		if (init && (endp_bind == 0) && FLWD_TSTRM && (FLWD_DPORT || FLWD_SPORT) && ffin)
		  endp_deco++;
		if (fin) {
			no_hands = 1;
			fin = 0;
		}
		if (ffin)
			fin = 1;
		if (bports) {
			if (FLWD_TSTRM && (seek_port(&bport, dst_port, src_port) == 1) && PKT_GOT_DATA) {
				packet_type = (char *)((char *)packet_tcp + sizeof(struct tcphdr) + 2);
				if ((*packet_type == RPC_REQ) || (*packet_type == RPC_RESP))
					rip_file(i - 1, *packet_type, bport->proto, (u_char *)packet_tcp);
			}
		}
		if ((endp_bind) && FLWD_TSTRM && FLWD_DPORT) {
		manage_epm_req:
			packet_type = (char *)((char *)packet_tcp + sizeof(struct tcphdr) + 2);
			if (*packet_type == RPC_REQ)
			{
				if (verbose)
					OC_DEBUG(0, "[INFO] Got endpoint mapper request on packet %d", i - 1);

				proto = rip_proto((u_char *)(const u_char *)data);
				if (strcmp(proto, UNKNOWN_PROTO) == 0)
					return ;
				if (verbose)
					OC_DEBUG(0, "[INFO] Query protocol %s", proto);
			} else {
				if (verbose) {
					OC_DEBUG(0, "[INFO] packet.type is not RPC_REQ but 0x%x", *packet_type);
				}
			}
		}
		if ((endp_bind) && FLWD_TSTRM && FLWD_SPORT) {
			packet_type = (char *)((char *)packet_tcp + sizeof(struct tcphdr) + 2);
			if (*packet_type == RPC_RESP) {
				if (verbose)
					OC_DEBUG(0, "[INFO] Got endpoint mapper response on packet %d", i - 1);

				binded = rip_port((u_char *)(const u_char *)data);
				if (binded == 0) return;
				endp_bind = 0;

				if (verbose)
					OC_DEBUG(0, "[INFO] Endpoint mapper response : %d", binded);

				if (strcmp(proto, UNKNOWN_PROTO) == 0) return;
				add_binded_port(proto, binded);
			} else {
				return;
			}
		}
		if (endp_bind)
			return ;
		if (init && (!no_hands) && FLWD_TSTRM && FLWD_DPORT && ACK_ONLY) {
			if (verbose)
				OC_DEBUG(0, "[INFO] Got end of handshake on packet %d", i - 1);

			endp_bind = 1;
			endp_deco = 0;
			goto manage_epm_req;
		}

		if ((init == 0) && FLWD_DPORT) {
			init = 1;
			strncpy(ip_src, (char *)inet_ntoa(packet_ip->ip_src), IP_LEN);
			strncpy(ip_dst, (char *)inet_ntoa(packet_ip->ip_dst), IP_LEN);
		}
	}
}

int main(int argc, const char *argv[])
{
	char			err_buff[PCAP_ERRBUF_SIZE];
	pcap_t			*desc_pkt;
	const char		*opt_pcap = NULL;
	const char		*opt_int = NULL;
	poptContext		pc;
	int			opt;

	enum { OPT_PCAP=1000, OPT_INT, OPT_DESTDIR, OPT_VERBOSE };
	struct poptOption	long_options[] = {
		POPT_AUTOHELP
		{ "pcap", 'i', POPT_ARG_STRING, NULL, OPT_PCAP, "Extract from pcap file", NULL },
		{ "int", 'l', POPT_ARG_STRING, NULL, OPT_INT, "Extract from interface", NULL },
		{ "destdir", 'd', POPT_ARG_STRING, NULL, OPT_DESTDIR, "Specify destination directory", NULL },
		{ "verbose", 'v', POPT_ARG_NONE, NULL, OPT_VERBOSE, "Enable verbosity", NULL },
		{NULL, 0, 0, NULL, 0, NULL, NULL}
	};

	pc = poptGetContext("rpcextract", argc, argv, long_options, 0);
	while ((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		case OPT_PCAP:
			opt_pcap = poptGetOptArg(pc);
			break;
		case OPT_INT:
			opt_int = poptGetOptArg(pc);
			break;
		case OPT_DESTDIR:
			destdir = poptGetOptArg(pc);
			break;
		case OPT_VERBOSE:
			verbose = true;
			break;
		}
	}

	if (!opt_pcap && !opt_int) {
		OC_DEBUG(0, "[ERR] No device or file selected");
		exit (1);
	}

	if (opt_pcap && opt_int) {
		OC_DEBUG(0, "[ERR] Select either pcap file or device");
		exit (1);
	}

	bports = NULL;
	last_opnum = -1;

	if (opt_pcap) {
		desc_pkt = pcap_open_offline(opt_pcap, err_buff);
		if (desc_pkt == NULL) {
			OC_DEBUG(0, "[ERR] Error opening pcap file: %s", err_buff);
			exit (1);
		}
	} else {
		desc_pkt = pcap_open_live(opt_int, SNAPLEN, IFF_PROMISC, TIMEOUT, err_buff);
		if (desc_pkt == NULL) {
			OC_DEBUG(0, "[ERR] Error opening interface: %s", err_buff);
			exit (1);
		}
		OC_DEBUG(0, "[INFO] Listening on device %s", opt_int);
	}

	if (destdir) {
		if ((mkdir(destdir, 0700) < 0) && (errno != EEXIST)) {
			perror("mkdir");
			exit (1);
		}
	}

	if (pcap_loop(desc_pkt, -1, extr_cb, NULL) < 0)
	{
		OC_DEBUG(0, "[ERR] Capture Loop: %s", pcap_geterr(desc_pkt));
		exit (1);
	}
	pcap_close(desc_pkt);

	return (0);
}
