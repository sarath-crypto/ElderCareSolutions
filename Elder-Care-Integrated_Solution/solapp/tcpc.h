#ifndef _TCPC_H
#define _TCPC_H

#include <iostream>
#include <vector>
#include <boost/asio.hpp>
#include <mutex>

#define TCP_PDU_MTU	1024

using namespace std;
using boost::asio::ip::tcp;

typedef struct hybrid{
	unsigned char len;
	unsigned char data[TCP_PDU_MTU];
}hybrid;

class AsyncTcpClient {
private:
	void do_read() {
		socket_.async_read_some(boost::asio::buffer(data_, TCP_PDU_MTU),[this](boost::system::error_code ec, std::size_t length) {
		if(!ec){
			hybrid hdata;
			hdata.len = length;
			memcpy(hdata.data,data_,length);
			mx_hq.lock();
			hq.push_back(hdata);
			mx_hq.unlock();
			do_read(); 
			}
		});
	}
	tcp::socket socket_;
	char data_[TCP_PDU_MTU];
public:
	mutex mx_hq;
	vector <hybrid>hq;
	AsyncTcpClient(boost::asio::io_context& io_context, const std::string& host, const std::string& port) : socket_(io_context){
	tcp::resolver resolver(io_context);
	auto endpoints = resolver.resolve(host, port);
	
	boost::asio::async_connect(socket_, endpoints,[this](boost::system::error_code ec, tcp::endpoint){
		if(!ec){
			do_read();
			}
		});
	}

	void send(vector<unsigned char> &bytes){
		boost::asio::async_write(socket_, boost::asio::buffer(bytes.data(),bytes.size()),[](boost::system::error_code ec,std::size_t bytes_transferred){
			if (ec) std::cerr << "Write error: " << ec.message() << std::endl;
		});
	}
};
#endif
