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

#define PING_OPTION_VERBOSE 				0x0001

#define PING_DEFAULT_COUNT 		0
#define PING_DEFAULT_INTERVAL 1000
#define PING_DEFAULT_DATALEN 	(64 - ICMP_MINLEN)

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
	if (!ping->buffer)
	{
		ping->buffer = malloc(size);
		if (!ping->buffer)
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

int
ping_print_timing(struct ping_data *ping,
	struct ip *ip, struct icmp *icmp, int datalen)
{
	struct timeval tv_out, *tv_in;
	double triptime;

	gettimeofday(&tv_out, NULL);
	tv_in = (struct timeval *)icmp->icmp_data;
	tvsub(&tv_out, tv_in);
	triptime = tv_out.tv_sec * 1000.0 + tv_out.tv_usec / 1000.0;
	ping_set_stat(&ping->ping_stat, triptime);

	printf("%i bytes from %s: icmp_seq=%u ttl=%i time=%.3f ms\n",
		datalen,
		inet_ntoa(ping->from_addr.sin_addr),
		ntohs(icmp->icmp_seq),
		ip->ip_ttl,
		triptime);
	ping->num_recv++;
	return 0;
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

	if (icmp->icmp_type == ICMP_ECHOREPLY)
		ping_print_timing(ping, ip, icmp, nrecv);

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
ping_reset(struct ping_data *ping)
{
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

	fdmax = ping->fd + 1;
	while (!g_stop)
	{
		if (ping_xmit(ping))
			error(EXIT_FAILURE, errno, "sending packet");

		FD_ZERO(&fdset);
		FD_SET(ping->fd, &fdset);
		nfds = select(fdmax, &fdset, NULL, NULL, NULL);
		if (nfds == -1)
			error(EXIT_FAILURE, errno, "select failed");
		else if (nfds == 1)
			ping_recv(ping);
		if (ping->count == ping->num_xmit)
				break;

		usleep(ping->interval * 1000);
	}

	ping_print_stat(ping, hostname);
	ping_reset(ping);
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
	if (!proto)
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
	if (!ping)
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
	ping->num_recv = 0;
	ping->num_xmit = 0;
	return ping;
}

static error_t
parse_opt(int key, char *arg,
	struct argp_state *state)
{
	(void)arg;
	switch(key)
	{
		case 'v':
			g_ping_options |= PING_OPTION_VERBOSE;
			break;

		case ARGP_KEY_NO_ARGS:
			argp_error(state, "missing host operand");

		/* fallthrough */
		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}


struct argp_option argp_options[] = {
	{"verbose", 'v', NULL, 0, "verbose output", 0},
	{0}
};

struct argp argp =
	{argp_options, parse_opt, NULL, NULL, NULL, NULL, NULL};

int
main(int argc, char **argv)
{
	int status;
	struct ping_data *ping;

	int index;
	if (argp_parse(&argp, argc, argv, 0, &index, NULL) != 0)
		return 0;

	argv += index;
	argc -= index;

	ping = ping_init();
	if (ping == NULL)
		exit(EXIT_FAILURE);

	while (argc--)
		status |= ping_run(ping, *argv++);

	close(ping->fd);
	free(ping);
	return status;
}
