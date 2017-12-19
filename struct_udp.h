#include <stdint.h>
#include <stdio.h> 


struct client{
	  int clientfd;
	  int fd;
	  int blocknum;  
};


struct PacketRRQ
{
	char *filename;
	uint16_t opcode;
	char mode[512];
};

