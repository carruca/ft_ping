#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>

#define BUFFER_SIZE 65536

unsigned char
	*create_buffer()
{
	unsigned char *buff;

	buff = malloc(BUFFER_SIZE);
	if (!buff)
	{
		return NULL;
	}
	return buff;
}

int
  main()
{
	int sock_r;
	unsigned char *buffer;
	struct sockaddr saddr;
	int saddr_len = sizeof(saddr);
	int buflen;
	struct ethhdr *eth;
	FILE *log_txt = NULL;

	sock_r = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (sock_r < 0)
	{
		printf("Error in socket creation: %s\n", strerror(errno));
		return -1;
	}
	printf("Raw socket created\n");
	buffer = create_buffer();
	if (!buffer)
	{
		close(sock_r);
		return -1;
	}
	memset(buffer, 0, BUFFER_SIZE);
	buflen = recvfrom(sock_r, buffer, BUFFER_SIZE, 0, &saddr, (socklen_t *)&saddr_len);
	if (buflen < 0)
	{
		printf("Error in recvfrom function\n");
		free(buffer);
		close(sock_r);
		return -1;
	}
	eth = (struct ethhdr *)buffer;
	log_txt = fopen("./log.txt", "w");
	if (!log_txt) 
	{
		printf("File not created\n");
		free(buffer);
		close(sock_r);
		return -1;
	}
	fprintf(log_txt, "Ethernet Header\n");
	fprintf(log_txt, "\t| -Source Adress : %.2X-%.2X-%.2X-%.2X-%.2X-%.2X\n",
		eth->h_source[0], eth->h_source[1],
		eth->h_source[2], eth->h_source[3],
		eth->h_source[4], eth->h_source[5]);
	fprintf(log_txt, "\t| -Destination Adress : %.2X-%.2X-%.2X-%.2X-%.2X-%.2X\n",
		eth->h_dest[0], eth->h_dest[1],
		eth->h_dest[2], eth->h_dest[3],
		eth->h_dest[4], eth->h_dest[5]);
	fprintf(log_txt, "\t| -Protocol: %d\n", eth->h_proto);

	unsigned short iphdrlen;
	struct iphdr *ip = (struct iphdr *)(buffer + sizeof(struct ethhdr));
	struct sockaddr_in src;
	struct sockaddr_in dst;

	memset(&src, 0, sizeof(src));
	src.sin_addr.s_addr = ip->saddr;
	memset(&dst, 0, sizeof(dst));
	dst.sin_addr.s_addr = ip->saddr;
	fprintf(log_txt, "IP Header\n");
	fprintf(log_txt, "\t| -Version : %d\n", (unsigned int)ip->version);
	fprintf(log_txt, "\t| -Internet Header Length : %d DWORDS or %d Bytes\n",
		(unsigned int)ip->ihl,
		((unsigned int)(ip->ihl))*4);
	fprintf(log_txt, "\t| -Type of Service : %d\n", (unsigned int)ip->tos);
	fprintf(log_txt, "\t| -Total Length : %d Bytes\n", ntohs(ip->tot_len));
	fprintf(log_txt, "\t| -Identification : %d\n", ntohs(ip->id));
	fprintf(log_txt, "\t| -Time To Live : %d\n", (unsigned int)ip->ttl);
	fprintf(log_txt, "\t| -Protocol : %d\n", (unsigned int)ip->protocol);
	fprintf(log_txt, "\t| -Header Checksum : %d\n", ntohs(ip->check));
	fprintf(log_txt, "\t| -Source IP : %s\n", inet_ntoa(src.sin_addr));
	fprintf(log_txt, "\t| -Destination IP : %s\n", inet_ntoa(dst.sin_addr));
	iphdrlen = (ip->ihl)*4;
	struct udphdr *udp = (struct udphdr *)(buffer + iphdrlen + sizeof(struct ethhdr));
	fprintf(log_txt, "UDP Header\n");
	fprintf(log_txt, "\t| -Source Port : %d\n", ntohs(udp->source));
	fprintf(log_txt, "\t| -Destination Port : %d\n", ntohs(udp->dest));
	fprintf(log_txt, "\t| -UDP Length : %d\n", ntohs(udp->len));
	fprintf(log_txt, "\t| -UDP Checksum : %d\n", ntohs(udp->check));

	unsigned char *data = buffer + iphdrlen + sizeof(struct ethhdr) + sizeof(struct udphdr);



	fflush(log_txt);
	fclose(log_txt);

	free(buffer);
	close(sock_r);
	return 0;
}
