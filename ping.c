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
#include <netdb.h>

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
	for (int i = 0; addr_list[i]; ++i)
	{
		printf("%s\n", inet_ntoa(*addr_list[i]));
	}

	int sockfd;

	sockfd = socket(AF_PACKET, SOCK_RAW, IPPROTO_ICMP);
	if (sockfd < 0)
	{
		perror("socket");
		return 1;
	}

	close(sockfd);
	return 0;
}
