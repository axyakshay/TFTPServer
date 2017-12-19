
#include <sys/socket.h>
#include <stdio.h>
#include <fcntl.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include <assert.h>
#include <string.h>
#include <sys/types.h> 
#include <netinet/in.h>
#include <stdlib.h>
#include "struct_udp.h"

#include <assert.h>
#include <unistd.h>
#define Datasizemax 512
#define Filesizemax 1000428800

int countClients=0;
struct PacketRRQ RRQPacket;

#define Clients_max 10

struct client ListClients[Clients_max];

//Sending ACK in response to WRQ/DATA
void ACK_Send(int fd, int ack_num, int sockfd, struct sockaddr_in clientaddr, int clientlen ) 
{
   int opcode= 4, n;
   
   char buftx[4];
   bzero(buftx, 4);
   *(int *)buftx= htons(opcode);
   *(int *)(buftx+2)=htons(ack_num);
	printf("Ack Packet sent \n");
	
   n = sendto(sockfd, buftx, 4, 0, (struct sockaddr *) &clientaddr, clientlen ) ;
   if(n<0){
		perror("Error in sendto\n");
		exit(1);
   }
}

//Error message
int error_Send(struct sockaddr_in clientaddr, int clientlen , int errorcode, int sockfd )
{
   char buftx[512];
   
   int length ,opcode_error = 5, n;
   
   bzero(buftx, 512);
   *(int *)(buftx)=htons(opcode_error);
   *(int *)(buftx+2)= htons(errorcode);
   
   if(errorcode==1){
        strncpy(buftx+4, "No file found" , 13);
        length = 17;
		buftx[length]='\0';
   }
   else if(errorcode==2){
        strncpy(buftx+4, "exceeding max size", 18);
        length = 22;
		buftx[length]='\0';
   }		
   else {
        return 0;
		buftx[length]='\0';
   }

   n = sendto(sockfd, buftx, length+1, 0, (struct sockaddr *) &clientaddr, clientlen ) ;
   if(n<0){
		perror(" Error: sendto not working properly \n");
		exit(1);
	}
   return 0;
}  


//Data is sent in blocks of 512
int data_Send(int fd, int block_num, int sockfd, struct sockaddr_in clientaddr, int clientlen ) {
	int check=0, value=1, opcode= 3, n;
	char buftx[516] ;
	char *temp = (char*) malloc (Datasizemax);;
	bzero(buftx, 516);
	*(int *)buftx = htons(opcode);
	*(int *)(buftx+2)=htons(block_num);
	
	check = read (fd, temp, Datasizemax);
	
	if(check<0)
		printf("Error in read \n");
	
	else if(check<Datasizemax){
         close(fd);
		 value=0;
		 printf("File transferred completely\n");
	}
	else{
		  temp[check]='\0';
	}
	memcpy(buftx+4,temp,check);
	n = sendto(sockfd, buftx, check+4, 0, (struct sockaddr *) &clientaddr, clientlen ) ;
	
	if(n<0){
		perror(" ERROR: sendto not working properly\n");
		exit(1);
	}
   return value;
}


//Clientadd
void Clientadd(int fd, int clientfd)
{
	ListClients[countClients].clientfd=clientfd;
	printf("Clientfd added is %d \n", clientfd);
    ListClients[countClients].fd=fd;
    ListClients[countClients].blocknum=1;
    countClients++;
    printf("Client's count now = %d \n", countClients);
}

//clientremove
void removeClient(int clientfd){
	int i, k;
	for (i=0; i <countClients; i++){
		if (clientfd==ListClients[i].clientfd){
			for (k=i; k <(countClients-1);k++){
				ListClients[i]=ListClients[i+1]; //Client removal
			}
		} 
		countClients--;
		break;
    }
}	
		
//Client search
struct client* Clientsearch(int fd_client){
      int i=0;
      while(i <  countClients){
            if(ListClients[i].clientfd==fd_client){  
		        printf("Client %d found at index %d \n", fd_client, i);
            	return &ListClients[i];   
            }
	  i++;
      }
      return NULL;
} 

