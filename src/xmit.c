#include "ft_ping.h"

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
