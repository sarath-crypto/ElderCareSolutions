#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>

#include <math.h>
#include <fstream>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <sstream>

#include <syslog.h>

#include "udps.h"

//#define	DEBUG		1

using namespace std;
extern bool ex;

udps::~udps(){
	close(sockfd);
}
udps::udps(unsigned short port){
	state = true;
	if((sockfd = socket(AF_INET,SOCK_DGRAM, 0)) < 0 ){
        	state = false;
		return;
	}
	bzero(&seraddr,sizeof(seraddr));
	bzero(&cliaddr,sizeof(cliaddr));
	seraddr.sin_family  = AF_INET;
	seraddr.sin_addr.s_addr = INADDR_ANY;
	seraddr.sin_port = htons(port);
	if(bind(sockfd, (const struct sockaddr *)&seraddr,sizeof(seraddr)) < 0 ){ 
        	state = false;
		return;
    	}	 
	con = false;
	sm = STOP;
	beacon_try = 0;
	timeout.tv_sec = 0;
        timeout.tv_usec = 10;
        setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
}

void udps::recv(void){
	int n = -1;
	socklen_t len  = sizeof(cliaddr);
	unsigned char buffer[BUF_LEN];
	n = recvfrom(sockfd,(char *)buffer,BUF_LEN,MSG_WAITALL,( struct sockaddr *) &cliaddr,&len); 
	if(n > 0){
		pdu p;
		memcpy((void *)&p,buffer,n);
		if(n == p.len){
#ifdef	DEBUG
			//printf("%s RECV->BYTES:%d TYPE:%d S:%d Q:%d K:%d\n",inet_ntop(AF_INET,&cliaddr.sin_addr,(char *)&buffer,BUF_LEN),n,(int)p.type,sm,rxfifo.size(),beacon_try);
			syslog(LOG_INFO,"%s RECV->BYTES:%d TYPE:%d S:%d Q:%d K:%d\n",inet_ntop(AF_INET,&cliaddr.sin_addr,(char *)&buffer,BUF_LEN),n,(int)p.type,sm,rxfifo.size(),beacon_try);
#endif
			rip = string(inet_ntop(AF_INET,&cliaddr.sin_addr,(char *)&buffer,BUF_LEN));
			rxfifo.push_back(p);
		}
	}
}

void udps::send(void){
	if(txfifo.size()){
		pdu p = txfifo[0];
		int n = -1;
		n = sendto(sockfd,(const char *)&p,p.len,MSG_CONFIRM, (const struct sockaddr *) &cliaddr,sizeof(cliaddr)); 
		if(n == p.len){
#ifdef	DEBUG
			//printf("%s SEND->BYTES:%d TYPE:%d S:%d Q:%d K:%d\n", inet_ntop(AF_INET,&cliaddr.sin_addr,(char *)&buffer,BUF_LEN),n,(int)p.type,sm,txfifo.size(),beacon_try);
			unsigned char buffer[BUF_LEN];
			syslog(LOG_INFO,"%s SEND->BYTES:%d TYPE:%d S:%d Q:%d K:%d\n", inet_ntop(AF_INET,&cliaddr.sin_addr,(char *)&buffer,BUF_LEN),n,(int)p.type,sm,txfifo.size(),beacon_try);
#endif
			if(txfifo.size())txfifo.erase(txfifo.begin());
		}
	}
}
void udps::process(void){
	if(rxfifo.size()){
		pdu p = rxfifo[0];
		if(p.type == KAL){
			string msg((const char*)p.data,p.len-HEADER_LEN);
			if(!msg.compare(wifi_key)){
				if(sm != BKN){
					pdu p;
					p.type = KAL;
					p.len = HEADER_LEN+apap_key.length();
					memcpy(p.data,apap_key.c_str(),apap_key.length());
					txfifo.push_back(p);
					send();
				}
				sm = BKN;
				con = true;
				beacon_try = 0;
			}
		}
		rxfifo.clear();
	}
}