int readtimeout(int sec, int usec, int fd)
{
	fd_set	reset;
	FD_ZERO(&reset);
	FD_SET(fd, &reset);
	
	struct timeval	tv;
	tv.tv_sec = sec;
	tv.tv_usec = usec;
	
	int return_value;
    return_value= select(fd+1, &reset, NULL, NULL, &tv);
	return return_value;
}

int socketcreate()
{
	  struct sockaddr_in clientaddr;
      int clientfd, portnum=0;
	 
	  if ((clientfd = socket(AF_INET, SOCK_DGRAM, 0) ) <0){
		  perror("ERROR: Can't create the socket \n");
		  exit(1);
	  }
	  else
		printf("Socket successfully created. clientfd = %d\n", clientfd);

	  int enable = 1;
	  setsockopt(clientfd, SOL_SOCKET, SO_REUSEADDR,(const void *)&enable , sizeof(int));

	  bzero((char *) &clientaddr, sizeof(clientaddr));
	  clientaddr.sin_family = AF_INET;
	  clientaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	  clientaddr.sin_port = htons((unsigned short)portnum);

	  if (bind(clientfd, (struct sockaddr *) &clientaddr,sizeof(clientaddr)) < 0){
	  	perror("ERROR on binding for client");
		exit(1);
	  }
      else
           printf(" The address is successfully bound \n");
      return clientfd;
}

