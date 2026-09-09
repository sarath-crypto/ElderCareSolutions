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
#include <iostream>
#include <string>

#include "udps.h"

#define CON_TH		4
//#define	DEBUG		1

using namespace std;

udps::~udps(){
	close(sockfd);
}

udps::udps(string ip){
	state = true;
	if((sockfd = socket(AF_INET,SOCK_DGRAM, 0)) < 0 ){
        	state = false;
		return;
	}
	bzero(&seraddr,sizeof(seraddr));
	bzero(&cliaddr,sizeof(cliaddr));

	seraddr.sin_family  = AF_INET;
	seraddr.sin_addr.s_addr = INADDR_ANY;
	seraddr.sin_port = htons(SERVER_PORT);
	if(bind(sockfd, (const struct sockaddr *)&seraddr,sizeof(seraddr)) < 0 ){ 
        	state = false;
		return;
    	}	 
	con = 0;
	timeout.tv_sec = 0;
        timeout.tv_usec = 10;
        setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
	aip = ip;
	await = false;
}

void udps::receive(void){
	int n = -1;
	socklen_t len  = sizeof(cliaddr);
	char buffer[BUF_LEN];
	n = recvfrom(sockfd,(char *)buffer,BUF_LEN,MSG_WAITALL,(struct sockaddr *)&cliaddr,&len); 
	if(n > 0){
		string msg(buffer,n); 
#ifdef	DEBUG
		printf("RECV:%s\n",msg.c_str());
#endif
		rxfifo.push_back(msg);
	}
}

void udps::sender(void){
	if(txfifo.size()){
		cliaddr.sin_family = AF_INET;
                cliaddr.sin_port = htons(SERVER_PORT);
                inet_aton(aip.c_str(),&cliaddr.sin_addr);
		string msg = txfifo[0];
		int n = -1;
		n = sendto(sockfd,(const char *)msg.data(),msg.length(),MSG_CONFIRM, (const struct sockaddr *) &cliaddr,sizeof(cliaddr)); 
		if(n == (int)msg.length()){
			if(msg.find("ON") != std::string::npos)await = true;
			if(msg.find("OFF") != std::string::npos)await = true;
			else txfifo.erase(txfifo.begin());
#ifdef	DEBUG
			printf("SEND %s %d\n",msg.c_str(),await);
#endif

		}
	}
}

void udps::process(void){
	if(rxfifo.size()){
		string msg = rxfifo[0];
		size_t space_pos = msg.find(' ');
    		msg = msg.substr(0,space_pos);
		if(!msg.compare(key)){
			con = CON_TH;
			if(await){
				txfifo.clear();
				await = false;
			}
#ifdef	DEBUG
			printf("process() msg %s con %d %d\n",msg.c_str(),con,await);
#endif
		}	
		rxfifo.erase(rxfifo.begin());
	}
}
