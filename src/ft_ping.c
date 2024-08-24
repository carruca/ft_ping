#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netdb.h>
#include <argp.h>
#include <error.h>
#include <math.h>
#include <signal.h>
#include <stdlib.h>

#define PING_OPTION_VERBOSE 				0x0001

#define PING_DEFAULT_COUNT 		0
#define PING_DEFAULT_INTERVAL 1000
#define PING_DEFAULT_DATALEN 	(64 - ICMP_MINLEN)
#define PING_DEFAULT_MAXTTL 	255

struct ping_stat
{
	double tmin;
	double tmax;
	double tsum;
	double tsumsq;
};

struct ping_data
{
	int fd;
	int type;
	pid_t id;
	size_t count;
	size_t interval;
	size_t datalen;
	size_t num_xmit;
	size_t num_recv;
	struct ping_stat ping_stat;
	struct sockaddr_in dest_addr;
	struct sockaddr_in from_addr;
	char *buffer;
};

unsigned g_ping_options = 0;
int g_stop = 0;
int g_ttl = 0;

int
ping_set_dest(struct ping_data *ping, const char *hostname)
{
	int rc;
	struct addrinfo hints, *res;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;

	rc = getaddrinfo(hostname, NULL, &hints, &res);
	if (rc != 0)
		return 1;
	memcpy(&ping->dest_addr, res->ai_addr, res->ai_addrlen);
	freeaddrinfo(res);
	return 0;
}

unsigned short
icmp_cksum(char *buffer, size_t bufsize)
{
	register int sum = 0;
	unsigned short *wp;

	for (wp = (unsigned short *)buffer; bufsize > 1; wp++, bufsize -= 2)
		sum += *wp;
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	return ~sum;
}

void
ping_encode_icmp(struct ping_data *ping, size_t bufsize)
{
	struct icmp *icmp;
	struct timeval tv;

	gettimeofday(&tv, NULL);

	icmp = (struct icmp *)ping->buffer;
	icmp->icmp_type = ping->type;
	icmp->icmp_code = 0;
	icmp->icmp_cksum = 0;
	icmp->icmp_id = htons(ping->id);
	icmp->icmp_seq = htons(ping->num_xmit);
	memcpy(icmp->icmp_data, &tv, sizeof(tv));

	icmp->icmp_cksum = icmp_cksum(ping->buffer, bufsize);
}


int
ping_setbuf(struct ping_data *ping, size_t size)
{
	if (ping->buffer == NULL)
	{
		ping->buffer = malloc(size);
		if (ping->buffer == NULL)
			return 1;
	}
	return 0;
}

int
ping_xmit(struct ping_data *ping)
{
	size_t bufsize;
	ssize_t nsent;

	bufsize = ping->datalen + ICMP_MINLEN;
	if (ping_setbuf(ping, bufsize))
		return 1;

	ping_encode_icmp(ping, bufsize);

	nsent = sendto(ping->fd, ping->buffer, bufsize, 0,
		(struct sockaddr *)&ping->dest_addr, sizeof(struct sockaddr_in));
	if (nsent < 0)
		return 1;
	++ping->num_xmit;
	return 0;
}

void
tvsub(struct timeval *out, struct timeval *in)
{
	if ((out->tv_usec -= in->tv_usec) < 0)
	{
		--out->tv_sec;
		out->tv_usec += 1000000;
	}
	out->tv_sec -= in->tv_sec;
}

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
				return -1;
			ping_echo_print(ping, ip, icmp, nrecv);
			break;

		case ICMP_TIME_EXCEEDED:
			ping_icmp_print(ping, ip, icmp, nrecv, "Time to live exceeded");
			break;
	}
	return 0;
}

void
ping_print_stat(struct ping_data *ping, char *hostname)
{
	double avg;
	double vari;

	printf("--- %s ping statistics ---\n", hostname);
	printf("%lu packets transmitted, ", ping->num_xmit);
	printf("%lu packets received", ping->num_recv);

	if (ping->num_xmit)
	{
		printf(", %d%% packet loss",
			(int)((ping->num_xmit - ping->num_recv) * 100 / ping->num_xmit));
	}
	printf("\n");

	if (ping->num_recv)
	{
		avg = ping->ping_stat.tsum / ping->num_recv;
		vari = ping->ping_stat.tsumsq / ping->num_recv - avg * avg;

		printf("round-trip min/avg/max/stddev = %.3f/%.3f/%.3f/%.3f ms\n",
		ping->ping_stat.tmin,
		avg,
		ping->ping_stat.tmax,
		sqrt(vari));
	}
}


void
ping_reset_data(struct ping_data *ping)
{
	ping->ping_stat.tmin = 0.0;
	ping->ping_stat.tmax = 0.0;
	ping->ping_stat.tsum = 0.0;
	ping->ping_stat.tsumsq = 0.0;

	ping->num_xmit = 0;
	ping->num_recv = 0;

	free(ping->buffer);
	ping->buffer = NULL;
}

