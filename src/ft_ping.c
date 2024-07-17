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

#define OPTION_VERBOSE 				0x0001
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
	size_t num_xmit; /* packets transmitted */
	size_t interval;
	size_t datalen;
	struct sockaddr_in dest_addr;
	struct sockaddr_in from;
	char *buffer;
};

unsigned ping_options;
_Bool stop;

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
ping_icmp_cksum(char *buffer, size_t bufsize)
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

	icmp = (struct icmp *)ping->buffer;
	icmp->icmp_type = ping->type;
	icmp->icmp_code = 0;
	icmp->icmp_cksum = 0;
	icmp->icmp_id = htons(ping->id);
	icmp->icmp_seq = htons(ping->num_xmit);

	icmp->icmp_cksum = ping_icmp_cksum(ping->buffer, bufsize);
}


int
ping_setbuf(struct ping_data *ping, size_t size)
{
	ping->buffer = malloc(size);
	if (!ping->buffer)
		return 1;
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
	free(ping->buffer);
	return 0;
}

void
ping_recv()
{
}

int
ping_loop(struct ping_data *ping)
{
	fd_set fdset;
	int fdmax, nfds;

	fdmax = ping->fd + 1;

	while (1)
	{

		FD_ZERO(&fdset);
		FD_SET(ping->fd, &fdset);
		nfds = select(fdmax, &fdset, NULL, NULL, NULL);
		if (nfds == -1)
			error(EXIT_FAILURE, errno, "select failed");
		else if (nfds == 1)
		{
			ping_recv();
		}
	}
	return 0;
}

int
ping_run(struct ping_data *ping)
{
	if (ping_xmit(ping))
		error(EXIT_FAILURE, errno, "sending packet");

	ping_loop(ping);
	return 0;
}

void
ping_print_dns(struct ping_data *ping, char *hostname)
{
	printf("PING %s (%s): %zu data bytes",
		hostname,
		inet_ntoa(ping->dest_addr.sin_addr),
		ping->datalen);
	if (ping_options & OPTION_VERBOSE)
		printf(", id 0x%x = %i", ping->id, ping->id);
	printf("\n");
}

int
ping_echo(struct ping_data *ping, char *hostname)
{
	int status;

	if (ping_set_dest(ping, hostname))
		error(EXIT_FAILURE, 0, "unknown host");

	ping_print_dns(ping, hostname);

	status = ping_run(ping);
	return status;
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
	ping->datalen = PING_DEFAULT_DATALEN;
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
			ping_options |= OPTION_VERBOSE;
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
	struct ping_data *ping;

	int index;
	struct argp_option argp_options[] = {
		{"verbose", 'v', NULL, 0, "verbose output", 0},
		{0}
	};

	struct argp argp =
		{argp_options, parse_opt, NULL, NULL, NULL, NULL, NULL};

	if (argp_parse(&argp, argc, argv, 0, &index, NULL) != 0)
		return 0;

	argv += index;
	argc -= index;

	ping = ping_init();
	if (ping == NULL)
		exit(EXIT_FAILURE);

	while (argc--)
		ping_echo(ping, *argv++);

	close(ping->fd);
	free(ping);
	return 0;
/*
	struct hostent *host;
	struct in_addr **addr_list;

	host = gethostbyname(*argv);
	if (host == NULL)
	{
		herror("gethostbyname");
		return 1;
	}

	printf("hostent struct:\n\t name: %s, addrtype: %d, length: %d\n",
		host->h_name,
		host->h_addrtype,
		host->h_length);
	printf("addrlist:\n");
	addr_list = (struct in_addr **)host->h_addr_list;
	char	dest_addr[14];
	char	src_addr[14];
	socklen_t	src_addrlen = sizeof(src_addr);
	for (int i = 0; addr_list[i]; ++i)
	{
		printf("%s\n", inet_ntoa(*addr_list[i]));
	}
	strcpy(dest_addr, inet_ntoa(*addr_list[0]));

	// socket
	int sockfd;

	sockfd = socket(AF_PACKET, SOCK_RAW, IPPROTO_ICMP);
	if (sockfd < 0)
	{
		perror("socket");
		return 1;
	}

	//loop
	//handle CTRL + C

	int ttl = 64;
	if (setsockopt(sockfd, SOL_SOCKET, IP_TTL, &ttl, sizeof(ttl)) < 0)
	{
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	// ICMP
	char msg[DATASIZE];
	bzero(msg, DATASIZE);
	struct icmp *icmp_msg = (struct icmp *)msg;
	printf("struct icmp size = %lu\nstruct icmphdr size = %lu\n",
		sizeof(struct icmp),
		sizeof(struct icmphdr));
	icmp_msg->icmp_type = ICMP_ECHO;
	icmp_msg->icmp_code = 0;
	icmp_msg->icmp_cksum = 0;
	icmp_msg->icmp_id = getpid();
	icmp_msg->icmp_seq = 1;

	sendto(sockfd, icmp_msg, sizeof(icmp_msg), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
	recvfrom(sockfd, icmp_msg, sizeof(icmp_msg), 0, (struct sockaddr *)&src_addr, &src_addrlen);
	close(sockfd);
	*/
}
