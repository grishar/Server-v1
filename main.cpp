#include <iostream>
#include <thread>
#include <string>
#include <sstream>
#include <fstream>
#include <algorithm>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdio>
#include <cstdlib>

using namespace std;

const int MAX_EVENTS = 10;


int setNonBlock(int fd){
	int flags;
#if defined(O_NONBLOCK)
	if((flags = fcntl(fd, F_GETFL, 0)) == -1)
		flags =0;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);

#else
	flags = 1;
	return ioctl(fd, FIOBIO, &flags);

#endif
}

string HttpRespons(char* buffer, size_t bufferLength, string rootDirectory){
	stringstream ss(buffer);
	string str[3]; //GET URL VERSION
	
	for(int i = 0; i < 3; ++i){
		ss >> str[i];
	}
	
	string filePath = rootDirectory + str[1];
	
	replace(filePath.begin(), filePath.end(), '?', '\0');
	
	ifstream file(filePath);
	
	string respons;
	
	if(file){
		respons = str[2] + " 200 OK\r\n\r\n";
// 		+ "Content-Type: text/html\r\n\r\n";
		
		char ch;
		
		string document;
		
		for(;;){
			file.get(ch);
			
			if(file.eof())
				break;
			
			document+=ch;
		}
		respons += document;
		
// 		respons += "Content-length: " + to_string(document.size()) + "\r\n" +
// 		"Connection: close\r\nContent-Type: text/html\r\n\r\n" + document;
		
		file.close();
		
	}
	else{
		respons = str[2] + " 404 NOT FOUND\r\n" + "Content-Type: text/html\r\n\r\n";
// 		+ "<html><header><title>Not Found</title></header><body>File Not Found</body></html>"
;
	}
	
	return respons;
}

void Loging(string fileName, string info){
	ofstream file(fileName, ios::app);
	file << info << endl;
	file.close();
	
}

void ThreadProcessing(int &epoll, string rootDir){
	
	for(;;){
	
		epoll_event events[MAX_EVENTS];

		int eventCount = epoll_wait(epoll, events, MAX_EVENTS, 50);

		for(int i = 0; i < eventCount; i++){
			
			char msgBuffer[1024];

			int recvResult = recv(events[i].data.fd, msgBuffer, 1024, MSG_NOSIGNAL);

			if(recvResult == 0 && errno != EAGAIN){
				shutdown(events[i].data.fd, SHUT_RDWR);
				epoll_ctl(epoll, EPOLL_CTL_DEL, events[i].data.fd, &events[i]);
			}
			else if (recvResult > 0){
				string request = msgBuffer;
				
				Loging("/tmp/log", request);
				
				string respons = HttpRespons(msgBuffer, recvResult, rootDir);
				
				Loging("/tmp/log", respons);
				
				send(events[i].data.fd, respons.c_str(), respons.size(), MSG_NOSIGNAL);
				
				shutdown(events[i].data.fd, SHUT_RDWR);
				epoll_ctl(epoll, EPOLL_CTL_DEL, events[i].data.fd, &events[i]);
			}
			
		}
	}
}


using namespace std;

int main(int argc, char *argv[])
{
	if(!fork()){
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
		
		int opt;
		
		string ipAddr;
		string port;
		string rootDir;
		
		while( (opt = getopt(argc, argv, "h:p:d:")) != -1 ){
			
			switch(opt){
				case 'h' :
					ipAddr = optarg;
					break;
				case 'p' :
					port = optarg;
					break;
				case 'd' :
					rootDir = optarg;
					break;
			}
			
		}
		
		// Connect Server
		int masterSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		
		int optval = 1;
		setsockopt(masterSocket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &optval, sizeof(optval));

		sockaddr_in sockAddr;
		sockAddr.sin_family = AF_INET;
		sockAddr.sin_port = htons(stoi(port));
		sockAddr.sin_addr.s_addr = inet_addr(ipAddr.c_str());
	// 	htonl(INADDR_ANY);

		bind(masterSocket, (sockaddr*)&sockAddr, sizeof(sockAddr));

		setNonBlock(masterSocket);

		listen(masterSocket, SOMAXCONN);
		
		timeval waitTime = {0, 10};
		
		int epoll  = epoll_create1(0);
		
		//Respons Server
		thread cliProcThr(ThreadProcessing, ref(epoll), rootDir);
		cliProcThr.detach();
		
		for(;;){
			
			fd_set set;
			FD_ZERO(&set);
			FD_SET(masterSocket, &set);

			select(masterSocket + 1, &set, NULL, NULL, &waitTime);

			if(FD_ISSET(masterSocket, &set)){
				
				int slaveSocket = accept(masterSocket, 0, 0);
				setNonBlock(slaveSocket);
				
				epoll_event event;
				event.data.fd = slaveSocket;
				event.events = EPOLLIN;
				epoll_ctl(epoll, EPOLL_CTL_ADD, slaveSocket, &event);
			}
		}
	}
	else{
		exit(0);
	}	


	return 0;
}
