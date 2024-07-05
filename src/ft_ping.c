#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <netinet/ip_icmp.h>
#include <netdb.h>
#include <argp.h>

#define DEFAULT_COUNT 0
#define OPTION_VERBOSE 0x0001

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
	size_t count;
	size_t interval;
	size_t datalen;
	char *hostname;
	struct sockaddr_in dest;
	struct sockaddr_in from;
};

size_t ping_options;

void
ping_echo(struct ping_data *ping, char *hostname)
{
	struct ping_stat ping_stat;

	memset(&ping_stat, 0, sizeof(ping_stat));
	(void)ping;
	(void)hostname;
}

struct ping_data *
init_ping()
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
	ping->count = DEFAULT_COUNT;
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
	argc += index;

	ping = init_ping();
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
