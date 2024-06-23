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

#define MSG_SIZE	64

struct ping
{
	int fd;
	int type;
	size_t count;
	size_t interval;
	size_t length;
	char *hostname;
	struct sockaddr_in dest;
	struct sockaddr_in from;
};

struct packet
{
	struct icmp icmp;	
};

struct ping *
init_ping()
{
	int sockfd;
	struct protoent *proto;
	struct ping *p;

	proto = getprotobyname("icmp");
	if (!proto)
	{
		perror("getprotobyname");
		return NULL;
	}

	sockfd = socket(AF_INET, SOCK_RAW, proto->p_proto);
	if (sockfd < 0)
	{
		perror("socket");
		return NULL;
	}

	p = malloc(sizeof(struct ping));
	if (!p)
	{
		return NULL;
	}

	memset(p, 0, sizeof(*p));
	p->fd = sockfd;
	return p;
}

int
main(int argc, char **argv)
{
	if (argc != 2)
	{
		printf(
			"%s: usage error: Destination address required\n",
			argv[0] + 2);
		return 1;
	}

	// DNS resolver
	struct hostent *host;
	struct in_addr **addr_list;

	host = gethostbyname(argv[1]);
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
	char msg[MSG_SIZE];
	bzero(msg, MSG_SIZE);
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
	return 0;
}
