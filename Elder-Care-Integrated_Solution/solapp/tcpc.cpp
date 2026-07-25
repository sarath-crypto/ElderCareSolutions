#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <syslog.h>
#include <cerrno>
#include "tcpc.h"

//#define DEBUG	1

bool tcpc::setNonBlocking(int fd) {
	int flags = fcntl(fd, F_GETFL, 0);
	if(flags == -1) return false;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

tcpc::tcpc(string ip,unsigned short port){
	state = true;
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock < 0){
#ifdef DEBUG
		std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
#endif
		state = false;
		return;
	}
	if(!setNonBlocking(sock)){
#ifdef DEBUG
		std::cerr << "Failed to set non-blocking flag" << std::endl;
#endif
		close(sock);
		state = false;
		return;
	}
	std::memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	inet_pton(AF_INET,ip.c_str(), &server_addr.sin_addr);
	con = false;
}

tcpc::~tcpc(){
	close(sock);
}

void tcpc::receive(void){
	if(!con){
		int res = connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
		if(res < 0){
			if(errno != EINPROGRESS) {
#ifdef DEBUG
				std::cerr << "Immediate connection failure: " << strerror(errno) << std::endl;
#endif
				close(sock);
				return;
			}
			fd_set write_fds, err_fds;
			FD_ZERO(&write_fds);
			FD_ZERO(&err_fds);
			FD_SET(sock, &write_fds);
			FD_SET(sock, &err_fds);

			struct timeval timeout;
			timeout.tv_sec = 5; 
			timeout.tv_usec = 0;

			res = select(sock + 1, nullptr, &write_fds, &err_fds, &timeout);
			if (res <= 0) {
				if (res == 0)syslog(LOG_INFO,"tcpc connection attempt timed out");
				else syslog(LOG_INFO,"tcpc select function error: %s ",strerror(errno));
				close(sock);
				return;
			}

			if (FD_ISSET(sock, &err_fds)) {
				int so_error = 0;
				socklen_t len = sizeof(so_error);
				getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len);
#ifdef DEBUG
				std::cerr << "Asynchronous connection error: " << strerror(so_error) << std::endl;
#endif
				close(sock);
				return;
			}
		}	
#ifdef DEBUG
		std::cout << "Successfully connected to the server!" << std::endl;
#endif
		con = true;
	}else{
		char read_buffer[1024];
		std::memset(read_buffer, 0, sizeof(read_buffer));
		ssize_t br = recv(sock, read_buffer, sizeof(read_buffer) - 1, 0);
		if(br > 0){
#ifdef DEBUG
			cout << "Received from server: " << br << endl;
#endif
			for(int i = 0;i< br;i++)rxq.push_back(read_buffer[i]);	
		}else if(br == 0){
#ifdef DEBUG
			cout << "Remote server closed connection cleanly." << endl;
#endif
			con = false;
		}else{
			if (errno != EAGAIN && errno != EWOULDBLOCK) {
#ifdef DEBUG
				cout << "Read error occurred: " << strerror(errno) << endl;
#endif
			}
		}
	}
}

void tcpc::sender(vector<unsigned char> & vb){
	ssize_t bytes_sent = send(sock,vb.data(),vb.size(),0);
	if(bytes_sent < 0) {
		if (errno != EAGAIN && errno != EWOULDBLOCK) {
#ifdef DEBUG
			std::cerr << "Send failed: " << strerror(errno) << std::endl;
#endif
			return;
		}
	}
}
