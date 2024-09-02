#include "ft_ping.h"

int
ping_decode_buffer(struct ping_data *ping, size_t bufsize,
	struct ip **ipp, struct icmp **icmpp)
{
	unsigned int hlen;
	unsigned short cksum;
	struct ip *ip;
	struct icmp *icmp;

	ip = (struct ip *)ping->buffer;
	hlen = ip->ip_hl << 2;
	icmp = (struct icmp *)(ping->buffer + hlen);

	*ipp = ip;
	*icmpp = icmp;

	cksum = icmp->icmp_cksum;
	icmp->icmp_cksum = 0;
	icmp->icmp_cksum = icmp_cksum(ping->buffer, bufsize);
	if (icmp->icmp_cksum != cksum)
		return 1;
	return 0;
}

void
ping_set_stat(struct ping_stat *ping_stat, double triptime)
{
	if (!ping_stat->tmin || triptime < ping_stat->tmin)
		ping_stat->tmin = triptime;
	if (triptime > ping_stat->tmax)
		ping_stat->tmax = triptime;

	ping_stat->tsum += triptime;
	ping_stat->tsumsq += triptime * triptime;
}

void
ping_echo_print(struct ping_data *ping,
	struct ip *ip, struct icmp *icmp, size_t datalen)
{
	struct timeval tv_out, *tv_in;
	double triptime;

	gettimeofday(&tv_out, NULL);
	tv_in = (struct timeval *)icmp->icmp_data;
	tvsub(&tv_out, tv_in);
	triptime = tv_out.tv_sec * 1000.0 + tv_out.tv_usec / 1000.0;
	ping_set_stat(&ping->ping_stat, triptime);

	printf("%lu bytes from %s: icmp_seq=%u ttl=%i time=%.3f ms\n",
		datalen,
		inet_ntoa(ping->from_addr.sin_addr),
		ntohs(icmp->icmp_seq),
		ip->ip_ttl,
		triptime);
	ping->num_recv++;
}

void
ping_icmp_print(struct ping_data *ping,
	struct ip *ip_org, struct icmp *icmp, size_t datalen, char *descp)
{
	size_t i, hlen;
	unsigned char *cp;
	struct ip *ip;
	int type, code;

	ip = &icmp->icmp_ip;
	hlen = ip->ip_hl << 2;
	cp = (unsigned char *)ip + hlen;
	type = *cp;
	code = *(cp + 1);

	printf("%lu bytes from %s: %s\n",
		ntohs(ip_org->ip_len) - hlen,
		inet_ntoa(ping->from_addr.sin_addr),
		descp);

	if (g_ping_options & PING_OPTION_VERBOSE)
	{
		printf("IP hdr DUMP:\n");
		for (i = 0; i < sizeof(*ip); ++i)
			printf("%02x%s",
				*((unsigned char *)ip + i),
				(i % 2) ? " " : "");
		printf("\n");

		printf("Vr HL TOS  Len   ID Flg  off TTL Pro  cks      Src\tDst\tData\n");
		printf(" %1x %1x %02x", ip->ip_v, ip->ip_hl, ip->ip_tos);
		printf(" %04x %04x", ntohs(ip->ip_len), ntohs(ip->ip_id));
		printf("   %1x %04x", (ntohs(ip->ip_off) & 0xe000) >> 13, ntohs(ip->ip_off) & 0x1fff);
		printf("  %02x  %02x %04x", ip->ip_ttl, ip->ip_p, ntohs(ip->ip_sum));
		printf(" %s ", inet_ntoa(*((struct in_addr *)&ip->ip_src)));
		printf(" %s ", inet_ntoa(*((struct in_addr *)&ip->ip_dst)));
		printf("\n");

		printf("ICMP: type %u, code %u, size %lu", type, code, datalen);
		printf(" , id 0x%04x, seq 0x%04x", *(cp + 4) * 256 + *(cp + 5),
			*(cp + 6) * 256 + *(cp + 7));
		printf("\n");
	}
}

int
ping_recv(struct ping_data *ping)
{
	int nrecv;
	socklen_t from_addrlen;
	size_t bufsize;
	struct ip *ip;
	struct icmp *icmp;

	from_addrlen = sizeof(struct sockaddr_in);
	bufsize = ping->datalen + ICMP_MINLEN;
	nrecv = recvfrom(ping->fd, ping->buffer, bufsize, 0,
		(struct sockaddr *)&ping->from_addr, &from_addrlen);
	if (nrecv < 0)
		return 1;

	ping_decode_buffer(ping, nrecv, &ip, &icmp);

	switch(icmp->icmp_type)
	{
		case ICMP_ECHOREPLY:
			if (ntohs(icmp->icmp_id) != ping->id)
				return 1;
			ping_echo_print(ping, ip, icmp, nrecv);
			break;

		case ICMP_TIME_EXCEEDED:
			ping_icmp_print(ping, ip, icmp, nrecv, "Time to live exceeded");
			break;

		default:
			return 1;
	}
	return 0;
}
