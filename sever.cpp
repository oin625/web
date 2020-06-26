#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <assert.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
using namespace std;
const int port = 8912;
int main(int argc, char *argv[])
{
	sockaddr_in sever_address;
	bzero(&sever_address, sizeof(sever_address));
	sever_address.sin_family = PF_INET;
	sever_address.sin_addr.s_addr = htonl(INADDR_ANY);
	sever_address.sin_port = htons(port);
	
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	assert(sock>=0);
	int option = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,(void*)&option, sizeof(option));
	int ret = bind(sock, (struct sockaddr*)&sever_address, sizeof(sever_address));

	assert(ret!=-1);

	ret = listen(sock, 5);
	while(1){
		sockaddr_in client;
		socklen_t client_addrlength = sizeof(client);
		int connfd = accept(sock, (struct sockaddr*)&client, &client_addrlength);
		if(connfd < 0) {
			puts("accept error!");
		}
		else {
			char request[1024];
			read(connfd, request, 1024);
			request[strlen(request)+1] = '\0';
			puts("test");
			printf("request: %s\n", request);
			puts("test666");
			puts("successful!");
			char buf[520] = "HTTP/1.1 200 OK\r\nconnection: close\r\n\r\n";
			write(connfd, buf, strlen(buf));
			int message = open("hello.html", O_RDONLY);
			sendfile(connfd, message, NULL, 2500);
			close(message);
			close(connfd);

		}
	}
	return 0;
}