int main(int argc, char **argv) 
{

	  int sockfd, clientfd, clientfd_new, transfer_incomplete=1,i, n, am , km, count, blocknum, portno, fd1, fd2, len, counter, lsize, k, retry, exec; 
	  char c, nextchar, filename[512], mode[512], write1[512];
	  struct sockaddr_in serveraddr, clientaddr; // Address of Server and Client
	  struct client *client1;
	  socklen_t clientlen;
	  clientlen=sizeof(clientaddr);
	  mode_t mode2 = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP| S_IROTH | S_IWOTH;

	  pid_t recvpid;
 	  FILE * fp1, *fp2;
	
      struct hostent* hret;	
      uint16_t opcode, acknum; 
      char * buff, *ptr=NULL, *temp;
	  buff=(char *)malloc(sizeof(struct PacketRRQ));

	  if (argc != 3) {
	    perror("please enter proper arguements");
	    exit(1);
	  }
	  portno = atoi(argv[2]);
	  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	  if (sockfd < 0) {
		perror("Error: Can't open the socket \n");
		exit(1);
	  }
	  else
		  printf("Socket creation successful \n");
     
      //Enables reuse of socket
	  int enable = 1;
	  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,(const void *)&enable , sizeof(int));


	  bzero((char *) &serveraddr, sizeof(serveraddr));
	  serveraddr.sin_family = AF_INET;
      hret = gethostbyname(argv[1]);
      memcpy(&serveraddr.sin_addr.s_addr, hret->h_addr,hret->h_length);
	  portno = atoi(argv[2]);
	  serveraddr.sin_port = htons((unsigned short)portno);

	  if (bind(sockfd, (struct sockaddr *) &serveraddr,sizeof(serveraddr)) < 0) {
	  	perror("Error: Couldn't bind the socket");
		exit(1);
	  }
      else  
		printf(" Socket successfully bound \n");

	  while (1){

							if((n = recvfrom(sockfd, buff, sizeof(struct PacketRRQ), 0,(struct sockaddr *)&clientaddr,&clientlen))>0){
									clientfd_new= socketcreate();	//Cfreates a new socket for a client request WRQ/RRQ
							        printf("Client %d is created \n", clientfd_new);
								if ( (recvpid = fork ()) == 0 ) {
										clientfd=clientfd_new;
										close(sockfd); //Closes the listening socket inside child process
										
									while(1)
										{
										bzero(&opcode,sizeof(uint16_t));
										memcpy(&opcode,buff,sizeof(uint16_t));
										
										opcode=ntohs(opcode);
										printf("Opcode= %d\n",opcode);
										
										if (opcode!= 1 && opcode!=4 && opcode!=2 && opcode!=3)
											printf("Invalid Opcode received\n");
										
										else if (opcode==1){

											printf(" RRQ request\n");
											
											bzero(&filename, 512);
											memcpy(&filename,buff + sizeof(uint16_t),512);
											am=2;
											count=0;
											while (buff[am]!='\0')
												am++;
											
											for (km=am+1; buff[km]!='\0';km++)
												count++;          //extracting the mode
											
											bzero(&mode, 512);
											memcpy(&mode, buff+am+1, count);
											
											printf("Mode = %s \n ", mode);
											len= strlen(filename);
											filename[len]='\0';
											printf("Filename = %s \n", filename);
											//fp1= fopen(filename, "r+");
											fd1= open(filename, O_RDWR);
											
											if (fd1<=0){			//File not found case
												n= error_Send(clientaddr, clientlen, 1, clientfd );
												printf("File not found. Child exiting\n");
												removeClient(clientfd);
												close(clientfd);
												_exit(0);
											}
								
											else {
												printf("File opening successful\n");
												/*fseek(fp1 , 0 , SEEK_END);
												lsize=ftell(fp1);
												fseek (fp1,  0 , SEEK_SET);
												*/
												
												lsize=lseek (fd1 , 0 , SEEK_END);
												lseek (fd1,  0 , SEEK_SET);
												
												printf("The file size is %d \n", lsize);
												//assert(0);
												if(lsize>Filesizemax){ //If the requested size is greater than the Maximum File size
													n=error_Send(clientaddr, clientlen, 2, clientfd );
													printf("Greater than the maximum file size. \n"); 
													removeClient(clientfd);
													close(clientfd);
													_exit(0);
													
												}
												else{
													/*
													ptr = NULL;
													if(!strcmp(mode, "netascii")){
														printf(" It is netascii!!! \n");
														printf("entered for netascii \n");
														transfer_incomplete=-1;
														//fp1=fdopen(fd1,"r+");
														for (counter = 0; counter <= lsize+1 ; counter++){
															if(transfer_incomplete >=0) {
																*ptr++ = nextchar;
																//printf(" inserted ptr position is %c \n", *ptr);	
																//ptr++;
																lsize++;
															    printf("size= %d at count =%d \n", lsize, counter);
																transfer_incomplete = -1;
																continue;
															}
															
															//printf("I am at %d \n", len);
															c= fgetc(fp1);
															//read(fd1, &c,1);
															printf("c = %c \n", c);
	
															if (c == EOF){
																*ptr++=c;
																printf("read err from getc on local file \n");
																break;
															}
															else if ( c == '\n') {
																printf ("LF \n");
																c='\r';
																nextchar = '\n';
																transfer_incomplete=0;
															}
															else if (c =='\r') {
																printf ("CR \n");
																nextchar = '\0';
																transfer_incomplete=0;
															}
															else{
																printf("wrong place \n");
																transfer_incomplete= -1;
															}
															*ptr = c;
															printf(" No CR encountered. ptr position is %c \n", *ptr);	
															assert(0);
															ptr++;
														}
													}
													//fseek ((FILE*) ptr,  0 , SEEK_SET);
													printf ("Outside \n");
													//exec= ftell((FILE *) temp);
													//assert(0);
													ptr=ptr-lsize-2;
													printf("I am now at position %c size %d \n", *ptr, lsize);
													fp1=fmemopen (ptr, lsize, "r+" );
													fd1=fileno(fp1);
													read(fd1,&c,1);
													printf("I am now at new position %c \n", c);
													//*/
													printf(" client1-> fd before = %d \n", fd1);
													Clientadd(fd1, clientfd); //Add the clientfd along with the file descriptot to the client list
													
													client1=Clientsearch(clientfd); //Retrieve the client details using the client file descriptor
													
													client1->blocknum = 1; //Set the initial Blocknum as 1
													
													//printf(" client1-> fd = %d \n", client1->fd);
													//assert(0);
													transfer_incomplete= data_Send(client1->fd, 1, clientfd, clientaddr, clientlen ); 
													//printf("transfer incomplete = %d \n", transfer_incomplete);
													/*if((client1->fd)<=0)
														printf("Non-positive file descriptor \n");*/
												}
											}
										}
										else if (opcode==2){
											printf(" WRQ \n");
											bzero(&filename, 512);
											memcpy(&filename,buff + sizeof(uint16_t),512);
											am=2;
											count=0;
											while (buff[am]!='\0')
												am++;
											
											for (km=am+1; buff[km]!='\0';km++)
												count++;
											
											bzero(&mode, 512);
											memcpy(&mode, buff+am+1, count);
											printf("Mode = %s \n ", mode);
											
											len= strlen(filename);
											filename[len]='\0';
											
											printf("Filename= %s \n", filename);
											fd2= open(filename, O_RDWR | O_CREAT , mode2); //Open the file and associate appropriate privileges using mode2
											
											Clientadd(fd2, clientfd);
											client1=Clientsearch(clientfd);
											
											ACK_Send(client1->fd, 0, clientfd, clientaddr, clientlen ); 
											/*if((client1->fd)<=0)
												printf("Non-positive file descriptor \n");*/
										}
										else if (opcode==3){
											printf ("DATA received from Client\n");
											bzero(&blocknum, sizeof(uint16_t));
											memcpy(&blocknum,buff + sizeof(uint16_t),sizeof(uint16_t));
											blocknum = htons(blocknum);
											printf("Blocknum from client = %d \n", blocknum);
		
											client1=Clientsearch(clientfd);
											if(client1!=NULL){
												
												if (blocknum == client1->blocknum){
													
													printf("Acknumber to be sent= %d \n ",client1->blocknum);

													bzero(&write1, 512);
													memcpy(&write1, buff + 2*sizeof(uint16_t),512);
					
													write(client1->fd, write1,n-4);	
													
													ACK_Send(client1->fd, client1->blocknum, clientfd, clientaddr, clientlen); 
													client1->blocknum = (client1->blocknum+1)%65536;
														
													
												} 
											}	
										}
										
										else{
											printf ("ACK from Client \n");
											bzero(&acknum, sizeof(uint16_t));
											memcpy(&acknum,buff + sizeof(uint16_t),sizeof(uint16_t));
											acknum = htons(acknum);
		
											client1=Clientsearch(clientfd);
											if(client1!=NULL){
												if (acknum == client1->blocknum){
													
													client1->blocknum = (client1->blocknum+1)%65536;
													printf("Ack from client= %d and next blocknum that we ll send = %d\n ",acknum, client1->blocknum);
													//printf("client->fd = %d \n", client1->fd);

													if(client1->fd<=0)
														printf("File transfer completed \n");
			
													else{
														    if (!transfer_incomplete)
																printf("File transfer completed \n");
															else{
																transfer_incomplete=data_Send(client1->fd, client1->blocknum, clientfd, clientaddr, clientlen);  
															}
													}	
												} 
											}
										}
										retry=0;
										
										//Attempting 10 timeout tries and each timeout try is 10 secs.
										if(transfer_incomplete){
										while(retry<10){
											printf("retrying %d th time for clientfd %d \n", retry, clientfd);
											k = readtimeout(10, 0, clientfd );
											if (k>0)
												break;
											else{
												//transfer_incomplete=data_Send(client1->fd, client1->blocknum, clientfd, clientaddr, clientlen);
												retry++;
											}
										}
														
										if (retry==10){ //Timeout case
											printf("This client %d has timed out \n" , clientfd);
											removeClient(clientfd);
											close(clientfd);
											_exit(0);
											break;
										}
										}
										n = recvfrom(clientfd, buff, sizeof(struct PacketRRQ), 0,(struct sockaddr *)&clientaddr,&clientlen);
										if (n<0){
											printf("Error in recvfrom\n");
											break;
										}
									}
								}
							}
          
	}//while 1
}


