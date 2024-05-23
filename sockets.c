#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define BUFFER_SIZE 65536

int
  main()
{
  int sock_r;
  unsigned char *buffer;
  struct sockaddr saddr;
  int saddr_len = sizeof(saddr);
  int buflen;

  sock_r = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
  if (sock_r < 0)
  {
    printf("Error in socket creation: %s\n", strerror(errno));
    return -1;
  }

  printf("Raw socket created\n");
  buffer = malloc(BUFFER_SIZE);
  if (!buffer)
  {
    close(sock_r);
    return -1;
  }

  memset(buffer, 0, BUFFER_SIZE);
  buflen = recvfrom(sock_r, buffer, BUFFER_SIZE, 0, &saddr, (socklen_t *)&saddr_len);
  if (buflen < 0) {
    printf("Error in recvfrom function");
    free(buffer);
    close(sock_r);
    return -1;
  }

  free(buffer);
  close(sock_r);
  return 0;
}