void
sig_handler(int signal)
{
	if (signal == SIGINT)
		g_stop = 1;
}

int
ping_loop(struct ping_data *ping, char *hostname)
{
	fd_set fdset;
	int fdmax, nfds;

	signal(SIGINT, sig_handler);

	if (ping_xmit(ping))
		error(EXIT_FAILURE, errno, "sending packet");

	fdmax = ping->fd + 1;

	while (!g_stop)
	{
		FD_ZERO(&fdset);
		FD_SET(ping->fd, &fdset);

		nfds = select(fdmax, &fdset, NULL, NULL, NULL);
		if (nfds == -1)
			error(EXIT_FAILURE, errno, "select failed");
		else if (nfds == 1)
		{
			if (!ping_recv(ping))
			{
				usleep(ping->interval * 1000);
				if (!g_stop && ping_xmit(ping))
					error(EXIT_FAILURE, errno, "sending packet");
			}
		}
	}

	ping_print_stat(ping, hostname);
	ping_reset_data(ping);
	return 0;
}

int
ping_run(struct ping_data *ping, char *hostname)
{
	if (ping_set_dest(ping, hostname))
		error(EXIT_FAILURE, 0, "unknown host");

	printf("PING %s (%s): %zu data bytes",
		hostname,
		inet_ntoa(ping->dest_addr.sin_addr),
		ping->datalen);
	if (g_ping_options & PING_OPTION_VERBOSE)
		printf(", id 0x%x = %i", ping->id, ping->id);
	printf("\n");

	return ping_loop(ping, hostname);
}

struct ping_data *
ping_init()
{
	int fd;
	struct protoent *proto;
	struct ping_data *ping;

	proto = getprotobyname("icmp");
	if (proto == NULL)
	{
		perror("getprotobyname");
		return NULL;
	}

	fd = socket(AF_INET, SOCK_RAW, proto->p_proto);
	if (fd < 0)
	{
		perror("socket");
		return NULL;
	}

	ping = malloc(sizeof(struct ping_data));
	if (ping == NULL)
	{
		close(fd);
		return NULL;
	}

	memset(ping, 0, sizeof(*ping));
	ping->fd = fd;
	ping->type = ICMP_ECHO;
	ping->id = getpid();
	ping->interval = PING_DEFAULT_INTERVAL;
	ping->datalen = PING_DEFAULT_DATALEN;
	ping->count = PING_DEFAULT_COUNT;
	return ping;
}

enum {
	ARG_TTL = 256,
};

size_t
ping_cvt_number(const char *arg, size_t maxval)
{
	char *endptr;
	unsigned long int n;

	n = strtoul(arg, &endptr, 0);
	if (*endptr)
		error(EXIT_FAILURE, 0, "invalid value (`%s' near `%s')", arg, endptr);

	if (n == 0)
		error(EXIT_FAILURE, 0, "option value too small: %s", arg);

	if (maxval && n > maxval)
		error(EXIT_FAILURE, 0, "option value too big: %s", arg);
	return n;
}


static error_t
parse_opt(int key, char *arg,
	struct argp_state *state)
{
	switch(key)
	{
		case 'v':
			g_ping_options |= PING_OPTION_VERBOSE;
			break;

		case ARG_TTL:
			g_ttl = ping_cvt_number(arg, PING_DEFAULT_MAXTTL);
			break;

		case ARGP_KEY_NO_ARGS:
			argp_error(state, "missing host operand");

		/* fallthrough */
		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

int
main(int argc, char **argv)
{
	int index;
	struct ping_data *ping;

	char args_doc[] = "HOST ...";
	char doc[] = "Send ICMP ECHO_REQUESTED packets to network hosts.";
	struct argp_option argp_options[] = {
		{"verbose", 'v', NULL, 0, "verbose output", 0},
		{"ttl", ARG_TTL, "N", 0, "specify N as time-to-alive", 0},
		{0}
	};
	struct argp argp =
		{argp_options, parse_opt, args_doc, doc, NULL, NULL, NULL};

	if (argp_parse(&argp, argc, argv, 0, &index, NULL) != 0)
		return 0;

	ping = ping_init();
	if (ping == NULL)
		exit(EXIT_FAILURE);

	if (g_ttl > 0)
		if (setsockopt(ping->fd, IPPROTO_IP, IP_TTL, &g_ttl, sizeof(g_ttl)) < 0)
			error(0, errno, "setsockopt(IP_TTL)");

	argv += index;
	argc -= index;

	while (argc--)
		ping_run(ping, *argv++);

	close(ping->fd);
	free(ping);
	return 0;
}
