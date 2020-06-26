#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/epoll.h>
#include <string.h>
#include <string>
#include "threadpool.h"
//#include "myhttp_coon.h"
using namespace std;
 int port = 8889;
#define EPOLL_SIZE 50

int setnonblocking(int fd){
	int old_option = fcntl(fd, F_GETFL);
	int new_option = old_option | O_NONBLOCK;
	fcntl(fd, F_SETFL, new_option);
	return old_option;
}

void addfd(int epfd, int fd, bool flag){
	epoll_event ev;
	ev.data.fd = fd;
	ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;//in,et,rdhup
	if(flag){ //flag为true说明是服务器端 可以加上oneshot
		ev.events = ev.events | EPOLLONESHOT;
	}
	epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
	setnonblocking(fd);

}

int main(int argc, char *argv[]){
    if (argc == 2) port = atoi(argv[1]);
	threadpool<http_coon> * pool = NULL;
	pool = new threadpool<http_coon>;
	http_coon * users = new http_coon[100];
	assert(users);
	
	struct sockaddr_in address;
	bzero(&address, sizeof(address));

	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	address.sin_addr.s_addr = htonl(INADDR_ANY);

	int listenfd = socket(AF_INET, SOCK_STREAM, 0);

	int ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
	assert(ret != -1);

	ret = listen(listenfd, 5);
	assert(ret == 0);
	
	int epfd;
	struct epoll_event *events;
	events = (epoll_event *)malloc(sizeof( epoll_event)*50);
	epfd = epoll_create(EPOLL_SIZE);
	assert(epfd !=0 );
	
	addfd(epfd, listenfd, false); //服务端你懂的
	puts("success");	
	while(true){
		puts("start");
		int number = epoll_wait(epfd, events, 1000, -1);
		puts("end");
		cout << number << endl;
		if(number<0 && (errno != EINTR)){
			puts("my epoll is failure!/n");
			break;
		}
		for(int i = 0; i < number; ++i){
			int sockfd = events[i].data.fd;
			if(sockfd == listenfd){ //新用户连接
				struct sockaddr_in client_address;
				socklen_t client_addresslength = sizeof(client_address);
				int client_fd = accept(listenfd, (struct sockaddr*)&client_address, &client_addresslength);
				if(client_fd < 0){
					printf("errno is %d",errno);
					continue;
				}

				/*if(http_coon::m_User_count > MAX_FD){
					show_error(client_fd, "Internal sever busy");
					continue;
				}*/
				//初始化客户连接
				cout << epfd << " " << "the fd of new client is "<<client_fd <<endl;
				addfd(epfd, client_fd, true);
				cout << "client_fd:" << client_fd<< "****/n" <<endl;
				users[client_fd].init(epfd, client_fd); 			
			}
			else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)){//出现异常关闭客户端连接
				users[sockfd].close_coon();
			}
			else if(events[i].events & EPOLLIN){ // in 读取
				if(users[sockfd].myread()){
					pool ->addjob(users+sockfd);
				}
				else {
					users[sockfd].close_coon();
				}
			}
			else if(events[i].events & EPOLLOUT){// out 可写入
				if(!users[sockfd].mywrite()){
					users[sockfd].close_coon();
				}
			}
		}
	}
	close(epfd);
	close(listenfd);
	delete[] users;
	delete pool;
	return 0;

}

