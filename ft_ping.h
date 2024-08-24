#ifndef FT_PING_H
# define FT_PING_H

# include <sys/types.h>
# include <sys/time.h>
# include <sys/select.h>
# include <sys/socket.h>
# include <arpa/inet.h>
# include <linux/if_ether.h>
# include <stdio.h>
# include <errno.h>
# include <string.h>
# include <stdlib.h>
# include <unistd.h>
# include <netinet/ip.h>
# include <netinet/ip_icmp.h>
# include <netdb.h>
# include <argp.h>
# include <error.h>
# include <math.h>
# include <signal.h>
# include <stdlib.h>

# define PING_OPTION_VERBOSE    0x0001

# define PING_DEFAULT_COUNT 		0
# define PING_DEFAULT_INTERVAL  1000
# define PING_DEFAULT_DATALEN 	(64 - ICMP_MINLEN)
# define PING_DEFAULT_MAXTTL 	  255

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

#endif
