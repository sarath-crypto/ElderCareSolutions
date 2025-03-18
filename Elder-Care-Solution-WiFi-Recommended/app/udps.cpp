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

#include "udps.h"

#define BUFSIZE	8192

//#define	DEBUG		1

using namespace std;

ssize_t	udps::read_netlink_socket (int sock_fd, char *buffer, size_t buffer_sz,uint32_t seq, uint32_t pid){
	struct nlmsghdr *nl_hdr;
	ssize_t read_len = 0, msg_len = 0;
	do {
		if ((size_t) msg_len > buffer_sz) {
			perror ("No size in buffer");
			return -1;
		}
		read_len = recv (sock_fd, buffer, buffer_sz - msg_len, 0);
		if (read_len < 0) {
			perror ("SOCK READ: ");
			return -1;
		}
		nl_hdr = (struct nlmsghdr *)buffer;
		if (!NLMSG_OK (nl_hdr, read_len)
				|| (nl_hdr->nlmsg_type == NLMSG_ERROR)) {
			perror ("Error in recieved packet");
			return -1;
		}
		if (nl_hdr->nlmsg_type == NLMSG_DONE)
			break;
		else {
			buffer += read_len;
			msg_len += read_len;
		}
		if (!(nl_hdr->nlmsg_flags & NLM_F_MULTI))
			break;

	} while ((nl_hdr->nlmsg_seq != seq) || (nl_hdr->nlmsg_pid != pid));
	return msg_len;
}

int udps::parse_routes (struct nlmsghdr *nl_hdr, struct route_info *rt_info){
	struct rtmsg *rt_msg;
	struct rtattr *rt_attr;
	int rt_len;
	rt_msg = (struct rtmsg *)NLMSG_DATA (nl_hdr);
	if ((rt_msg->rtm_family != AF_INET) || (rt_msg->rtm_table != RT_TABLE_MAIN))return -1;
	rt_attr = (struct rtattr *)RTM_RTA (rt_msg);
	rt_len = RTM_PAYLOAD (nl_hdr);
	for (; RTA_OK (rt_attr,rt_len); rt_attr = RTA_NEXT (rt_attr,rt_len)) {
		switch (rt_attr->rta_type) {
			case RTA_OIF:
				if_indextoname (*(int *)RTA_DATA (rt_attr), rt_info->if_name);
				break;
			case RTA_GATEWAY:
				memcpy (&rt_info->gw_addr, RTA_DATA(rt_attr),sizeof (rt_info->gw_addr));
				break;
			case RTA_PREFSRC:
				memcpy (&rt_info->src_addr, RTA_DATA(rt_attr),sizeof (rt_info->src_addr));
				break;
			case RTA_DST:
				memcpy (&rt_info->dst_addr, RTA_DATA(rt_attr),sizeof (rt_info->dst_addr));
				break;
		}
	}
	return 0;
}

void udps::get_gatewayip(void){
	struct nlmsghdr *nl_msg;
	struct route_info route_info;
	char msg_buffer[BUFSIZE]; 
	int sock, len, msg_seq = 0;

	sock = socket (PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
	if (sock < 0) {
		perror("Socket Creation: ");
		return;
	}
	memset (msg_buffer, 0, sizeof (msg_buffer));
	nl_msg = (struct nlmsghdr *)msg_buffer;
	nl_msg->nlmsg_len = NLMSG_LENGTH (sizeof (struct rtmsg)); 
	nl_msg->nlmsg_type = RTM_GETROUTE; 
	nl_msg->nlmsg_flags = NLM_F_DUMP | NLM_F_REQUEST; 
	nl_msg->nlmsg_seq = msg_seq++;
	nl_msg->nlmsg_pid = getpid(); 

	if (send (sock, nl_msg, nl_msg->nlmsg_len, 0) < 0) {
		syslog(LOG_INFO,"Write To Socket Failed...");
		return;
	}

	len = read_netlink_socket (sock, msg_buffer, sizeof (msg_buffer),msg_seq, getpid ());
	if (len < 0) {
		syslog(LOG_INFO,"Read From Socket Failed...");
		return;
	}
	close(sock);
	char gatewayip[24];
	for (; NLMSG_OK (nl_msg, len); nl_msg = NLMSG_NEXT (nl_msg, len)) {
		memset (&route_info, 0, sizeof(route_info));
		if (parse_routes (nl_msg, &route_info) < 0)continue;  
		if (strstr ((char *)inet_ntoa (route_info.dst_addr), "0.0.0.0")) {
			inet_ntop (AF_INET, &route_info.gw_addr, gatewayip, sizeof(gatewayip));
			break;
		}
	}
	gwip = string(gatewayip);
}

udps::~udps(){
	close(sockfd);
}

udps::udps(){
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
	con = false;
	sm = STOP;
	beacon_try = 0;
	timeout.tv_sec = 0;
        timeout.tv_usec = 10;
        setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
	get_gatewayip();
}

void udps::receive(void){
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
			rxfifo.push_back(p);
		}
	}
}

void udps::sender(void){
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
			if(!msg.compare(key)){
				if(sm != BKN){
					pdu p;
					p.type = KAL;
					p.len = HEADER_LEN+key.length();
					memcpy(p.data,key.c_str(),key.length());
					txfifo.push_back(p);
					sender();
				}
				sm = BKN;
				con = true;
				beacon_try = 0;
			}
		}
		rxfifo.clear();
	}
}
