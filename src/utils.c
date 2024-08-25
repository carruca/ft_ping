#include "ft_ping.h"

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
tvsub(struct timeval *out, struct timeval *in)
{
	if ((out->tv_usec -= in->tv_usec) < 0)
	{
		--out->tv_sec;
		out->tv_usec += 1000000;
	}
	out->tv_sec -= in->tv_sec;
}

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
