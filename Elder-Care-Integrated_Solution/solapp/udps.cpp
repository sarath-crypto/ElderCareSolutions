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
#include <stdlib.h>
#include <string.h>
#include <linux/rtnetlink.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <bits/stdc++.h>
#include "udps.h"

#define CON_TH		4
//#define	DEBUG		1

using namespace std;

udps::~udps(){
	close(sockfd);
}

udps::udps(string ip,unsigned short port){
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
	con = 0;
	timeout.tv_sec = 0;
        timeout.tv_usec = 10;
        setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
	uip  = ip;
	uport = port;	
}

void udps::receive(void){
	int n = -1;
	socklen_t len  = sizeof(cliaddr);
	char buffer[BUF_LEN];
	n = recvfrom(sockfd,(char *)buffer,BUF_LEN,MSG_WAITALL,(struct sockaddr *)&cliaddr,&len); 
	if(n > 0){
		string msg(buffer,n);
		rxfifo.push_back(msg);
#ifdef	DEBUG
		printf("UDP  %s RECV->BYTES:%d Q:%lu %s\n",inet_ntop(AF_INET,&cliaddr.sin_addr,(char *)&buffer,BUF_LEN),n,rxfifo.size(),msg.c_str());
#endif
	}
}

void udps::sender(void){
	if(txfifo.size()){
		string msg = txfifo[0];
		txfifo.erase(txfifo.begin());
		cliaddr.sin_family = AF_INET;
                cliaddr.sin_port = htons(uport);
                inet_aton(uip.c_str(),&cliaddr.sin_addr);
		long unsigned int n = -1;
		n = sendto(sockfd,msg.c_str(),msg.length(),MSG_CONFIRM, (const struct sockaddr *) &cliaddr,sizeof(cliaddr)); 
		if(n == msg.length()){
#ifdef	DEBUG
			unsigned char buffer[BUF_LEN];
			printf("UDP %s SEND->BYTES:%lu Q:%lu %s\n", inet_ntop(AF_INET,&cliaddr.sin_addr,(char *)&buffer,BUF_LEN),n,txfifo.size(),msg.c_str());
#endif
		}
	}
}

void udps::process(void){
	if(rxfifo.size()){
		string msg = rxfifo[0];
		if(!msg.compare(key)){
			con = CON_TH;
#ifdef	DEBUG
			printf("udp process() msg %s con %d\n",msg.c_str(),con);
#endif
		}
		rxfifo.erase(rxfifo.begin());
	}
}
