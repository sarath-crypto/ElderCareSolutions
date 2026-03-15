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
#include "global.h"
#include "udps.h"

#define BUFSIZE		512
#define ALARM_PDU_SZ	11
#define CON_TH		4
//#define	DEBUG		1

using namespace std;

extern ipc *pipc;

udps::~udps(){
	close(sockfd);
}

udps::udps(string a_ip,string s_ip){
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
	con_a = 0;
	con_s = 0;
	timeout.tv_sec = 0;
        timeout.tv_usec = 10;
        setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
	aip = a_ip;
	sip = s_ip;
}

void udps::receive(void){
	int n = -1;
	socklen_t len  = sizeof(cliaddr);
	unsigned char buffer[BUF_LEN];
	n = recvfrom(sockfd,(char *)buffer,BUF_LEN,MSG_WAITALL,(struct sockaddr *)&cliaddr,&len); 
	if(n > 0){
#ifdef	DEBUG
		printf("RECV:%d\n",n);
#endif
		if(n == ALARM_PDU_SZ){
			pdu p;
			memcpy((void *)&p,buffer,n);
			if(n == p.len){
#ifdef	DEBUG
				printf("PDU %s RECV->BYTES:%d TYPE:%d  Q:%lu \n",inet_ntop(AF_INET,&cliaddr.sin_addr,(char *)&buffer,BUF_LEN),n,(int)p.type,rxfifo_a.size());
				syslog(LOG_INFO,"%s RECV->BYTES:%d TYPE:%d  Q:%lu \n",inet_ntop(AF_INET,&cliaddr.sin_addr,(char *)&buffer,BUF_LEN),n,(int)p.type,rxfifo_a.size());
#endif
				rxfifo_a.push_back(p);
			}
		}else{
			spdu sp;
			memcpy((void *)&sp.data,buffer,n);
			sp.len = n;
#ifdef	DEBUG
			printf("SPDU %s RECV->BYTES:%d\n",inet_ntop(AF_INET,&cliaddr.sin_addr,(char *)&buffer,BUF_LEN),n);
			syslog(LOG_INFO,"%s RECV->BYTES:%d",inet_ntop(AF_INET,&cliaddr.sin_addr,(char *)&buffer,BUF_LEN),n);
#endif
			rxfifo_s.push_back(sp);
		}
	}
}

void udps::sender(void){
	if(txfifo_a.size()){
		cliaddr.sin_family = AF_INET;
                cliaddr.sin_port = htons(SERVER_PORT);
                inet_aton(aip.c_str(),&cliaddr.sin_addr);
		pdu p = txfifo_a[0];
		int n = -1;
		n = sendto(sockfd,(const char *)&p,p.len,MSG_CONFIRM, (const struct sockaddr *) &cliaddr,sizeof(cliaddr)); 
		if(n == p.len){
#ifdef	DEBUG
			unsigned char buffer[BUF_LEN];
			printf("%s SEND->BYTES:%d TYPE:%d  Q:%lu\n", inet_ntop(AF_INET,&cliaddr.sin_addr,(char *)&buffer,BUF_LEN),n,(int)p.type,txfifo_a.size());
			syslog(LOG_INFO,"%s SEND->BYTES:%d TYPE:%d  Q:%lu\n", inet_ntop(AF_INET,&cliaddr.sin_addr,(char *)&buffer,BUF_LEN),n,(int)p.type,txfifo_a.size());
#endif
			if(txfifo_a.size())txfifo_a.erase(txfifo_a.begin());
		}
	}
	if(txfifo_s.size()){
		cliaddr.sin_family = AF_INET;
                cliaddr.sin_port = htons(SERVER_PORT);
                inet_aton(sip.c_str(),&cliaddr.sin_addr);
		spdu sp = txfifo_s[0];
		int n = -1;
		n = sendto(sockfd,(const char *)&sp,sp.len,MSG_CONFIRM, (const struct sockaddr *) &cliaddr,sizeof(cliaddr)); 
		if(n == sp.len){
#ifdef	DEBUG
			unsigned char buffer[BUF_LEN];
			printf("%s SEND->BYTES:%d\n", inet_ntop(AF_INET,&cliaddr.sin_addr,(char *)&buffer,BUF_LEN),n);
			syslog(LOG_INFO,"%s SEND->BYTES:%d", inet_ntop(AF_INET,&cliaddr.sin_addr,(char *)&buffer,BUF_LEN),n);
#endif
			if(txfifo_s.size())txfifo_s.erase(txfifo_s.begin());
		}
	}
}

void udps::process(void){
	if(rxfifo_a.size()){
		pdu p = rxfifo_a[0];
		if(p.type == KAL){
			string msg((const char*)p.data,p.len-HEADER_LEN);
			if(!msg.compare(key)){
				con_a = CON_TH;
#ifdef	DEBUG
				printf("udp process() msg %s con %d\n",msg.c_str(),con_a);
				syslog(LOG_INFO,"udp process() msg %s con %d\n",msg.c_str(),con_a);
#endif
			}	
		}
		rxfifo_a.erase(rxfifo_a.begin());
	}
	if(rxfifo_s.size()){
		spdu sp = rxfifo_s[0];
		string msg((const char*)sp.data,sp.len);

		vector <string> w;
        	stringstream ss(msg.c_str());
        	string sw;
        	while(getline(ss,sw,' '))w.push_back(sw);
		if(!w[0].compare(key)){
			con_s = CON_TH;
			pipc->uload = stoi(w[1]);
			pipc->whi = stoi(w[2]);
			pipc->wl = stoi(w[3]);
			pipc->bl = stoi(w[4]);
			pipc->sl = stoi(w[5]);
			if(stoi(w[6]) > 2200)pipc->grid = true;
#ifdef	DEBUG
			printf("udps process() msg %s con %d\n",msg.c_str(),con_s);
			syslog(LOG_INFO,"udps process() msg %s con %d\n",msg.c_str(),con_s);
#endif
		}	
		rxfifo_s.erase(rxfifo_s.begin());
	}

}
