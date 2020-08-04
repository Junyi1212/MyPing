#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netdb.h>
#include <setjmp.h>
#include <error.h>
#include <string.h>

#define PACKET_SIZE 4096
#define MAX_WAIT_TIME 5
#define MAX_NO_PACKETS 3

char sendpacket[PACKET_SIZE];
char recvpacket[PACKET_SIZE];
int sockfd,datalen=56;
int nsend=0,nreceived=0;
struct sockaddr_in dest_addr;
pid_t pid;

void statistics()
{
	printf("\n-----Ping statistics------\n");
	printf("%d ICMP packets sended,%d receive,%%%dlost\n",nsend,nreceived,(nsend-nreceived)/nsend*100);
	close(sockfd);
	exit(0);
}

unsigned short cal_chksum(unsigned short *addr,int len)
{	
	int nleft=len;
	int sum=0;
	unsigned short *w=addr;
	unsigned short answer=0;
	while(nleft>1)
	{
		sum+=*w++;
		nleft-=2;
	}
	if(nleft==1)
	{
		*(unsigned char *)(&answer)=*(unsigned char *)w;
		sum+=answer;
	}
	sum=(sum>>16)+(sum&0xffff);
	sum+=(sum>>16);
	answer=~sum;
	return answer;
}

int pack(int pack_no)
{
	int i,packsize;
	struct icmp *icmp;

	icmp=(struct icmp *)sendpacket;
	pid=getpid();
	icmp->icmp_type=ICMP_ECHO;
	icmp->icmp_code=0;
	icmp->icmp_cksum=0;
	icmp->icmp_id=pid;
	icmp->icmp_seq=pack_no;
	packsize=8+datalen;
	icmp->icmp_cksum=cal_chksum((unsigned short *)icmp,packsize);

	return packsize;
}

void send_packet()
{	
	int packetsize;

	while(nsend<MAX_NO_PACKETS)
	{
		nsend++;
		packetsize=pack(nsend);
		if((sendto(sockfd,sendpacket,packetsize,0,(struct sockaddr *)&dest_addr,sizeof(dest_addr)))<0)
		{
			perror("Send ICMP packets error:");
			continue;
		}
		printf("#%d ICMP packet sended to  %s\n",nsend,inet_ntoa(dest_addr.sin_addr));
		sleep(1);
	}
}

int unpack(char *buf,int len)
{	
	int i,iphdrlen;
	struct ip *ip;
	struct icmp *icmp;
	long *lp;
	ip=(struct ip*)buf;
	iphdrlen=ip->ip_hl<<2;
	icmp=(struct icmp *)(buf+iphdrlen);
	len-=iphdrlen;
	if(len<8)
	{
		printf("ICMP packets\' length is less than 8\n");
		return(-1);	
	}
	if((icmp->icmp_type==ICMP_ECHOREPLY)&&(icmp->icmp_id==pid))
		return icmp->icmp_seq;
	else	  
		return -1;
}

void recv_packet()
{	
	int n,fromlen,packet_no;
	struct sockaddr_in from;
	printf("\n");
	signal(SIGALRM,statistics);
	while(nreceived<nsend)
	{
		fromlen=sizeof(from);
		alarm(MAX_WAIT_TIME);   	
		if((n=recvfrom(sockfd,recvpacket,sizeof(recvpacket),0,(struct sockaddr *)&from,&fromlen))<0)
		{
			perror("Receive packet error");
			continue;	
		}
		packet_no=unpack(recvpacket,n);
		if(packet_no==-1)	continue;
		printf("#%d packet\'s reply received from %s\n",packet_no,inet_ntoa(from.sin_addr));
		nreceived++;
	}
}

int main(int argc,char *argv[])
{
	struct hostent *host;
	struct protoent *protocol;
	unsigned long inaddr=0L;
	int waittime=MAX_WAIT_TIME;
	int size=50*1024;

	if(argc<1)
	{
		printf("Usage: %s hostname | IPaddress\n",argv[0]);
		exit(1);
	}
	protocol=getprotobyname("icmp");
	if((sockfd=socket(AF_INET,SOCK_RAW,protocol->p_proto))<0)
	{
		perror("Socket error:");
		exit(3);
	}
	setsockopt(sockfd,SOL_SOCKET,SO_RCVBUF,&size,sizeof(size));
	bzero(&dest_addr,sizeof(dest_addr));
	dest_addr.sin_family=AF_INET;
	if((inaddr=inet_addr(argv[1]))==INADDR_NONE)
	{
		if((host=gethostbyname(argv[1]))==NULL)
		{
			perror("hostname error");
			exit(4);
		}
		memcpy((char *)&dest_addr.sin_addr,host->h_addr,host->h_length);
	}
	else
	{
		memcpy((char *)&dest_addr.sin_addr,(char*)&inaddr,sizeof(inaddr));
	}
	send_packet();
	recv_packet();
	statistics();

	return 0;
}








